#include "Server.h"
#include "Dijkstra.h"
#include "DefaultTopology.h"
#include <nlohmann/json.hpp>
#include <chrono>
#include <iostream>
#include <stdexcept>

using json  = nlohmann::json;
using Clock = std::chrono::steady_clock;   // monotonic — correct for elapsed time

namespace {
constexpr int kMaxRouterId = 100000;       // SPF arrays are sized by max ID

void sendError(httplib::Response& res, int status, const std::string& msg) {
    res.status = status;
    res.set_content(json{{"error", msg}}.dump(), "application/json");
}

// Parses a router ID from a query/path string. Throws on non-numeric or
// out-of-range input so handlers can turn it into a 400 instead of
// letting std::invalid_argument escape and kill the request thread.
int parseRouterId(const std::string& s) {
    size_t pos = 0;
    int id = std::stoi(s, &pos);
    if (pos != s.size() || id <= 0 || id > kMaxRouterId)
        throw std::invalid_argument("router id out of range");
    return id;
}

long long elapsedUs(Clock::time_point t0) {
    return std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - t0).count();
}
} // namespace

Server::Server(LSDB& lsdb) : lsdb_(lsdb) {
    registerRoutes();
}

void Server::start(int port) {
    std::cout << "Server listening on http://localhost:" << port << "\n";
    svr_.listen("0.0.0.0", port);
}

void Server::registerRoutes() {
    svr_.Get ("/topology",     [this](const auto& q, auto& r) { handleGetTopology(q, r); });
    svr_.Post("/topology",     [this](const auto& q, auto& r) { handlePostTopology(q, r); });
    svr_.Get ("/route",        [this](const auto& q, auto& r) { handleGetRoute(q, r); });
    svr_.Get (R"(/table/(\d+))",[this](const auto& q, auto& r) { handleGetTable(q, r); });
    svr_.Post("/fail",         [this](const auto& q, auto& r) { handlePostFail(q, r); });
    svr_.Post("/restore",      [this](const auto& q, auto& r) { handlePostRestore(q, r); });
    svr_.Post("/reset",        [this](const auto& q, auto& r) { handlePostReset(q, r); });

    // CORS: the React frontend runs on a different port.
    svr_.set_default_headers({
        {"Access-Control-Allow-Origin",  "*"},
        {"Access-Control-Allow-Methods", "GET, POST, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type"}
    });
    svr_.Options(R"(.*)", [](const httplib::Request&, httplib::Response& res) {
        res.status = 204;
    });
}

// GET /topology — current graph as {nodes, edges}, read straight from the LSDB.
void Server::handleGetTopology(const httplib::Request&, httplib::Response& res) {
    std::shared_lock lock(graphMu_);

    json nodes = json::array(), edges = json::array();
    for (const auto& [id, lsa] : lsdb_.all()) {
        nodes.push_back(id);
        for (const auto& [nbr, cost] : lsa.neighbours) {
            // Each link appears in both endpoints' LSAs — report it once.
            if (id < nbr) edges.push_back({{"u", id}, {"v", nbr}, {"cost", cost}});
        }
    }
    res.set_content(json{{"nodes", nodes}, {"edges", edges}}.dump(), "application/json");
}

// POST /topology — replaces the whole topology with {nodes, edges}.
void Server::handlePostTopology(const httplib::Request& req, httplib::Response& res) {
    json body;
    try {
        body = json::parse(req.body);
    } catch (const std::exception& e) {
        return sendError(res, 400, std::string("invalid JSON: ") + e.what());
    }

    // Validate fully before touching shared state.
    std::vector<int> nodes;
    std::vector<std::tuple<int, int, int>> edges;
    try {
        for (const auto& n : body.at("nodes")) {
            int id = n.get<int>();
            if (id <= 0 || id > kMaxRouterId) throw std::invalid_argument("node id out of range");
            nodes.push_back(id);
        }
        for (const auto& e : body.at("edges")) {
            int u = e.at("u").get<int>(), v = e.at("v").get<int>(), c = e.at("cost").get<int>();
            if (c <= 0) throw std::invalid_argument("edge cost must be positive");
            edges.emplace_back(u, v, c);
        }
    } catch (const std::exception& e) {
        return sendError(res, 400, e.what());
    }

    std::unique_lock lock(graphMu_);
    // Clear first — same reason as /reset: update() would reject the new
    // LSAs as stale if old ones with higher sequence numbers remained.
    lsdb_.clear();
    for (int id : nodes) lsdb_.update(LSA{id, {}, 1});
    for (const auto& [u, v, c] : edges) {
        if (!lsdb_.setLink(u, v, c)) {
            lsdb_.clear();
            loadDefaultTopology(lsdb_);
            return sendError(res, 400, "edge references an undeclared node; default topology restored");
        }
    }
    res.set_content(json{{"status", "ok"}, {"routers", lsdb_.routerCount()}}.dump(),
                    "application/json");
}

