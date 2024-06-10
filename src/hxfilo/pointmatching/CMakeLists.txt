#######################################
# Include necessary AvizoApps utility files
#######################################
include(CopyUtils)

#######################################
# Search all required packages
#######################################
find_package(AvizoAppsQt5 REQUIRED)

#######################################
# Create a target for pointmatching
#######################################
avizoapps_add_library(pointmatching SHARED
    CXX_SOURCES
        # Public
            AngularCacheObject3d.cpp
            AngularCacheObject3d.h
            AngularPointRepresentation3d.cpp
            AngularPointRepresentation3d.h
            CacheObject.cpp
            CacheObject.h
            ExactPointMatchingAlgorithm.cpp
            ExactPointMatchingAlgorithm.h
            GreedyPointMatchingAlgorithm.cpp
            GreedyPointMatchingAlgorithm.h
            HxTest_PointMatching.cpp
            HxTest_PointMatching.h
            IterativePointMatching.cpp
            IterativePointMatching.h
            McCliqueDetection.cpp
            McCliqueDetection.h
            PointMatchingAlgorithm.cpp
            PointMatchingAlgorithm.h
            PointMatchingDataStruct.cpp
            PointMatchingDataStruct.h
            PointMatchingScoringFunction.cpp
            PointMatchingScoringFunction.h
            PointRepresentation.h
            SimpleCacheObject3d.cpp
            SimpleCacheObject3d.h
            SimplePointRepresentation3d.cpp
            SimplePointRepresentation3d.h
            SparsePointMatchingAlgorithm.cpp
            SparsePointMatchingAlgorithm.h
            StartTransformationGenerator3d.cpp
            StartTransformationGenerator3d.h
            Test_ExactPointMatching.cpp
            Test_ExactPointMatching.h
            Test_GreedyPointMatching.cpp
            Test_GreedyPointMatching.h
            Transformation.cpp
            Transformation.h
)

target_link_libraries(pointmatching
    PUBLIC
    AvizoApps::Qt5Core
    hxcore
    hxgraphalgorithms
    mclib
)

#######################################
# Add files to copy to the same location within the build folder
#######################################
avizoapps_target_share(pointmatching
    SHARE
    share/resources/pointmatching.rc
)

