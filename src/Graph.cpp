#include "Graph.h"

// Graph is currently fully implemented in the header (Graph.h) since all
// methods are small and benefit from inlining. This .cpp file exists as
// the translation unit for the Graph component and is where you would
// move implementations if Graph.h grows larger (e.g. when adding
// serialization, weighted shortest-path caching, or persistence).