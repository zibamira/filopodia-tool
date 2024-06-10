#include "HxTraceDijkstraMap.h"
#include "FilopodiaFunctions.h"
#include <hxfield/HxUniformScalarField3.h>
#include <mclib/McException.h>
#include <hxcore/HxObjectPool.h>

#include "ConvertVectorAndMcDArray.h"

HX_INIT_CLASS(HxTraceDijkstraMap,HxCompModule)


HxTraceDijkstraMap::HxTraceDijkstraMap()
    : HxCompModule(HxSpatialGraph::getClassTypeId())
    , portDijkstra(this, tr("dijkstrapriormap"), tr("Dijkstra prior map"), HxUniformLabelField3::getClassTypeId())
    , portAction(this, tr("action"), tr("Action"))
{
}

HxTraceDijkstraMap::~HxTraceDijkstraMap()
{
}

HxSpatialGraph* getNodesOfGraphInTime(const HxSpatialGraph* graph, const int time=-1) {

    // Returns graph with all nodes
    // If graph has time attribute "TimeStep" only graph with nodes of time will be returned
    // If graph has type attribute "NodeType" only graph with tips and root node will be returned

    SpatialGraphSelection sel(graph);
    sel.selectAllVertices();

    if (time != -1) {
        sel = FilopodiaFunctions::getSelectionForTimeStep(graph, time);
    }

    if (FilopodiaFunctions::checkIfGraphHasAttribute(graph, FilopodiaFunctions::getTypeLabelAttributeName())) {
        SpatialGraphSelection branchSel = FilopodiaFunctions::getNodesOfTypeInSelection(graph, sel, BRANCHING_NODE);
        SpatialGraphSelection baseSel = FilopodiaFunctions::getNodesOfTypeInSelection(graph,sel, BASE_NODE);
        sel.subtractSelection(branchSel);
        sel.subtractSelection(baseSel);
    }

    return graph->getSubgraph(sel);

}

int getCenterNode(HxSpatialGraph* graph,
                  const McHandle<HxUniformScalarField3> priorMap)
{
    // Find voxel in prior map with prior = (0,0,0)
    // This is the location of the point the shortest path was traced to

    McHandle<HxCoord3> coords = priorMap->lattice().coords();

    unsigned char centerDirection = FilopodiaFunctions::vectorToDirection(McVec3i(0,0,0));
    const McDim3l& dims = priorMap->lattice().getDims();
    McVec3i centerPos = McVec3i(-1,-1,-1);

    for (int x=0; x<dims.nx; ++x) {
        for (int y=0; y<dims.ny; ++y) {
            for (int z=0; z<dims.nz; ++z) {
                int value;
                priorMap->lattice().evalNative(x, y, z, &value);
                if (value == centerDirection) {
                    centerPos = McVec3i(x,y,z);
                }
            }
        }
    }

    if (centerPos == McVec3i(-1,-1,-1)) {
        throw McException("Tracing error: center position not found.");
    }

    const McVec3f pos = coords->pos(centerPos);
    const McVec3f voxelSize = priorMap->getVoxelSize();

    // Check if graph has node at center position
    // Create new node if no center node was found
    for (int v=0; v<graph->getNumVertices(); ++v) {
        const McVec3f nodeCoords = graph->getVertexCoords(v);
        if ((abs(nodeCoords.x-pos.x) < 0.5*voxelSize.x) &&
            (abs(nodeCoords.y-pos.y) < 0.5*voxelSize.y) &&
            (abs(nodeCoords.z-pos.z) < 0.5*voxelSize.z)) {
            printf("Get center node of dijkstra map: Center node %i already exists.\n", v);
            return v;
        } else {
            printf("Get center node of dijkstra map: Add new center node.\n");
            return graph->addVertex(pos);
        }
    }

    return -1;

}

void traceEndnodesToRoot(const McHandle<HxUniformScalarField3> priorMap,
                         HxSpatialGraph* graph)
{

    const int centerNode = getCenterNode(graph, priorMap);

    if (centerNode == -1)
    {
        graph->clear();
        throw McException("Cannot trace nodes to center: no center node found.");
    }

    const McVec3f centerPoint = graph->getVertexCoords(centerNode);

    for (int v = 0; v < graph->getNumVertices(); ++v)
    {
        if (v == centerNode) continue;

        const IncidenceList incidentEdgeList = graph->getIncidentEdges(v);
        if (incidentEdgeList.size() == 0)
        {
            const McVec3f startPoint = graph->getVertexCoords(v);
            SpatialGraphPoint intersectionPoint(-1, -1);
            int intersectionNode = -1;
            SpatialGraphSelection sel(graph);
            sel.selectAllEdges();
            sel.selectAllVertices();
            const std::vector<McVec3f> newPoints = FilopodiaFunctions::traceWithDijkstra(graph,
                                                                                      priorMap,
                                                                                      startPoint,
                                                                                      centerPoint,
                                                                                      sel,
                                                                                      intersectionPoint,
                                                                                      intersectionNode);

            int source = v;
            int target = centerNode;

            if (intersectionNode != -1)
            {
                printf("intersection node\n");
                target = intersectionNode;
            }
            else if (intersectionPoint != SpatialGraphPoint(-1, -1))
            {
                printf("intersectionPoint\n");

                // Convert point to vertex op:
                SpatialGraphSelection s1(graph);
                s1.selectPoint(intersectionPoint);
                PointToVertexOperation* pointToVertexOp = new PointToVertexOperation(graph, s1, SpatialGraphSelection(graph));
                pointToVertexOp->exec();

                target = pointToVertexOp->getNewVertexNum();

            }

            graph->addEdge(source, target, newPoints);
            }

    }
}

void HxTraceDijkstraMap::compute() {

    if (!portAction.wasHit())
    {
        return;
    }

    const HxSpatialGraph* inputGraph = hxconnection_cast<HxSpatialGraph> (portData);
    if(!inputGraph) {
        throw McException(QString("No input graph"));
    }

    McHandle<HxUniformScalarField3> priorMap = hxconnection_cast<HxUniformLabelField3>(portDijkstra);
    if (!priorMap) {
        throw McException(QString("Please connect dijkstra prior map"));
    }

    McHandle<HxSpatialGraph> outputGraph = dynamic_cast<HxSpatialGraph*>(getResult());
    if (!outputGraph) {
        outputGraph = HxSpatialGraph::createInstance();
    }

    if (FilopodiaFunctions::checkIfGraphHasAttribute(inputGraph, FilopodiaFunctions::getTimeStepAttributeName())) {
        const QString filename = priorMap->getLabel();
        const int time = FilopodiaFunctions::getTimeFromFileName(filename);
        outputGraph->copyFrom(getNodesOfGraphInTime(inputGraph, time));
        outputGraph->setLabel(QString("tracedGraph_t%1").arg(time));
    } else {
        outputGraph->copyFrom(getNodesOfGraphInTime(inputGraph));
        outputGraph->setLabel("tracedGraph");
    }

    try {
        traceEndnodesToRoot(priorMap, outputGraph);
    }
    catch (McException& e){
        HxMessage::error(QString("Could not trace nodes to center:\n%1.").arg(e.what()), "ok" );
    }

    setResult(outputGraph);

}
