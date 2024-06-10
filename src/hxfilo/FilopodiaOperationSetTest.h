#include "FilopodiaOperationSet.h"
#include "FilopodiaFunctions.h"
#include <hxspatialgraph/internal/HxSpatialGraph.h>
#include <gtest/gtest.h>
#include <mclib/McException.h>
#include <qdebug.h>

#include "ConvertVectorAndMcDArray.h"

class FilopodiaOperationTest : public ::testing::Test
{
protected:
    virtual void
    SetUp()
    {
        TimeMinMax timeMinMax;
        timeMinMax.minT = 0;
        timeMinMax.maxT = 10;

        graph = HxSpatialGraph::createInstance();
        FilopodiaFunctions::addAllFilopodiaLabelGroups(graph, timeMinMax);

        // Points
        const McVec3f p1(5.0f, 4.0f, 0.0f);     //  v1 (root)
        const McVec3f p2(9.0f, 8.0f, 0.0f);     //  v2 (tip1)
        const McVec3f p3(2.0f, 1.0f, 0.0f);     //  v3 (tip2)
        const McVec3f p4(1.0f, 2.0f, 0.0f);     //  v4 (tip3)
        const McVec3f p5(5.0f, 7.0f, 0.0f);     //  v5 (tip4)
        const McVec3f p6(1.0f, 7.0f, 0.0f);     //  v6 (tip5)
        const McVec3f p7(7.0f, 6.0f, 0.0f);     //  v7 (base1)
        const McVec3f p8(4.0f, 3.0f, 0.0f);     //  v8 (base2)
        const McVec3f p9(4.0f, 6.0f, 0.0f);     //  v9 (base3)
        const McVec3f p10(2.0f, 6.0f, 0.0f);    // v10 (base4)
        const McVec3f p11(3.0f, 2.0f, 0.0f);    // v11 (branch1)
        const McVec3f p12(3.0f, 5.0f, 0.0f);    // v12 (branch2)
        const McVec3f p13(6.0f, 5.0f, 0.0f);    // e1
        const McVec3f p14(8.0f, 7.0f, 0.0f);    // e2
        const McVec3f p15(2.0f, 2.0f, 0.0f);    // e6
        const McVec3f p16(4.0f, 4.5f, 0.0f);    // e7

        // Nodes
        v1 = graph->addVertex(p1);              // root
        v2 = graph->addVertex(p2);              // (tip1)
        v3 = graph->addVertex(p3);              // (tip2)
        v4 = graph->addVertex(p4);              // (tip3)
        v5 = graph->addVertex(p5);              // (tip4)
        v6 = graph->addVertex(p6);              // (tip5)
        v7 = graph->addVertex(p7);              // (base1)
        v8 = graph->addVertex(p8);              // (base2)
        v9 = graph->addVertex(p9);              // (base3)
        v10 = graph->addVertex(p10);            // (base4)
        v11 = graph->addVertex(p11);            // (branch1)
        v12 = graph->addVertex(p12);            // (branch2)

        // Edges
        std::vector<McVec3f> pEdge1;               // root - - base1
        pEdge1.push_back(p1);
        pEdge1.push_back(p13);
        pEdge1.push_back(p7);

        e1 = graph->addEdge(v1, v7, pEdge1);

        std::vector<McVec3f> pEdge2;               // base1 - - tip1
        pEdge2.push_back(p7);
        pEdge2.push_back(p14);
        pEdge2.push_back(p2);

        e2 = graph->addEdge(v7, v2, pEdge2);

        std::vector<McVec3f> pEdge3;               // root - base2
        pEdge3.push_back(p1);
        pEdge3.push_back(p8);

        e3 = graph->addEdge(v1, v8, pEdge3);

        std::vector<McVec3f> pEdge4;               // base2 - branch1
        pEdge4.push_back(p8);
        pEdge4.push_back(p11);

        e4 = graph->addEdge(v8, v11, pEdge4);

        std::vector<McVec3f> pEdge5;               // branch1 - tip2
        pEdge5.push_back(p11);
        pEdge5.push_back(p3);

        e5 = graph->addEdge(v11, v3, pEdge5);

        std::vector<McVec3f> pEdge6;               // branch1 - - tip3
        pEdge6.push_back(p11);
        pEdge6.push_back(p15);
        pEdge6.push_back(p4);

        e6 = graph->addEdge(v11, v4, pEdge6);

        std::vector<McVec3f> pEdge7;               // root - - branch2
        pEdge7.push_back(p1);
        pEdge7.push_back(p16);
        pEdge7.push_back(p12);

        e7 = graph->addEdge(v1, v12, pEdge7);

        std::vector<McVec3f> pEdge8;               // branch2 - base3
        pEdge8.push_back(p12);
        pEdge8.push_back(p10);

        e8 = graph->addEdge(v12, v10, pEdge8);

        std::vector<McVec3f> pEdge9;               // base3 - tip4
        pEdge9.push_back(p10);
        pEdge9.push_back(p5);

        e9 = graph->addEdge(v10, v5, pEdge9);

        std::vector<McVec3f> pEdge10;              // branch2 - base4
        pEdge10.push_back(p12);
        pEdge10.push_back(p10);

        e10 = graph->addEdge(v12, v10, pEdge10);

        std::vector<McVec3f> pEdge11;              // base3 - tip5
        pEdge11.push_back(p10);
        pEdge11.push_back(p6);

        e11 = graph->addEdge(v10, v6, pEdge11);

        // Set labels
        labels.push_back(0);                                                           //gc
        labels.push_back(8);                                                           //timstep
        labels.push_back(FilopodiaFunctions::getTypeLabelId(graph, BASE_NODE));        //type
        labels.push_back(FilopodiaFunctions::getLocationLabelId(graph, FILOPODIUM));   //location
        labels.push_back(FilopodiaFunctions::getFilopodiaLabelId(graph, UNASSIGNED));  //filopodia
        labels.push_back(FilopodiaFunctions::getMatchLabelId(graph, UNASSIGNED));      //match
        labels.push_back(FilopodiaFunctions::getManualGeometryLabelId(graph,MANUAL));  //geometry
    }

