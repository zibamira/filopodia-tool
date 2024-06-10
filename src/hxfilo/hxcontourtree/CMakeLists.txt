#######################################
# Include necessary AvizoApps utility files
#######################################
include(CopyUtils)

#######################################
# Search all required packages
#######################################
find_package(AvizoAppsOIV REQUIRED)
find_package(AvizoAppsQt5 REQUIRED)

#######################################
# Create a target for hxcontourtree
#######################################
avizoapps_add_library(hxcontourtree SHARED
    CXX_SOURCES
        # Public
            AugmentedContourTree.cpp
            AugmentedContourTree.h
            CompareCheckFunctors.h
            ContourTree.cpp
            ContourTree.h
            HxContourTreeSegmentation.cpp
            HxContourTreeSegmentation.h
            HxCreateContourTree.cpp
            HxCreateContourTree.h
            HxMergeTreeSegmentation.cpp
            HxMergeTreeSegmentation.h
            HxPropagateContours.cpp
            HxPropagateContours.h
            HxTouchPointGraphBuilder.cpp
            HxTouchPointGraphBuilder.h
            Lattice3Mesh.cpp
            Lattice3Mesh.h
            MergeTree.cpp
            MergeTree.h
            SetUnionDataStructure.cpp
            SetUnionDataStructure.h
            SimplicialMesh.h
            SimplicialMesh2D_Test.cpp
            SimplicialMesh2D_Test.h
            SimplicialMesh3DForHexahedralMesh.cpp
            SimplicialMesh3DForHexahedralMesh.h
)

target_link_libraries(hxcontourtree
    PUBLIC
    AvizoApps::Qt5Core
    AvizoApps::Inventor
    amiramesh
    hxcore
    hxfield
    hxlandmark
    hxplot
    hxspatialgraph
    mclib
)

#######################################
# Add files to copy to the same location within the build folder
#######################################
avizoapps_target_share(hxcontourtree
    SHARE
    data/tutorials/contourtree/foam_ContourTreeSegmentation.hx
    data/tutorials/contourtree/foam_HierarchicalWatershed_versus_ContourTreeSegmentation.hx
    data/tutorials/contourtree/foam_WatershedSeparation-8-SegmentedPores_versus_ContourTreeSegmentation.hx
    share/resources/hxcontourtree.rc
    share/script-objects/ContourTreeSegmentation.scro
)

