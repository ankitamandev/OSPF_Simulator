// Reconvergence experiment harness: full vs incremental SPF.
//
// Each event fails B existing links (link DOWN), then restores them with
// their ORIGINAL costs (link UP), so the topology is identical at the start
// of every event. Both strategies replay the exact same event sequence
// (same seed), timed separately:
//   full — apply all B changes to the LSDB, then one full Dijkstra
//   incr — apply each change and update the existing tree incrementally
// Only SPF computation is timed (nanoseconds, steady_clock); LSDB mutation
// cost is identical for both and excluded.
//
// After every event the incremental tree is checked against a fresh full
// recompute (verifySpt). Any mismatch aborts the run — timing data from an
// incorrect algorithm is meaningless.
//
// Usage:
//   benchmark [--topology random|campus] [--nodes N] [--trials T]
//             [--warmup W] [--reps R] [--burst B] [--readers K]
//             [--root ID] [--seed S] [--csv FILE] [--no-verify]
//   benchmark N T            (legacy positional form: nodes, trials)

#include "LSDB.h"
#include "Dijkstra.h"
#include "SpfEngine.h"
#include "DefaultTopology.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <random>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using Clock = std::chrono::steady_clock;

namespace {

struct Config {
    std::string topology = "random";
    int nodes = 50, trials = 1000, warmup = 100, reps = 5;
    int burst = 1, readers = 0, root = 1;
    unsigned seed = 42;
    std::string csv;
    bool verify = true;
};

struct Link { int u, v, cost; };

enum class Mode  { Full, Incr };
enum class Event { Down, Up };

struct Record {
    int rep, trial;
    Mode mode;
    Event event;
    long long spfNs;      // SPF computation only
    long long waitNs;     // time to acquire the writer lock
    long long totalNs;    // lock request -> release: what a reader would feel
    int touched;          // nodes reset or updated
};

long long ns(Clock::time_point a, Clock::time_point b) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(b - a).count();
}

// Connected random topology with no duplicate links: each router joins to
// one or two earlier routers (guarantees connectivity), then ~N/2 extra
// links bring the average degree to about 4, typical of real networks.
void buildRandom(LSDB& db, int n, std::mt19937& rng) {
    db.clear();
    for (int i = 1; i <= n; i++) db.update(LSA{i, {}, 1});
    std::uniform_int_distribution<int> cost(1, 100);
    for (int i = 2; i <= n; i++) {
        std::uniform_int_distribution<int> earlier(1, i - 1);
        int links = std::min(i - 1, 1 + static_cast<int>(rng() % 2));
        for (int k = 0; k < links; k++) db.setLink(i, earlier(rng), cost(rng));
    }
    std::uniform_int_distribution<int> any(1, n);
    for (int added = 0, tries = 0; added < n / 2 && tries < n * 50; tries++) {
        int u = any(rng), v = any(rng);
        if (u == v || db.linkCost(u, v)) continue;
        db.setLink(u, v, cost(rng));
        added++;
    }
}

std::vector<Link> collectLinks(const LSDB& db) {
    std::vector<Link> links;
    for (const auto& [id, lsa] : db.all())
        for (const auto& [nbr, c] : lsa.neighbours)
            if (id < nbr) links.push_back({id, nbr, c});
    return links;
}

void buildTopology(LSDB& db, const Config& cfg, std::mt19937& rng) {
    if (cfg.topology == "campus") loadDefaultTopology(db);
    else                          buildRandom(db, cfg.nodes, rng);
}

// Readers model routing-table lookups: pick a destination and walk the
// tree back toward the root to find the first hop, under a shared lock.
void readerLoop(const SPT* const* tree, std::shared_mutex* mu, int maxId, unsigned seed,
                std::atomic<bool>* stop, std::atomic<long long>* reads) {
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> dst(1, std::max(1, maxId));
    long long local = 0;
    volatile int sink = 0;
    while (!stop->load(std::memory_order_relaxed)) {
        int d = dst(rng);
        {
            std::shared_lock lock(*mu);
            const SPT& t = **tree;
            const int n = static_cast<int>(t.parent.size());
            int hop = d, steps = 0;
            if (d < n && t.dist[d] != UNREACHABLE)
                while (t.parent[hop] != NO_PARENT && t.parent[t.parent[hop]] != NO_PARENT &&
                       steps++ < n)
                    hop = t.parent[hop];
            sink = hop;
        }
        local++;
    }
    (void)sink;
    reads->fetch_add(local);
}