    McHandle<HxSpatialGraph> graph;
    int v1;
    int v2;
    int v3;
    int v4;
    int v5;
    int v6;
    int v7;
    int v8;
    int v9;
    int v10;
    int v11;
    int v12;

    int e1;
    int e2;
    int e3;
    int e4;
    int e5;
    int e6;
    int e7;
    int e8;
    int e9;
    int e10;
    int e11;

    McDArray<int> labels;
};

TEST_F(FilopodiaOperationTest, ConvertFilopodiaPoint)
{
    // Prepare operation
    // Select second edge point on e1 (root - - base1)
    const SpatialGraphPoint point = SpatialGraphPoint(e1, 1);

    SpatialGraphSelection sel(graph);
    sel.clear();
    sel.selectPoint(point);

    McHandle<HxSpatialGraph> modifiedGraph = graph->duplicate();

    // Execute operation
    try
    {
        ConvertFilopodiaPointOperationSet op(modifiedGraph, sel, SpatialGraphSelection(graph), labels);

        op.exec();
        op.undo();

        EXPECT_EQ(graph->getNumVertices(), modifiedGraph->getNumVertices());
        EXPECT_EQ(graph->getNumEdges(), modifiedGraph->getNumEdges());
        EXPECT_EQ(graph->getTotalNumEdgePoints(), modifiedGraph->getTotalNumEdgePoints());
    }
    catch (McException& e)
    {
        qDebug() << "\n Error in FilopodiaOperationTest ConvertFilopodiaPoint: \n" << e.what();
        EXPECT_TRUE(false);
    }
}

TEST_F(FilopodiaOperationTest, AddFilopodiaVertex)
{
    // Prepare operation
    const McVec3f newPos(6.0f, 1.0f, 0.0f);

    McHandle<HxSpatialGraph> modifiedGraph = graph->duplicate();

    // Execute operation
    try
    {
        AddFilopodiaVertexOperationSet op(modifiedGraph,
                                          newPos,
                                          SpatialGraphSelection(graph),
                                          SpatialGraphSelection(graph),
                                          labels);

        op.exec();
        op.undo();

        EXPECT_EQ(graph->getNumVertices(), modifiedGraph->getNumVertices());
        EXPECT_EQ(graph->getNumEdges(), modifiedGraph->getNumEdges());
        EXPECT_EQ(graph->getTotalNumEdgePoints(), modifiedGraph->getTotalNumEdgePoints());
    }
    catch (McException& e)
    {
        qDebug() << "\n Error in FilopodiaOperationTest AddFilopodiaVertex: \n" << e.what();
        EXPECT_TRUE(false);
    }
}

