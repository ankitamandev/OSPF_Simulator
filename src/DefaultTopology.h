#pragma once
#include "LSDB.h"

// Loads the default 20-router enterprise campus topology into the LSDB.
// Routers 1-3: core, 4-8: distribution, 9-20: edge/access.
// Clears the LSDB first, so it is safe to call again for a reset.
void loadDefaultTopology(LSDB& lsdb);
