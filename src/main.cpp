#include "Server.h"
#include "LSDB.h"
#include <iostream>

int main() {
    LSDB lsdb;
    Server server(lsdb);

    std::cout << "OSPF Routing Engine starting...\n";
    std::cout << "Endpoints:\n";
    std::cout << "  GET  /topology         - view current network\n";
    std::cout << "  POST /topology         - load a network {nodes, edges}\n";
    std::cout << "  GET  /route?src=&dst=  - shortest path query\n";
    std::cout << "  POST /fail             - simulate link failure {u, v}\n";
    std::cout << "  GET  /table/:id        - full routing table for a router\n\n";

    server.start(8080);
    return 0;
}