TEST_F(FilopodiaOperationTest, AddFilopodiaEdge)
{
    // Edges need only 5 labels -> remove type label and match label
    labels.remove(2);
    labels.remove(5);

    // Prepare operation
    SpatialGraphSelection sel(graph);
    sel.clear();
    sel.selectVertex(v5);
    sel.selectVertex(v6);

    std::vector<McVec3f> edgePoints;
    edgePoints.push_back(graph->getVertexCoords(v5));
    edgePoints.push_back(graph->getVertexCoords(v6));

    McHandle<HxSpatialGraph> modifiedGraph = graph->duplicate();

    // Execute operation
    try {
        AddFilopodiaEdgeOperationSet op(modifiedGraph,
                                        sel,
                                        SpatialGraphSelection(graph),
                                        edgePoints,
                                        labels);

        op.exec();
        op.undo();

        EXPECT_EQ(graph->getNumVertices(), modifiedGraph->getNumVertices());
        EXPECT_EQ(graph->getNumEdges(), modifiedGraph->getNumEdges());
        EXPECT_EQ(graph->getTotalNumEdgePoints(), modifiedGraph->getTotalNumEdgePoints());
    }
    catch (McException& e)
    {
        qDebug() << "\n Error in FilopodiaOperationTest AddFilopodiaEdge: \n" << e.what();
        EXPECT_TRUE(false);
    }
}

TEST_F(FilopodiaOperationTest, ReplaceFilopodiaEdge)
{
    // Edges need only 5 labels -> remove type label and match label
    labels.remove(2);
    labels.remove(5);

    // Prepare operation
    const McVec3f newPoint(6.0f, 4.0f, 0.0f);

    std::vector<McVec3f> newEdgePoints;
    newEdgePoints.push_back(graph->getVertexCoords(v1));
    newEdgePoints.push_back(newPoint);
    newEdgePoints.push_back(graph->getVertexCoords(v7));

    SpatialGraphSelection sel(graph);
    sel.clear();
    sel.selectVertex(v1);
    sel.selectVertex(v7);

    SpatialGraphSelection selToDelete(graph);
    selToDelete.clear();
    selToDelete.selectEdge(e1);

    McHandle<HxSpatialGraph> modifiedGraph = graph->duplicate();

    // Execute operation
    try
    {
        ReplaceFilopodiaEdgeOperationSet op(modifiedGraph,
                                            sel,
                                            SpatialGraphSelection(graph),
                                            newEdgePoints,
                                            labels,
                                            selToDelete);

        op.exec();
        op.undo();

        EXPECT_EQ(graph->getNumVertices(), modifiedGraph->getNumVertices());
        EXPECT_EQ(graph->getNumEdges(), modifiedGraph->getNumEdges());
        EXPECT_EQ(graph->getTotalNumEdgePoints(), modifiedGraph->getTotalNumEdgePoints());
    }
    catch (McException& e)
    {
        qDebug() << "\n Error in FilopodiaOperationTest ReplaceFilopodiaEdge: \n" << e.what();
        EXPECT_TRUE(false);
    }
}

TEST_F(FilopodiaOperationTest, DeleteFilopodia)
{
    // Prepare operation
    SpatialGraphSelection sel1(graph);
    sel1.clear();
    sel1.selectEdge(e1);
    sel1.selectEdge(e2);
    sel1.selectVertex(v1);
    sel1.selectVertex(v2);
    sel1.selectVertex(v7);

    McHandle<HxSpatialGraph> modifiedGraph = graph->duplicate();

    // Execute operation
    try {
        DeleteFilopodiaOperationSet op1(modifiedGraph,
                                        sel1,
                                        SpatialGraphSelection(graph));

        op1.exec();
        op1.undo();

        EXPECT_EQ(graph->getNumVertices(), modifiedGraph->getNumVertices());
        EXPECT_EQ(graph->getNumEdges(), modifiedGraph->getNumEdges());
        EXPECT_EQ(graph->getTotalNumEdgePoints(), modifiedGraph->getTotalNumEdgePoints());
    }
    catch (McException& e)
    {
        qDebug() << "\n Error in FilopodiaOperationTest DeleteFilopodia: \n" << e.what();
        EXPECT_TRUE(false);
    }

    // Prepare operation
    SpatialGraphSelection sel2(graph);
    sel2.clear();
    sel2.selectEdge(e5);
    sel2.selectVertex(v3);
    sel2.selectVertex(v11);

    // Execute operation
    try
    {
        DeleteFilopodiaOperationSet op2(modifiedGraph,
                                        sel2,
                                        SpatialGraphSelection(graph));

        op2.exec();
        op2.undo();

        EXPECT_EQ(graph->getNumVertices(), modifiedGraph->getNumVertices());
        EXPECT_EQ(graph->getNumEdges(), modifiedGraph->getNumEdges());
        EXPECT_EQ(graph->getTotalNumEdgePoints(), modifiedGraph->getTotalNumEdgePoints());
    }
    catch (McException& e)
    {
        qDebug() << "\n Error in FilopodiaOperationTest DeleteFilopodia: \n" << e.what();
        EXPECT_TRUE(false);
    }
}

