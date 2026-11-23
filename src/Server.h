#pragma once

#include "LSDB.h"
#include <mutex>          // std::unique_lock
#include <shared_mutex>   // std::shared_mutex, std::shared_lock
#include <httplib.h>

// Exposes the routing engine over HTTP. cpp-httplib handles each request
// on its own thread, so every handler locks graphMu_ before touching the
// LSDB: reads take a shared_lock (run concurrently), writes take a
// unique_lock (exclusive — blocks readers too).
class Server {
public:
    explicit Server(LSDB& lsdb);
    void start(int port);

private:
    LSDB& lsdb_;
    mutable std::shared_mutex graphMu_;
    httplib::Server svr_;

    void registerRoutes();

    void handleGetTopology(const httplib::Request& req, httplib::Response& res);
    void handlePostTopology(const httplib::Request& req, httplib::Response& res);
    void handleGetRoute(const httplib::Request& req, httplib::Response& res);
    void handleGetTable(const httplib::Request& req, httplib::Response& res);
    void handlePostFail(const httplib::Request& req, httplib::Response& res);
    void handlePostRestore(const httplib::Request& req, httplib::Response& res);
    void handlePostReset(const httplib::Request& req, httplib::Response& res);
};
