#######################################
# Include necessary AvizoApps utility files
#######################################
include(CopyUtils)


#######################################
# Create a target for hxgraphalgorithms
#######################################
avizoapps_add_library(hxgraphalgorithms SHARED
    CXX_SOURCES
        # Public
            KruskalTreeClusterer.cpp
            KruskalTreeClusterer.h
            McCliqueDetection.cpp
            McCliqueDetection.h
            McDijkstraShortestPath.cpp
            McDijkstraShortestPath.h
            McMinSpanningTree.h
)

target_link_libraries(hxgraphalgorithms
    PUBLIC
    mclib
)