TEST_F(FilopodiaOperationTest, MergeFilopodia)
{
    // Prepare edge and node labels
    std::vector<McDArray<int> > newNodeLabels;
    std::vector<McDArray<int> > newEdgeLabels;

    const McDArray<int> nodeLabels = labels;
    McDArray<int> edgeLabels = labels;
    edgeLabels.remove(2);
    edgeLabels.remove(5);

    const McVec3f p1(7.0f, 7.0f, 0.0f);
    const McVec3f p2(6.0f, 3.5f, 0.0f);

    // New nodes
    std::vector<McVec3f> newNodes;
    newNodes.push_back(p1);
    newNodeLabels.push_back(nodeLabels);
    newNodes.push_back(p2);
    newNodeLabels.push_back(nodeLabels);

    // New edges
    std::vector<McVec3f> edgeTip;
    edgeTip.push_back(p1);
    edgeTip.push_back(p2);

    std::vector<McVec3f> edgeRoot;
    edgeRoot.push_back(p2);
    edgeRoot.push_back(graph->getVertexCoords(v1));

    std::vector<McVec3f> edgePoint;
    edgePoint.push_back(p2);
    edgePoint.push_back(graph->getEdgePoint(e1, 1));

    McDArray<int> newEdgeSources;
    newEdgeSources.push_back(0);
    newEdgeSources.push_back(1);

    McDArray<int> newEdgeTargets;
    newEdgeTargets.push_back(1);
    newEdgeTargets.push_back(-1);

    newEdgeLabels.push_back(edgeLabels);
    newEdgeLabels.push_back(edgeLabels);

    McHandle<HxSpatialGraph> modifiedGraph = graph->duplicate();

    // Prepare connection with root
    SpatialGraphSelection rootSel(graph);
    rootSel.clear();
    rootSel.selectVertex(v1);

    std::vector<std::vector<McVec3f> > newRootEdges;
    newRootEdges.push_back(edgeTip);
    newRootEdges.push_back(edgeRoot);

    // Execute connect with root operation
    try
    {
        MergeFilopodiaOperationSet connectRootOp(modifiedGraph,
                                                 rootSel,
                                                 SpatialGraphSelection(graph),
                                                 newNodes,
                                                 newNodeLabels,
                                                 newEdgeSources,
                                                 newEdgeTargets,
                                                 newRootEdges,
                                                 newEdgeLabels);

        connectRootOp.exec();
        connectRootOp.undo();

        EXPECT_EQ(graph->getNumVertices(), modifiedGraph->getNumVertices());
        EXPECT_EQ(graph->getNumEdges(), modifiedGraph->getNumEdges());
        EXPECT_EQ(graph->getTotalNumEdgePoints(), modifiedGraph->getTotalNumEdgePoints());
    }
    catch (McException& e)
    {
        qDebug() << "\n Error in FilopodiaOperationTest MergeFilopodia: \n" << e.what();
        EXPECT_TRUE(false);
    }

    // Prepare connection with point
    SpatialGraphSelection pointSel(graph);
    pointSel.clear();
    pointSel.selectPoint(SpatialGraphPoint(e1, 1));

    std::vector<std::vector<McVec3f> > newPointEdges;
    newPointEdges.push_back(edgeTip);
    newPointEdges.push_back(edgePoint);

    McDArray<int> connectLabels = labels;

    // Execute connect with point operation
    try
    {
        MergeFilopodiaOperationSet connectPointOp(modifiedGraph,
                                                  pointSel,
                                                  SpatialGraphSelection(graph),
                                                  newNodes,
                                                  newNodeLabels,
                                                  newEdgeSources,
                                                  newEdgeTargets,
                                                  newPointEdges,
                                                  newEdgeLabels,
                                                  connectLabels);

        connectPointOp.exec();
        connectPointOp.undo();

        EXPECT_EQ(graph->getNumVertices(), modifiedGraph->getNumVertices());
        EXPECT_EQ(graph->getNumEdges(), modifiedGraph->getNumEdges());
        EXPECT_EQ(graph->getTotalNumEdgePoints(), modifiedGraph->getTotalNumEdgePoints());
    }
    catch (McException& e)
    {
        qDebug() << "\n Error in FilopodiaOperationTest MergeFilopodia: \n" << e.what();
        EXPECT_TRUE(false);
    }
}