// GET /route?src=X&dst=Y — shortest path and cost.
void Server::handleGetRoute(const httplib::Request& req, httplib::Response& res) {
    if (!req.has_param("src") || !req.has_param("dst"))
        return sendError(res, 400, "src and dst query params required");

    int src, dst;
    try {
        src = parseRouterId(req.get_param_value("src"));
        dst = parseRouterId(req.get_param_value("dst"));
    } catch (const std::exception&) {
        return sendError(res, 400, "src and dst must be positive integer router ids");
    }

    std::shared_lock lock(graphMu_);
    SPT t = dijkstra(lsdb_, src);          // SPF directly on the LSDB

    if (dst >= static_cast<int>(t.dist.size()) || t.dist[dst] == UNREACHABLE)
        return sendError(res, 404, "no path found");

    json path = reconstructPath(t.parent, src, dst);
    res.set_content(json{{"path", path}, {"cost", t.dist[dst]}}.dump(), "application/json");
}

// GET /table/:id — full routing table for one router.
void Server::handleGetTable(const httplib::Request& req, httplib::Response& res) {
    int routerID;
    try {
        routerID = parseRouterId(req.matches[1]);
    } catch (const std::exception&) {
        return sendError(res, 400, "invalid router id");
    }

    std::shared_lock lock(graphMu_);
    SPT t = dijkstra(lsdb_, routerID);

    json table = json::array();
    for (int dest = 0; dest < static_cast<int>(t.dist.size()); ++dest) {
        if (dest == routerID || t.dist[dest] == UNREACHABLE) continue;
        auto path = reconstructPath(t.parent, routerID, dest);
        int nextHop = path.size() > 1 ? path[1] : dest;
        table.push_back({{"dest", dest}, {"nextHop", nextHop}, {"cost", t.dist[dest]}});
    }
    res.set_content(table.dump(), "application/json");
}

// POST /fail {u, v} — removes a link and reports reconvergence time
// (LSA update + SPF from the lowest-ID router).
void Server::handlePostFail(const httplib::Request& req, httplib::Response& res) {
    int u, v;
    try {
        json body = json::parse(req.body);
        u = body.at("u").get<int>();
        v = body.at("v").get<int>();
    } catch (const std::exception& e) {
        return sendError(res, 400, e.what());
    }

    std::unique_lock lock(graphMu_);
    auto t0 = Clock::now();
    if (!lsdb_.removeLink(u, v))
        return sendError(res, 404, "no such link");
    SPT t = dijkstra(lsdb_, lsdb_.all().begin()->first);
    long long us = elapsedUs(t0);

    int reachable = 0;
    for (int d : t.dist) reachable += (d != UNREACHABLE);

    res.set_content(json{{"reconverge_us", us},
                         {"reachable_from_root", reachable},
                         {"failed_link", {{"u", u}, {"v", v}}}}.dump(), "application/json");
}

// POST /restore {u, v, cost?} — adds a link, or updates its cost.
// Idempotent: restoring a link that already has that cost changes nothing.
void Server::handlePostRestore(const httplib::Request& req, httplib::Response& res) {
    int u, v, cost;
    try {
        json body = json::parse(req.body);
        u    = body.at("u").get<int>();
        v    = body.at("v").get<int>();
        cost = body.value("cost", 10);
        if (cost <= 0) throw std::invalid_argument("cost must be positive");
    } catch (const std::exception& e) {
        return sendError(res, 400, e.what());
    }
    if (u == v) return sendError(res, 400, "cannot link a router to itself");

    std::unique_lock lock(graphMu_);
    if (!lsdb_.all().count(u) || !lsdb_.all().count(v))
        return sendError(res, 404, "unknown router");

    auto t0 = Clock::now();
    bool changed = lsdb_.setLink(u, v, cost);
    long long us = 0;
    int reachable = 0;
    if (changed) {
        SPT t = dijkstra(lsdb_, lsdb_.all().begin()->first);
        us = elapsedUs(t0);
        for (int d : t.dist) reachable += (d != UNREACHABLE);
    }

    json out = {{"reconverge_us", us},
                {"changed", changed},
                {"restored_link", {{"u", u}, {"v", v}, {"cost", cost}}}};
    if (changed) out["reachable_from_root"] = reachable;   // SPF only ran if something changed
    res.set_content(out.dump(), "application/json");
}

// POST /reset — reloads the default 20-router topology.
void Server::handlePostReset(const httplib::Request&, httplib::Response& res) {
    std::unique_lock lock(graphMu_);
    loadDefaultTopology(lsdb_);            // clears first internally
    res.set_content(json{{"status", "reset"}, {"routers", lsdb_.routerCount()}}.dump(),
                    "application/json");
}