// Runs one repetition in one mode. Same seed => same topology and same
// event sequence for both modes, so results pair up event by event.
std::vector<Record> runRep(const Config& cfg, int rep, Mode mode, long long* readsOut) {
    std::mt19937 rng(cfg.seed + rep);
    LSDB db;
    buildTopology(db, cfg, rng);
    auto links = collectLinks(db);
    if (cfg.burst < 1 || cfg.burst > static_cast<int>(links.size())) {
        std::cerr << "burst must be between 1 and the link count (" << links.size() << ")\n";
        std::exit(1);
    }

    std::shared_mutex mu;
    SpfEngine engine(db, cfg.root);          // incremental state
    SPT fullTree = dijkstra(db, cfg.root);   // full-mode state
    const SPT* current = (mode == Mode::Incr) ? &engine.tree() : &fullTree;

    std::atomic<bool> stop{false};
    std::atomic<long long> reads{0};
    std::vector<std::thread> readers;
    for (int k = 0; k < cfg.readers; k++)
        readers.emplace_back(readerLoop, &current, &mu, db.maxRouterId(),
                             cfg.seed * 1000 + k, &stop, &reads);

    std::vector<int> order(links.size());
    for (size_t i = 0; i < order.size(); i++) order[i] = static_cast<int>(i);

    std::vector<Record> out;
    const int total = cfg.warmup + cfg.trials;

    for (int trial = 0; trial < total; trial++) {
        // Choose B distinct existing links (partial Fisher-Yates).
        for (int k = 0; k < cfg.burst; k++) {
            std::uniform_int_distribution<int> pick(k, static_cast<int>(order.size()) - 1);
            std::swap(order[k], order[pick(rng)]);
        }

        for (Event ev : {Event::Down, Event::Up}) {
            long long spf = 0;
            int touched = 0;

            auto tReq = Clock::now();
            std::unique_lock lock(mu);
            auto tAcq = Clock::now();

            if (mode == Mode::Full) {
                for (int k = 0; k < cfg.burst; k++) {
                    const Link& l = links[order[k]];
                    if (ev == Event::Down) db.removeLink(l.u, l.v);
                    else                   db.setLink(l.u, l.v, l.cost);
                }
                auto t0 = Clock::now();
                fullTree = dijkstra(db, cfg.root);
                spf = ns(t0, Clock::now());
                touched = static_cast<int>(db.routerCount());
            } else {
                // One change at a time, so each update starts from a tree
                // that was valid for the topology before that single change.
                for (int k = 0; k < cfg.burst; k++) {
                    const Link& l = links[order[k]];
                    if (ev == Event::Down) db.removeLink(l.u, l.v);
                    else                   db.setLink(l.u, l.v, l.cost);
                    auto t0 = Clock::now();
                    if (ev == Event::Down) engine.onLinkDown(l.u, l.v);
                    else                   engine.onLinkUp(l.u, l.v, l.cost);
                    spf += ns(t0, Clock::now());
                    touched += engine.lastTouched();
                }
            }

            lock.unlock();
            auto tRel = Clock::now();

            if (cfg.verify && mode == Mode::Incr) {
                std::shared_lock rlock(mu);
                std::string why;
                if (!verifySpt(db, cfg.root, engine.tree(), &why)) {
                    std::cerr << "\nCORRECTNESS FAILURE (rep " << rep << ", trial " << trial
                              << ", " << (ev == Event::Down ? "down" : "up") << "): " << why << "\n";
                    rlock.unlock();
                    stop = true;
                    for (auto& t : readers) t.join();
                    std::exit(2);
                }
            }

            if (trial >= cfg.warmup)
                out.push_back({rep, trial - cfg.warmup, mode, ev, spf,
                               ns(tReq, tAcq), ns(tReq, tRel), touched});
        }
    }

    stop = true;
    for (auto& t : readers) t.join();
    if (readsOut) *readsOut = reads.load();
    return out;
}