TEST_F(FilopodiaOperationTest, AddFilopodium)
{
    // Prepare edge and node labels
    std::vector<McDArray<int> > newNodeLabels;
    std::vector<McDArray<int> > newEdgeLabels;

    const McDArray<int> nodeLabels = labels;
    McDArray<int> edgeLabels = labels;
    edgeLabels.remove(2);
    edgeLabels.remove(5);

    const McVec3f p1(7.0f, 7.0f, 0.0f);
    const McVec3f p2(6.0f, 3.5f, 0.0f);

    // New nodes
    std::vector<McVec3f> newNodes;
    newNodes.push_back(p1);
    newNodeLabels.push_back(nodeLabels);
    newNodes.push_back(p2);
    newNodeLabels.push_back(nodeLabels);

    // New edges
    std::vector<McVec3f> edgeTip;
    edgeTip.push_back(p1);
    edgeTip.push_back(p2);

    std::vector<McVec3f> edgeRoot;
    edgeRoot.push_back(p2);
    edgeRoot.push_back(graph->getVertexCoords(v1));

    std::vector<McVec3f> edgePoint;
    edgePoint.push_back(p2);
    edgePoint.push_back(graph->getEdgePoint(e1, 1));

    McDArray<int> newEdgeSources;
    newEdgeSources.push_back(0);
    newEdgeSources.push_back(1);

    McDArray<int> newEdgeTargets;
    newEdgeTargets.push_back(1);
    newEdgeTargets.push_back(-1);

    newEdgeLabels.push_back(edgeLabels);
    newEdgeLabels.push_back(edgeLabels);

    McHandle<HxSpatialGraph> modifiedGraph = graph->duplicate();

    // Prepare connection with root
    SpatialGraphSelection rootSel(graph);
    rootSel.clear();
    rootSel.selectVertex(v1);

    std::vector<std::vector<McVec3f> > newRootEdges;
    newRootEdges.push_back(edgeTip);
    newRootEdges.push_back(edgeRoot);

    // Execute connect with root operation
    try {
        AddFilopodiumOperationSet connectRootOp(modifiedGraph,
                                                rootSel,
                                                SpatialGraphSelection(graph),
                                                newNodes,
                                                newNodeLabels,
                                                newEdgeSources,
                                                newEdgeTargets,
                                                newRootEdges,
                                                newEdgeLabels,
                                                McDArray<int>());

        connectRootOp.exec();
        connectRootOp.undo();

        EXPECT_EQ(graph->getNumVertices(), modifiedGraph->getNumVertices());
        EXPECT_EQ(graph->getNumEdges(), modifiedGraph->getNumEdges());
        EXPECT_EQ(graph->getTotalNumEdgePoints(), modifiedGraph->getTotalNumEdgePoints());
    }
    catch (McException& e)
    {
        qDebug() << "\n Error in FilopodiaOperationTest AddFilopodium: \n" << e.what();
        EXPECT_TRUE(false);
    }

    // Prepare connection with point
    SpatialGraphSelection pointSel(graph);
    pointSel.clear();
    pointSel.selectPoint(SpatialGraphPoint(e1, 1));

    std::vector<std::vector<McVec3f> > newPointEdges;
    newPointEdges.push_back(edgeTip);
    newPointEdges.push_back(edgePoint);

    McDArray<int> connectLabels = labels;

    // Execute connect with point operation
    try {
        AddFilopodiumOperationSet connectPointOp(modifiedGraph,
                                                 pointSel,
                                                 SpatialGraphSelection(graph),
                                                 newNodes,
                                                 newNodeLabels,
                                                 newEdgeSources,
                                                 newEdgeTargets,
                                                 newPointEdges,
                                                 newEdgeLabels,
                                                 connectLabels);

        connectPointOp.exec();
        connectPointOp.undo();

        EXPECT_EQ(graph->getNumVertices(), modifiedGraph->getNumVertices());
        EXPECT_EQ(graph->getNumEdges(), modifiedGraph->getNumEdges());
        EXPECT_EQ(graph->getTotalNumEdgePoints(), modifiedGraph->getTotalNumEdgePoints());
    }
    catch (McException& e)
    {
        qDebug() << "\n Error in FilopodiaOperationTest AddFilopodium: \n" << e.what();
        EXPECT_TRUE(false);
    }
}

