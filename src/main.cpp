#include "Server.h"
#include "LSDB.h"
#include "DefaultTopology.h"
#include <iostream>

int main() {
    LSDB lsdb;
    loadDefaultTopology(lsdb);
    std::cout << "Default topology loaded: " << lsdb.routerCount() << " routers.\n";

    Server server(lsdb);

    std::cout << "OSPF Routing Engine starting \n"
              << "Endpoints:\n"
              << "  GET  /topology         - view current network\n"
              << "  POST /topology         - replace the network {nodes, edges}\n"
              << "  GET  /route?src=&dst=  - shortest path query\n"
              << "  GET  /table/:id        - full routing table for a router\n"
              << "  POST /fail             - fail a link {u, v}\n"
              << "  POST /restore          - restore a link {u, v, cost}\n"
              << "  POST /reset            - reload the default topology\n\n";

    server.start(8080);
    return 0;
}
