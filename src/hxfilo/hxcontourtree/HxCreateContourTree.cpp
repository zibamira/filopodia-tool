#include <hxfield/HxUniformScalarField3.h>

#include <hxspatialgraph/internal/HxSpatialGraph.h>
#include <hxspatialgraph/internal/EdgeVertexAttribute.h>

#include "AugmentedContourTree.h"
#include "ContourTree.h"
#include "HxCreateContourTree.h"
#include "SimplicialMesh3DForHexahedralMesh.h"
#include "SimplicialMesh2D_Test.h"

HX_INIT_CLASS(HxCreateContourTree,HxCompModule);

HxCreateContourTree::HxCreateContourTree()
    : HxCompModule(HxLattice3::getClassTypeId())
    , portThreshold(this, "threshold", tr("Threshold"))
    , portOptions(this, "options", tr("Options"), 2)
    , portTrees(this, "trees", tr("Trees"), 3)
    , portDoIt(this, "doIt", tr("Do It"))
{
    portOptions.setLabel(0, "jitter coords");
    portOptions.setLabel(1, "test");
    portOptions.setValue(0, 0);
    portOptions.setValue(1, 0);

    portTrees.setLabel(0, "contour tree");
    portTrees.setLabel(1, "join tree");
    portTrees.setLabel(2, "split tree");
    portTrees.setValue(0);
}

HxCreateContourTree::~HxCreateContourTree()
{}

void
HxCreateContourTree::update()
{
    if ( portData.isNew() )
    {
        HxUniformScalarField3 * field = hxconnection_cast<HxUniformScalarField3>(portData);
        if ( field )
        {
            float min=0.f, max=0.f;
            field->lattice().computeRange(min, max);
            portThreshold.setMinMax(min, max);
            portThreshold.setValue((min + max)/2.f);
        }
    }

    portOptions.hide();
}

void
HxCreateContourTree::compute()
{
    if ( !portDoIt.wasHit() )
        return;

    HxUniformScalarField3 * field = hxconnection_cast<HxUniformScalarField3>(portData);
    if ( !field )
        return;

    if ( portOptions.getValue(1) )
        computeTrees_Test();
    else
        computeTrees();
}

void getTestMeshCoords(
    ContourTree               & contourTree,
    const std::vector< McVec3f > & coords,
    std::vector< McVec3f >       & coords2)
{
    coords2.clear();
    std::vector< mclong > meshVertexIds;
    contourTree.getMeshVertexIds(meshVertexIds);
    for ( size_t i=0; i<meshVertexIds.size(); ++i )
    {
        coords2.push_back(coords[meshVertexIds[i]]);
    }
}

void
HxCreateContourTree::computeTrees_Test()
{
    SimplicialMesh2D_Test testMesh;
    AugmentedContourTree augmentedContourTree(&testMesh);

    const std::vector< McVec3f > & coords = testMesh.getCoords();
    const std::vector< float >   & values = testMesh.getValues();

    if ( portTrees.getValue() == 0 )
    {
        augmentedContourTree.computeContourTree();
        const int resultIdJoinTree = 0;
        createSpatialGraphOfAugmentedTree(resultIdJoinTree, augmentedContourTree, coords, values);

        ContourTree contourTree(augmentedContourTree);
        std::vector< McVec3f > coords2;
        getTestMeshCoords(contourTree, coords, coords2);
        createSpatialGraphOfTree(resultIdJoinTree+3, contourTree, coords2, values);
    }
    else if ( portTrees.getValue() == 1 )
    {
        augmentedContourTree.computeJoinTree();
        const int resultIdJoinTree = 1;
        createSpatialGraphOfAugmentedTree(resultIdJoinTree, augmentedContourTree, coords, values);

        ContourTree contourTree(augmentedContourTree);
        std::vector< McVec3f > coords2;
        getTestMeshCoords(contourTree, coords, coords2);
        createSpatialGraphOfTree(resultIdJoinTree+3, contourTree, coords2, values);
    }
    else if ( portTrees.getValue() == 2 )
    {
        augmentedContourTree.computeSplitTree();
        const int resultIdSplitTree = 2;
        createSpatialGraphOfAugmentedTree(resultIdSplitTree, augmentedContourTree, coords, values);

        ContourTree contourTree(augmentedContourTree);
        std::vector< McVec3f > coords2;
        getTestMeshCoords(contourTree, coords, coords2);
        createSpatialGraphOfTree(resultIdSplitTree+3, contourTree, coords2, values);
    }
}