TEST_F(FilopodiaOperationTest, MoveTip)
{
    McVec3f newPos(9.0f, 6.0f, 0.0f);

    std::vector<McDArray<int> > newNodeLabels;
    std::vector<McDArray<int> > newEdgeLabels;

    const McDArray<int> nodeLabels = labels;
    McDArray<int> edgeLabels = labels;
    edgeLabels.remove(2);
    edgeLabels.remove(5);

    // New nodes
    std::vector<McVec3f> newNodes;
    newNodes.push_back(newPos);

    newNodeLabels.push_back(nodeLabels);

    // New edges
    std::vector<McVec3f> edgeTip;
    edgeTip.push_back(newPos);
    edgeTip.push_back(graph->getVertexCoords(v7));

    McDArray<int> newEdgeSources;
    newEdgeSources.push_back(0);

    McDArray<int> newEdgeTargets;
    newEdgeTargets.push_back(-1);

    newEdgeLabels.push_back(edgeLabels);

    std::vector<std::vector<McVec3f> > newRootEdges;
    newRootEdges.push_back(edgeTip);

    // Old elements to delete
    SpatialGraphSelection selToDelete(graph);
    selToDelete.selectEdge(e2);
    selToDelete.selectVertex(v2);

    McHandle<HxSpatialGraph> modifiedGraph = graph->duplicate();

    // Connection
    SpatialGraphSelection sel(graph);
    sel.selectVertex(v7);

    // Execute operation
    try {
        MoveTipOperationSet rootOp(modifiedGraph,
                                   sel,
                                   SpatialGraphSelection(graph),
                                   newNodes,
                                   newNodeLabels,
                                   newEdgeSources,
                                   newEdgeTargets,
                                   newRootEdges,
                                   newEdgeLabels,
                                   selToDelete);
        rootOp.exec();
        rootOp.undo();

        EXPECT_EQ(graph->getNumVertices(), modifiedGraph->getNumVertices());
        EXPECT_EQ(graph->getNumEdges(), modifiedGraph->getNumEdges());
        EXPECT_EQ(graph->getTotalNumEdgePoints(), modifiedGraph->getTotalNumEdgePoints());
    }
    catch (McException& e)
    {
        qDebug() << "\n Error in FilopodiaOperationTest MoveTip: \n" << e.what();
        EXPECT_TRUE(false);
    }
}

TEST_F(FilopodiaOperationTest, MoveTipAlongEdge)
{
    // Prepare operation
    SpatialGraphSelection sel(graph);
    sel.clear();
    sel.selectVertex(v2);
    sel.selectPoint(SpatialGraphPoint(e2,1));

    McHandle<HxSpatialGraph> modifiedGraph = graph->duplicate();

    try {
        MoveTipAlongEdgeOperationSet op(modifiedGraph,
                                        sel,
                                        SpatialGraphSelection(graph),
                                        labels);

        op.exec();
        op.undo();

        EXPECT_EQ(graph->getNumVertices(), modifiedGraph->getNumVertices());
        EXPECT_EQ(graph->getNumEdges(), modifiedGraph->getNumEdges());
        EXPECT_EQ(graph->getTotalNumEdgePoints(), modifiedGraph->getTotalNumEdgePoints());
    }
    catch (McException& e)
    {
        qDebug() << "\n Error in FilopodiaOperationTest MoveTipAlongEdge: \n" << e.what();
        EXPECT_TRUE(false);
    }
}