// ---------- statistics ----------

double percentile(std::vector<long long> v, double p) {
    if (v.empty()) return 0;
    std::sort(v.begin(), v.end());
    size_t idx = static_cast<size_t>(std::ceil(p / 100.0 * v.size()));
    return static_cast<double>(v[idx == 0 ? 0 : idx - 1]);
}

double mean(const std::vector<double>& v) {
    double s = 0;
    for (double x : v) s += x;
    return v.empty() ? 0 : s / v.size();
}

// Two-sided 95% Student-t critical value.
double tCrit(int df) {
    static const double table[] = {0, 12.706, 4.303, 3.182, 2.776, 2.571, 2.447, 2.365,
        2.306, 2.262, 2.228, 2.201, 2.179, 2.160, 2.145, 2.131, 2.120, 2.110, 2.101,
        2.093, 2.086, 2.080, 2.074, 2.069, 2.064, 2.060, 2.056, 2.052, 2.048, 2.045, 2.042};
    return df >= 1 && df <= 30 ? table[df] : 1.96;
}

// Mean across reps and 95% confidence-interval half-width.
std::pair<double, double> meanCi(const std::vector<double>& v) {
    double m = mean(v);
    if (v.size() < 2) return {m, 0};
    double ss = 0;
    for (double x : v) ss += (x - m) * (x - m);
    double sd = std::sqrt(ss / (v.size() - 1));
    return {m, tCrit(static_cast<int>(v.size()) - 1) * sd / std::sqrt(static_cast<double>(v.size()))};
}

std::string fmt(std::pair<double, double> mc) {
    std::ostringstream s;
    s << std::fixed << std::setprecision(2) << std::setw(8) << mc.first / 1000.0 << " ± "
      << std::left << std::setw(6) << mc.second / 1000.0;
    return s.str();
}

void report(const Config& cfg, const std::vector<Record>& all, bool withLoad) {
    for (Event ev : {Event::Down, Event::Up}) {
        std::cout << "\n" << (ev == Event::Down ? "LINK DOWN" : "LINK UP")
                  << (withLoad ? " - end-to-end latency (lock wait + update), microseconds"
                               : " - SPF time, microseconds")
                  << "  [mean across reps +/- 95% CI]\n";
        std::cout << "  mode       p50                p95                p99"
                     "                mean            touched\n";

        std::map<Mode, double> p50s;
        for (Mode m : {Mode::Full, Mode::Incr}) {
            std::vector<double> p50, p95, p99, mn, tch;
            for (int r = 0; r < cfg.reps; r++) {
                std::vector<long long> xs;
                double tsum = 0;
                for (const auto& rec : all)
                    if (rec.rep == r && rec.mode == m && rec.event == ev) {
                        xs.push_back(withLoad ? rec.totalNs : rec.spfNs);
                        tsum += rec.touched;
                    }
                if (xs.empty()) continue;
                p50.push_back(percentile(xs, 50));
                p95.push_back(percentile(xs, 95));
                p99.push_back(percentile(xs, 99));
                double s = 0;
                for (auto x : xs) s += x;
                mn.push_back(s / xs.size());
                tch.push_back(tsum / xs.size());
            }
            p50s[m] = mean(p50);
            std::cout << "  " << (m == Mode::Full ? "full" : "incr") << "  " << fmt(meanCi(p50))
                      << "  " << fmt(meanCi(p95)) << "  " << fmt(meanCi(p99)) << "  "
                      << fmt(meanCi(mn)) << "  " << std::fixed << std::setprecision(1)
                      << mean(tch) << "\n";
        }
        if (p50s[Mode::Incr] > 0)
            std::cout << "  speedup at p50 (full / incr): " << std::fixed << std::setprecision(1)
                      << p50s[Mode::Full] / p50s[Mode::Incr] << "x\n";
    }
}