void
HxCreateContourTree::computeTrees()
{
    HxUniformScalarField3 * field = hxconnection_cast<HxUniformScalarField3>(portData);
    if ( !field )
        return;

    SimplicialMesh3DForHexahedralMesh hexahedralMesh(&(field->lattice()));
    hexahedralMesh.setThreshold(portThreshold.getValue());

    AugmentedContourTree augmentedContourTree(&hexahedralMesh);

    int resultId = 0;
    if ( portTrees.getValue() == 0 )
    {
        augmentedContourTree.computeContourTree();
        resultId = 0;
    }
    else if ( portTrees.getValue() == 1 )
    {
        augmentedContourTree.computeJoinTree();
        resultId = 1;
    }
    else if ( portTrees.getValue() == 2 )
    {
        augmentedContourTree.computeSplitTree();
        resultId = 2;
    }

    std::vector< McVec3i > gridPos;
    std::vector< McVec3f > coords;
    std::vector< float > values;

    hexahedralMesh.getGridPos(gridPos);

    getCoordsFromGridPos(gridPos, coords);

    getValuesFromGridPos(gridPos, values);

    createSpatialGraphOfAugmentedTree(resultId, augmentedContourTree, coords, values);


    ContourTree contourTree(augmentedContourTree);

    std::vector< mclong > meshVertexIds;
    contourTree.getMeshVertexIds(meshVertexIds);

    hexahedralMesh.getGridPos(meshVertexIds, gridPos);

    getCoordsFromGridPos(gridPos, coords);

    getValuesFromGridPos(gridPos, values);

    createSpatialGraphOfTree(resultId+3, contourTree, coords, values);
}

namespace
{

void jitterCoord(
    const McVec3f & voxelSize,
    McVec3f       & coord)
{
    coord += McVec3f(0.35*voxelSize[0]*((float)(rand()%2)),
                     0.35*voxelSize[1]*((float)(rand()%2)),
                     0.35*voxelSize[2]*((float)(rand()%2)));
}

}

void
HxCreateContourTree::getCoordsFromGridPos(
    const std::vector< McVec3i > & gridPos,
    std::vector< McVec3f >       & coords)
{
    HxUniformScalarField3 * field = hxconnection_cast<HxUniformScalarField3>(portData);
    if ( !field )
        return;

    const McBox3f& bbox = field->getBoundingBox();
    const McVec3f voxelSize = field->getVoxelSize();

    coords.resize(gridPos.size());
    for ( mclong i=0; i < static_cast<mclong>(gridPos.size()); ++i )
        for ( int j=0; j<3; ++j )
        {
            coords[i][j] = bbox[2*j] + float(gridPos[i][j]) * voxelSize[j];
            if ( portOptions.getValue(0) )
                jitterCoord(voxelSize, coords[i]);
        }
}

void
HxCreateContourTree::getValuesFromGridPos(
    const std::vector< McVec3i > & gridPos,
    std::vector< float >         & values)
{
    HxUniformScalarField3 * field = hxconnection_cast<HxUniformScalarField3>(portData);
    if ( !field )
        return;

    values.resize(gridPos.size());
    for ( mclong i=0; i < static_cast<mclong>(gridPos.size()); ++i )
        field->lattice().eval(gridPos[i][0],
                            gridPos[i][1],
                            gridPos[i][2],
                            &values[i]);
}