TEST_F(FilopodiaOperationTest, MoveBase)
{
    McVec3f newPos(5.0f, 6.0f, 0.0f);

    std::vector<McDArray<int> > newNodeLabels;
    std::vector<McDArray<int> > newEdgeLabels;

    const McDArray<int> nodeLabels = labels;
    McDArray<int> edgeLabels = labels;
    edgeLabels.remove(2);
    edgeLabels.remove(5);

    // New nodes
    std::vector<McVec3f> newNodes;
    newNodes.push_back(graph->getVertexCoords(v2));
    newNodes.push_back(newPos);

    newNodeLabels.push_back(nodeLabels);
    newNodeLabels.push_back(nodeLabels);

    // New edges
    std::vector<McVec3f> edgeTip;
    edgeTip.push_back(graph->getVertexCoords(v2));
    edgeTip.push_back(newPos);

    std::vector<McVec3f> edgeRoot;
    edgeRoot.push_back(newPos);
    edgeRoot.push_back(graph->getVertexCoords(v1));

    std::vector<McVec3f> edgePoint;
    edgePoint.push_back(newPos);
    edgePoint.push_back(graph->getEdgePoint(e7,1));

    McDArray<int> newEdgeSources;
    newEdgeSources.push_back(0);
    newEdgeSources.push_back(1);

    McDArray<int> newEdgeTargets;
    newEdgeTargets.push_back(1);
    newEdgeTargets.push_back(-1);

    newEdgeLabels.push_back(edgeLabels);
    newEdgeLabels.push_back(edgeLabels);

    // Old elements to delete
    SpatialGraphSelection selToDelete(graph);
    selToDelete.selectEdge(e1);
    selToDelete.selectEdge(e2);
    selToDelete.selectVertex(v2);
    selToDelete.selectVertex(v3);

    McHandle<HxSpatialGraph> modifiedGraph = graph->duplicate();

    // Prepare connection with root
    SpatialGraphSelection rootSel(graph);
    rootSel.selectVertex(v1);

    std::vector<std::vector<McVec3f> > newRootEdges;
    newRootEdges.push_back(edgeTip);
    newRootEdges.push_back(edgeRoot);

    // Execute operation with root connection
    try
    {
        MoveBaseOperationSet rootOp(modifiedGraph,
                                    rootSel,
                                    SpatialGraphSelection(graph),
                                    newNodes,
                                    newNodeLabels,
                                    newEdgeSources,
                                    newEdgeTargets,
                                    newRootEdges,
                                    newEdgeLabels,
                                    selToDelete);
        rootOp.exec();
        rootOp.undo();

        EXPECT_EQ(graph->getNumVertices(), modifiedGraph->getNumVertices());
        EXPECT_EQ(graph->getNumEdges(), modifiedGraph->getNumEdges());
        EXPECT_EQ(graph->getTotalNumEdgePoints(), modifiedGraph->getTotalNumEdgePoints());
    }
    catch (McException& e)
    {
        qDebug() << "\n Error in FilopodiaOperationTest MoveBase: \n" << e.what();
        EXPECT_TRUE(false);
    }

    // Prepare connection with point
    SpatialGraphSelection pointSel(graph);
    pointSel.selectPoint(SpatialGraphPoint(e7, 1));

    std::vector<std::vector<McVec3f> > newPointEdges;
    newPointEdges.push_back(edgeTip);
    newPointEdges.push_back(edgePoint);

    // Execute operation with point connection
    try
    {
        MoveBaseOperationSet pointOp(modifiedGraph,
                                     pointSel,
                                     SpatialGraphSelection(graph),
                                     newNodes,
                                     newNodeLabels,
                                     newEdgeSources,
                                     newEdgeTargets,
                                     newPointEdges,
                                     newEdgeLabels,
                                     selToDelete,
                                     labels);
        pointOp.exec();
        pointOp.undo();

        EXPECT_EQ(graph->getNumVertices(), modifiedGraph->getNumVertices());
        EXPECT_EQ(graph->getNumEdges(), modifiedGraph->getNumEdges());
        EXPECT_EQ(graph->getTotalNumEdgePoints(), modifiedGraph->getTotalNumEdgePoints());
    }
    catch (McException& e)
    {
        qDebug() << "\n Error in FilopodiaOperationTest MoveBase: \n" << e.what();
        EXPECT_TRUE(false);
    }
}

TEST_F(FilopodiaOperationTest, MoveBaseAlongEdge)
{
    // Prepare operation
    SpatialGraphSelection sel(graph);
    sel.clear();
    sel.selectVertex(v7);
    sel.selectPoint(SpatialGraphPoint(e1,1));

    McHandle<HxSpatialGraph> modifiedGraph = graph->duplicate();

    try
    {
        MoveBaseAlongEdgeOperationSet op(modifiedGraph, sel, SpatialGraphSelection(graph), labels);

        op.exec();
        op.undo();

        EXPECT_EQ(graph->getNumVertices(), modifiedGraph->getNumVertices());
        EXPECT_EQ(graph->getNumEdges(), modifiedGraph->getNumEdges());
        EXPECT_EQ(graph->getTotalNumEdgePoints(), modifiedGraph->getTotalNumEdgePoints());
    }
    catch (McException& e)
    {
        qDebug() << "\n Error in FilopodiaOperationTest MoveBaseAlongEdge: \n" << e.what();
        EXPECT_TRUE(false);
    }
}