Config parse(int argc, char* argv[]) {
    Config c;
    if (argc >= 2 && std::string(argv[1]).rfind("--", 0) != 0) {   // legacy: N T
        c.nodes = std::atoi(argv[1]);
        if (argc >= 3) c.trials = std::atoi(argv[2]);
        return c;
    }
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        auto next = [&]() -> std::string {
            if (i + 1 >= argc) { std::cerr << "missing value for " << a << "\n"; std::exit(1); }
            return argv[++i];
        };
        if      (a == "--topology")  c.topology = next();
        else if (a == "--nodes")     c.nodes    = std::stoi(next());
        else if (a == "--trials")    c.trials   = std::stoi(next());
        else if (a == "--warmup")    c.warmup   = std::stoi(next());
        else if (a == "--reps")      c.reps     = std::stoi(next());
        else if (a == "--burst")     c.burst    = std::stoi(next());
        else if (a == "--readers")   c.readers  = std::stoi(next());
        else if (a == "--root")      c.root     = std::stoi(next());
        else if (a == "--seed")      c.seed     = static_cast<unsigned>(std::stoul(next()));
        else if (a == "--csv")       c.csv      = next();
        else if (a == "--no-verify") c.verify   = false;
        else { std::cerr << "unknown option " << a << "\n"; std::exit(1); }
    }
    if (c.topology != "random" && c.topology != "campus") {
        std::cerr << "--topology must be random or campus\n";
        std::exit(1);
    }
    return c;
}

} // namespace

int main(int argc, char* argv[]) {
    Config cfg = parse(argc, argv);

    {
        std::mt19937 rng(cfg.seed);
        LSDB db;
        buildTopology(db, cfg, rng);
        auto links = collectLinks(db);
        std::cout << "Topology: " << cfg.topology << ", " << db.routerCount() << " routers, "
                  << links.size() << " links, avg degree " << std::fixed << std::setprecision(1)
                  << 2.0 * links.size() / db.routerCount() << ", root " << cfg.root << "\n";
    }
    std::cout << "Per rep: " << cfg.warmup << " warm-up + " << cfg.trials
              << " measured events | burst " << cfg.burst << " | readers " << cfg.readers
              << " | reps " << cfg.reps << " | seed " << cfg.seed << "\n"
              << "Correctness oracle: "
              << (cfg.verify ? "ON (incremental tree checked after every event)" : "OFF") << "\n";

    std::vector<Record> all;
    long long readsFull = 0, readsIncr = 0;
    for (int r = 0; r < cfg.reps; r++) {
        for (Mode m : {Mode::Full, Mode::Incr}) {
            long long reads = 0;
            auto recs = runRep(cfg, r, m, &reads);
            (m == Mode::Full ? readsFull : readsIncr) += reads;
            all.insert(all.end(), recs.begin(), recs.end());
        }
    }

    report(cfg, all, cfg.readers > 0);
    if (cfg.readers > 0)
        std::cout << "\nReader lookups completed - full: " << readsFull
                  << ", incr: " << readsIncr << "\n";

    if (!cfg.csv.empty()) {
        std::ofstream f(cfg.csv);
        f << "topology,nodes,burst,readers,rep,trial,mode,event,spf_ns,wait_ns,total_ns,touched\n";
        for (const auto& r : all)
            f << cfg.topology << "," << cfg.nodes << "," << cfg.burst << "," << cfg.readers << ","
              << r.rep << "," << r.trial << "," << (r.mode == Mode::Full ? "full" : "incr") << ","
              << (r.event == Event::Down ? "down" : "up") << "," << r.spfNs << "," << r.waitNs
              << "," << r.totalNs << "," << r.touched << "\n";
        std::cout << "Raw events written to " << cfg.csv << "\n";
    }
    return 0;
}
