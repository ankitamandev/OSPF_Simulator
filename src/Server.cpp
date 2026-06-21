#include "Server.h"
#include "Dijkstra.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <set>

using json = nlohmann::json;

Server::Server(LSDB& lsdb) : lsdb_(lsdb) {
    registerRoutes();
}

void Server::start(int port) {
    std::cout << "Server listening on http://localhost:" << port << "\n";
    svr_.listen("0.0.0.0", port);
}

void Server::registerRoutes() {
    svr_.Get("/topology", [this](const httplib::Request& req, httplib::Response& res) {
        handleGetTopology(req, res);
    });

    svr_.Post("/topology", [this](const httplib::Request& req, httplib::Response& res) {
        handlePostTopology(req, res);
    });

    svr_.Get("/route", [this](const httplib::Request& req, httplib::Response& res) {
        handleGetRoute(req, res);
    });

    svr_.Post("/fail", [this](const httplib::Request& req, httplib::Response& res) {
        handlePostFail(req, res);
    });

    svr_.Get(R"(/table/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        handleGetTable(req, res);
    });

    // Basic CORS so the React frontend (different port) can call this API.
    svr_.set_default_headers({
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "GET, POST, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type"}
    });
    svr_.Options(R"(.*)", [](const httplib::Request&, httplib::Response& res) {
        res.status = 204;
    });
}

// GET /topology — returns the current graph as {nodes, edges}.
// READ: shared_lock allows this to run concurrently with other reads.
void Server::handleGetTopology(const httplib::Request&, httplib::Response& res) {
    std::shared_lock lock(graphMu_);

    Graph g = lsdb_.buildGraph();
    json nodes = json::array();
    json edges = json::array();
    std::set<std::pair<int,int>> seen;

    for (const auto& [id, neighbours] : g.adj) {
        nodes.push_back(id);
        for (const auto& e : neighbours) {
            auto key = std::minmax(id, e.to);
            if (seen.count(key)) continue;
            seen.insert(key);
            edges.push_back({{"u", id}, {"v", e.to}, {"cost", e.cost}});
        }
    }

    json response = {{"nodes", nodes}, {"edges", edges}};
    res.set_content(response.dump(), "application/json");
}

// POST /topology — loads a fresh topology from JSON {nodes, edges}.
// WRITE: unique_lock blocks all reads/writes until this completes.
void Server::handlePostTopology(const httplib::Request& req, httplib::Response& res) {
    std::unique_lock lock(graphMu_);

    try {
        json body = json::parse(req.body);

        // Rebuild LSDB from scratch with the new topology.
        std::map<int, LSA> newDb;
        for (auto& node : body["nodes"]) {
            int id = node.get<int>();
            newDb[id] = LSA{id, {}, 1};
        }
        for (auto& edge : body["edges"]) {
            int u = edge["u"].get<int>();
            int v = edge["v"].get<int>();
            int cost = edge["cost"].get<int>();
            newDb[u].neighbours.push_back({v, cost});
            newDb[v].neighbours.push_back({u, cost});
        }

        for (auto& [id, lsa] : newDb) {
            lsdb_.update(lsa);
        }

        res.status = 200;
        res.set_content(R"({"status":"ok"})", "application/json");
    } catch (const std::exception& e) {
        res.status = 400;
        json err = {{"error", e.what()}};
        res.set_content(err.dump(), "application/json");
    }
}

// GET /route?src=X&dst=Y — shortest path between two routers.
// READ: shared_lock.
void Server::handleGetRoute(const httplib::Request& req, httplib::Response& res) {
    std::shared_lock lock(graphMu_);

    if (!req.has_param("src") || !req.has_param("dst")) {
        res.status = 400;
        res.set_content(R"({"error":"src and dst query params required"})", "application/json");
        return;
    }

    int src = std::stoi(req.get_param_value("src"));
    int dst = std::stoi(req.get_param_value("dst"));

    Graph g = lsdb_.buildGraph();
    auto [dist, parent] = dijkstra(g, src);

    if (dist.find(dst) == dist.end() || dist[dst] == UNREACHABLE) {
        res.status = 404;
        res.set_content(R"({"error":"no path found"})", "application/json");
        return;
    }

    auto path = reconstructPath(parent, src, dst);
    json pathJson = json::array();
    for (int node : path) pathJson.push_back(node);

    json response = {{"path", pathJson}, {"cost", dist[dst]}};
    res.set_content(response.dump(), "application/json");
}

// POST /fail — simulates a link failure {u, v} and returns reconvergence time.
// WRITE: unique_lock.
void Server::handlePostFail(const httplib::Request& req, httplib::Response& res) {
    std::unique_lock lock(graphMu_);

    try {
        json body = json::parse(req.body);
        int u = body["u"].get<int>();
        int v = body["v"].get<int>();

        // Reconverge from the lowest-ID router as a representative source.
        int src = lsdb_.all().empty() ? u : lsdb_.all().begin()->first;
        long long us = lsdb_.failLink(u, v, src);

        json response = {{"reconverge_us", us}, {"failed_link", {{"u", u}, {"v", v}}}};
        res.set_content(response.dump(), "application/json");
    } catch (const std::exception& e) {
        res.status = 400;
        json err = {{"error", e.what()}};
        res.set_content(err.dump(), "application/json");
    }
}

// GET /table/:id — full routing table for one router.
// READ: shared_lock.
void Server::handleGetTable(const httplib::Request& req, httplib::Response& res) {
    std::shared_lock lock(graphMu_);

    int routerID = std::stoi(req.matches[1]);
    Graph g = lsdb_.buildGraph();
    auto [dist, parent] = dijkstra(g, routerID);

    json table = json::array();
    for (const auto& [dest, cost] : dist) {
        if (dest == routerID || cost == UNREACHABLE) continue;

        auto path = reconstructPath(parent, routerID, dest);
        int nextHop = path.size() > 1 ? path[1] : dest;

        table.push_back({{"dest", dest}, {"nextHop", nextHop}, {"cost", cost}});
    }

    res.set_content(table.dump(), "application/json");
}