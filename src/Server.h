#pragma once

#include "LSDB.h"
#include <shared_mutex>
#include <httplib.h>

// Exposes the routing engine over HTTP. The LSDB is shared mutable
// state accessed from multiple request-handling threads, so all
// access goes through a std::shared_mutex: read endpoints take a
// shared_lock (concurrent reads allowed), write endpoints take a
// unique_lock (exclusive access while mutating topology).
class Server {
public:
    explicit Server(LSDB& lsdb);

    // Starts listening on the given port. Blocks until the server
    // is stopped (Ctrl+C).
    void start(int port);

private:
    LSDB& lsdb_;
    mutable std::shared_mutex graphMu_;
    httplib::Server svr_;

    void registerRoutes();

    void handleGetTopology(const httplib::Request& req, httplib::Response& res);
    void handlePostTopology(const httplib::Request& req, httplib::Response& res);
    void handleGetRoute(const httplib::Request& req, httplib::Response& res);
    void handlePostFail(const httplib::Request& req, httplib::Response& res);
    void handleGetTable(const httplib::Request& req, httplib::Response& res);
};