TEST_F(FilopodiaOperationTest, ConnectNode)
{
    std::vector<McDArray<int> > newNodeLabels;
    std::vector<McDArray<int> > newEdgeLabels;
    McDArray<int> edgeLabels = labels;
    edgeLabels.remove(2);
    edgeLabels.remove(5);

    // Graph connection
    SpatialGraphSelection connectSel(graph);
    connectSel.clear();
    connectSel.selectPoint(SpatialGraphPoint(e6,1));

    // Old tip and edge to delete
    SpatialGraphSelection selToDelete(graph);
    selToDelete.selectVertex(v3);
    selToDelete.selectEdge(e5);

    // New node
    std::vector<McVec3f> newNodes;
    newNodes.push_back(graph->getVertexCoords(v3));

    newNodeLabels.push_back(labels);

    // New edge
    std::vector<McVec3f> edgeTip;
    edgeTip.push_back(graph->getVertexCoords(v3));
    edgeTip.push_back(graph->getEdgePoint(e6, 1));

    McDArray<int> newEdgeSources;
    newEdgeSources.push_back(0);

    McDArray<int> newEdgeTargets;
    newEdgeTargets.push_back(-1);

    std::vector<std::vector<McVec3f> > newEdgePoints;
    newEdgePoints.push_back(edgeTip);

    newEdgeLabels.push_back(edgeLabels);

    McHandle<HxSpatialGraph> modifiedGraph = graph->duplicate();

    // Execute operation
    try
    {
        ConnectOperationSet op(graph,
                               connectSel,
                               SpatialGraphSelection(graph),
                               newNodes,
                               newNodeLabels,
                               newEdgeSources,
                               newEdgeTargets,
                               newEdgePoints,
                               newEdgeLabels,
                               labels,
                               selToDelete);
        op.exec();
        op.undo();

        EXPECT_EQ(graph->getNumVertices(), modifiedGraph->getNumVertices());
        EXPECT_EQ(graph->getNumEdges(), modifiedGraph->getNumEdges());
        EXPECT_EQ(graph->getTotalNumEdgePoints(), modifiedGraph->getTotalNumEdgePoints());
    }
    catch (McException& e)
    {
        qDebug() << "\n Error in FilopodiaOperationTest ConnectNode: \n" << e.what();
        EXPECT_TRUE(false);
    }

}


TEST_F(FilopodiaOperationTest, ReplaceFilopodia)
{
    // Prepare edge and node labels
    std::vector<McDArray<int> > newNodeLabels;
    std::vector<McDArray<int> > newEdgeLabels;

    McDArray<int> nodeLabels = labels;
    nodeLabels[5] = FilopodiaFunctions::getNextAvailableManualNodeMatchId(graph);

    McDArray<int> edgeLabels = labels;
    edgeLabels.remove(2);
    edgeLabels.remove(5);

    const McVec3f p1(7.0f, 7.0f, 0.0f);
    const McVec3f p2(6.0f, 6.0f, 0.0f);

    McDArray<int> previousBases;
    previousBases.push_back(v7);
    previousBases.push_back(v7);

    // New nodes
    std::vector<McVec3f> newNodes;
    newNodes.push_back(p1);
    newNodeLabels.push_back(nodeLabels);
    newNodes.push_back(p2);
    newNodeLabels.push_back(nodeLabels);

    // New edges
    std::vector<McVec3f> edgeTip;
    edgeTip.push_back(p1);
    edgeTip.push_back(p2);

    std::vector<McVec3f> edgeRoot;
    edgeRoot.push_back(p2);
    edgeRoot.push_back(graph->getVertexCoords(v1));

    McDArray<int> newEdgeSources;
    newEdgeSources.push_back(0);
    newEdgeSources.push_back(1);

    McDArray<int> newEdgeTargets;
    newEdgeTargets.push_back(1);
    newEdgeTargets.push_back(-1);

    newEdgeLabels.push_back(edgeLabels);
    newEdgeLabels.push_back(edgeLabels);

    // Old elements
    SpatialGraphSelection selToDelete(graph);
    selToDelete.selectVertex(v2);
    selToDelete.selectVertex(v7);
    selToDelete.selectEdge(e1);
    selToDelete.selectEdge(e2);

    McHandle<HxSpatialGraph> modifiedGraph = graph->duplicate();
    // Prepare connection with root
    SpatialGraphSelection rootSel(graph);
    rootSel.clear();
    rootSel.selectVertex(v1);

    std::vector<std::vector<McVec3f> > newRootEdges;
    newRootEdges.push_back(edgeTip);
    newRootEdges.push_back(edgeRoot);

    // Execute connect with root operation
    try
    {
        ReplaceFilopodiaOperationSet connectRootOp(graph,
                                                   rootSel,
                                                   SpatialGraphSelection(graph),
                                                   newNodes,
                                                   newNodeLabels,
                                                   previousBases,
                                                   newEdgeSources,
                                                   newEdgeTargets,
                                                   newRootEdges,
                                                   newEdgeLabels,
                                                   selToDelete);

        connectRootOp.exec();
        connectRootOp.undo();

        EXPECT_EQ(graph->getNumVertices(), modifiedGraph->getNumVertices());
        EXPECT_EQ(graph->getNumEdges(), modifiedGraph->getNumEdges());
        EXPECT_EQ(graph->getTotalNumEdgePoints(), modifiedGraph->getTotalNumEdgePoints());
    }
    catch (McException& e)
    {
        qDebug() << "\n Error in FilopodiaOperationTest ReplaceFilopodia: \n" << e.what();
        EXPECT_TRUE(false);
    }
}