void
HxCreateContourTree::createSpatialGraphOfAugmentedTree(
    const int                     resultId,
    const AugmentedContourTree  & contourTree,
    const std::vector< McVec3f >   & coords,
    const std::vector< float >     & values)
{
    std::vector< mclong > components;
    contourTree.getComponents(components);

    HxSpatialGraph * spatialGraph = HxSpatialGraph::createInstance();
    if ( portTrees.getValue() == 0 )
        spatialGraph->setLabel("AugmentedContourTree");
    else if ( portTrees.getValue() == 1 )
        spatialGraph->setLabel("AugmentedJoinTree");
    else
        spatialGraph->setLabel("AugmentedSplitTree");

    EdgeVertexAttribute * idxAttrib =
        static_cast<EdgeVertexAttribute*>(spatialGraph->addAttribute("Idx", HxSpatialGraph::VERTEX, 
                                                                     McPrimType::MC_INT32, 1));
    EdgeVertexAttribute * valueAttrib =
        static_cast<EdgeVertexAttribute*>(spatialGraph->addAttribute("Value", HxSpatialGraph::VERTEX, 
                                                                     McPrimType::MC_FLOAT, 1));
    EdgeVertexAttribute * componentAttrib =
        static_cast<EdgeVertexAttribute*>(spatialGraph->addAttribute("Component", HxSpatialGraph::VERTEX, 
                                                                     McPrimType::MC_INT32, 1));

    for ( mclong i=0; i < static_cast<mclong>(coords.size()); ++i )
    {
        spatialGraph->addVertex(coords[i]);
        idxAttrib->setIntDataAtIdx(int(i), int(i));
        valueAttrib->setFloatDataAtIdx(int(i), values[i]);
        componentAttrib->setIntDataAtIdx(int(i), int(components[i]));
    }

    std::vector< McSmallArray< mclong, 2 > > edges;
    contourTree.getEdges(edges);

    for ( mclong i=0; i < static_cast<mclong>(edges.size()); ++i )
    {
        spatialGraph->addEdge(edges[i][0], edges[i][1]);
    }    

    setResult(resultId, spatialGraph);
}

void
HxCreateContourTree::createSpatialGraphOfTree(
    const int                       resultId,
    const ContourTree             & contourTree,
    const std::vector< McVec3f >     & coords,
    const std::vector< float >       & values)
{
    HxSpatialGraph * spatialGraph = HxSpatialGraph::createInstance();
    if ( portTrees.getValue() == 0 )
        spatialGraph->setLabel("ContourTree");
    else if ( portTrees.getValue() == 1 )
        spatialGraph->setLabel("JoinTree");
    else
        spatialGraph->setLabel("SplitTree");

    EdgeVertexAttribute * idxAttrib =
        static_cast<EdgeVertexAttribute*>(spatialGraph->addAttribute("Idx", HxSpatialGraph::VERTEX, 
                                                                     McPrimType::MC_INT32, 1));
    EdgeVertexAttribute * valueAttrib =
        static_cast<EdgeVertexAttribute*>(spatialGraph->addAttribute("Value", HxSpatialGraph::VERTEX, 
                                                                     McPrimType::MC_FLOAT, 1));
    EdgeVertexAttribute * criticalPointAttrib =
        static_cast<EdgeVertexAttribute*>(spatialGraph->addAttribute("CriticalPoints", HxSpatialGraph::VERTEX, 
                                                                     McPrimType::MC_INT32, 1));

    std::vector< mculong > maxima;
    std::vector< mculong > minima;
    std::vector< mculong > saddles;
    std::vector< McVec2i > edges;
    contourTree.getNodesAndEdges(maxima, minima, saddles, edges);

    mculong numCoords = 0;
    for ( size_t i=0; i<maxima.size(); ++i )
    {
        spatialGraph->addVertex(coords[maxima[i]]);
        idxAttrib->setIntDataAtIdx(int(numCoords), int(maxima[i]+1));
        valueAttrib->setFloatDataAtIdx(int(numCoords), values[maxima[i]]);
        criticalPointAttrib->setIntDataAtIdx(int(numCoords), 2);
        ++numCoords;
    }

    for ( size_t i=0; i<saddles.size(); ++i )
    {
        spatialGraph->addVertex(coords[saddles[i]]);
        idxAttrib->setIntDataAtIdx(int(numCoords), int(saddles[i]+1));
        valueAttrib->setFloatDataAtIdx(int(numCoords), values[saddles[i]]);
        criticalPointAttrib->setIntDataAtIdx(int(numCoords), 1);       
        ++numCoords;
    }

    for ( size_t i=0; i<minima.size(); ++i )
    {
        spatialGraph->addVertex(coords[minima[i]]);
        idxAttrib->setIntDataAtIdx(int(numCoords), int(minima[i]+1));
        valueAttrib->setFloatDataAtIdx(int(numCoords), values[minima[i]]);
        criticalPointAttrib->setIntDataAtIdx(int(numCoords), 0);       
        ++numCoords;
    }

    for ( mclong i=0; i < static_cast<mclong>(edges.size()); ++i )
    {
        spatialGraph->addEdge(edges[i][0], edges[i][1]);
    }

    setResult(resultId, spatialGraph);
}
