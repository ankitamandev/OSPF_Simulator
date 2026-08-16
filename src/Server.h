#pragma once

#include "LSDB.h"
#include <shared_mutex>
#include <httplib.h>

// Server to handle backend requests
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
    void handlePostFail(const httplib::Request& req, httplib::Response& res);
    void handleGetTable(const httplib::Request& req, httplib::Response& res);
    void handlePostReset(const httplib::Request& req, httplib::Response& res);
    void handlePostRestore(const httplib::Request& req, httplib::Response& res);
};