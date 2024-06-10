#include "HxClipSpatialGraphByThickness.h"
#include "FilopodiaFunctions.h"
#include <hxspatialgraph/internal/HxSpatialGraph.h>
#include <mclib/McException.h>


HX_INIT_CLASS(HxClipSpatialGraphByThickness,HxCompModule)

HxClipSpatialGraphByThickness::HxClipSpatialGraphByThickness() :
    HxCompModule(HxSpatialGraph::getClassTypeId()),
    portAction(this, tr("action"), tr("Action"))
{
}

HxClipSpatialGraphByThickness::~HxClipSpatialGraphByThickness()
{
}


void findEndNodes(HxSpatialGraph* graph, std::vector<int>& endNodes,const int time ) {

    const char* nodeAttName = "NodeType";
    EdgeVertexAttribute* nodeAtt = graph->findVertexAttribute(nodeAttName);
    if (!nodeAtt) {
        throw McException(QString("No vertex attribute found with name %1").arg(QString::fromLatin1(nodeAttName)));
    }

    const int timeLabelId = FilopodiaFunctions::getTimeIdFromTimeStep(graph, time);

    for (int v=0; v<graph->getNumVertices(); ++v) {

        const int currentTimeLabelId = FilopodiaFunctions::getTimeIdOfNode(graph, v);

        if ( FilopodiaFunctions::hasNodeType(graph, TIP_NODE, v) && (currentTimeLabelId == timeLabelId) ) {
            endNodes.push_back(v);
        }

    }
}


void findEdgesFromEndToRoot(HxSpatialGraph* graph, const int endNode, const int rootNode, std::vector<int>& edges) {

    for (int e=0; e<graph->getNumEdges();++e) {

        if (graph->getEdgeSource(e) == endNode) {
            edges.push_back(e);
            int newNode = graph->getEdgeTarget(e);

            if (newNode == rootNode) {
                return;
            } else {
                findEdgesFromEndToRoot(graph, newNode, rootNode, edges);
            }
        }
    }
}


std::vector<float> calculateThicknessDependingDistance(const HxSpatialGraph* graph, const std::vector<int>& edgesFromEndToRoot) {

    const char* PointAttributeName = "thickness";
    PointAttribute* pAtt = graph->findEdgePointAttribute(PointAttributeName);
    if (!pAtt) {
        throw McException(QString("Graph has no attribute %1").arg(QString::fromLatin1(PointAttributeName)));
    }

    std::vector<float> thick;
    for (int e=edgesFromEndToRoot.size()-1; e>=0; --e) {
        const int edge = edgesFromEndToRoot[e];

        for (int p=graph->getNumEdgePoints(edge)-1; p>=0; --p) {
            thick.push_back(pAtt->getFloatDataAtPoint(edge,p));
        }
    }

    return thick;
}


std::vector<float> calculateDistanceToRoot(HxSpatialGraph* graph, const std::vector<int>& edgesFromEndToRoot, const int rootNode) {

    const char* PointAttributeName2 = "pointDistToRoot";
    PointAttribute* pAtt2 = graph->addPointAttribute(PointAttributeName2, McPrimType::MC_FLOAT, 1);

    std::vector<float> pointDist;

    float oldDistance = 0.0f;
    McVec3f oldCoords = graph->getVertexCoords(rootNode);

    for (int e=edgesFromEndToRoot.size()-1; e>=0; --e) {

        const int edge = edgesFromEndToRoot[e];

        for (int p=graph->getNumEdgePoints(edge)-1; p>=0; --p) {

            const McVec3f newCoords = graph->getEdgePoint(edge, p);
            const float dst = (newCoords - oldCoords).length();

            oldCoords = newCoords;
            oldDistance = oldDistance + dst;
            pointDist.push_back(oldDistance);
            pAtt2->setFloatDataAtPoint(oldDistance, edge, p);
        }
    }

    return pointDist;
}


SpatialGraphPoint findIntersectionPoint(HxSpatialGraph* graph, const std::vector<int>& edgesFromEndToRoot) {

    const char* PointAttributeName = "thickness";
    PointAttribute* pAtt = graph->findEdgePointAttribute(PointAttributeName);
    if (!pAtt) {
        throw McException(QString("Graph has no attribute %1").arg(QString::fromLatin1(PointAttributeName)));
    }

    // Naiv thresholding for starting point detection:
    const float threshold = 0.4;
    int iPoint = -1;
    int iEdge = -1;

    for ( int e=0; e<edgesFromEndToRoot.size(); ++e ) {
        const int edgeNum = edgesFromEndToRoot[e];

        for ( int p=1; p<graph->getNumEdgePoints(edgeNum)-1; ++p ) {
            const float pointThick = pAtt->getFloatDataAtPoint(edgeNum, p);

            if ( (pointThick > threshold) & (iPoint == -1) ){
                iPoint = p;
                iEdge = edgeNum;
            }
        }
    }
    return SpatialGraphPoint(iEdge, iPoint);

}


SpatialGraphPoint findNextPointToConvert(const HxSpatialGraph* graph) {

    const char* PointAttributeNameClip = "clipPoint";
    PointAttribute* pAttClip = graph->findEdgePointAttribute(PointAttributeNameClip);
    if (!pAttClip) {
        throw McException(QString("Graph has no attribute %1").arg(QString::fromLatin1(PointAttributeNameClip)));
    }

    for (int e=0; e<graph->getNumEdges(); ++e) {

        if ( (pAttClip->getFloatDataAtPoint(e, 0) > 300.0f) || (pAttClip->getFloatDataAtPoint(e, graph->getNumEdgePoints(e)) > 300.0f) ) {
            return SpatialGraphPoint(-1, -1);
        } else {
            for (int p=1; p<graph->getNumEdgePoints(e)-1; ++p) {
                const float clip = pAttClip->getFloatDataAtPoint(e, p);
                if (clip > 300.0f){
                    return SpatialGraphPoint(e, p);
                }
             }
         }
     }
    return SpatialGraphPoint(-1,-1);
}


void findStartingPoints(const HxSpatialGraph* graph, int currentNode, int edge, SpatialGraphSelection& selection) {

    const char* PointAttributeNameClip = "clipPoint";
    PointAttribute* pAttClip = graph->findEdgePointAttribute(PointAttributeNameClip);
    if (!pAttClip) {
        throw McException(QString("Graph has no attribute %1").arg(QString::fromLatin1(PointAttributeNameClip)));
    }

    const IncidenceList IncidentEdgeList = graph->getIncidentEdges(currentNode);

    for (int i=0; i<IncidentEdgeList.size(); ++i) {

        const int e = IncidentEdgeList[i];

        if (e != edge) {

            int bingo = -1;

            for (int p=0; p<graph->getNumEdgePoints(e); ++p) {
                const float clip = pAttClip->getFloatDataAtPoint(e, p);

                if ( (clip > 150.0f) && (bingo == -1) ) {
                    bingo = 1;
                    pAttClip->setFloatDataAtPoint(400.0f, e, p);

                }
            }

            if (bingo == -1) {

                const int source = graph->getEdgeSource(e);
                const int target = graph->getEdgeTarget(e);
                int newNode = -1;

                if (currentNode == source) {
                    newNode = target;
                } else if (currentNode == target) {
                    newNode = source;
                } else {
                    throw McException(QString("newNode is invalid."));
                }

                findStartingPoints(graph, newNode, e, selection);
            }
        }
    }
}


void insertStartingNodes(HxSpatialGraph* graph, const int rootNode, const int time) {

    // This functions searches all starting nodes (intersection of filopodia edge to growth cone body).
    // Follwing steps will be executed:
    // - find all endning nodes
    // - find all edges from each ending node to root
    // - find all edge points candidates which could be a starting point by naiv thresholding
    //   (edgepoint has thickness > threshold --> candidate --> clip value = 200)
    // - find actual starting nodes within candidates (only convert candidate closest to root)
    //   (loop starting with root node, look at all incident edges: if candidate --> convert to starting node, if not --> new node is target of incident edge)

    const char* PointAttributeNameClip = "clipPoint";
    PointAttribute* pAttClip = graph->addPointAttribute(PointAttributeNameClip, McPrimType::MC_FLOAT, 1);
    for (int e=0; e<graph->getNumEdges(); ++e) {
        for (int p=0; p<graph->getNumEdgePoints(e); ++p) {
            pAttClip->setFloatDataAtPoint(0.0f, e, p);
        }
    }

    const char* nodeAttName = "NodeType";
    EdgeVertexAttribute* nodeAtt = graph->findVertexAttribute(nodeAttName);
    if (!nodeAtt) {
        throw McException(QString("No vertex attribute found with name %1").arg(QString::fromLatin1(nodeAttName)));
    }
    const int startingNodeLabel = FilopodiaFunctions::getTypeLabelId(graph, BASE_NODE);

    const char* timeAttName = "TimeStep";
    EdgeVertexAttribute* timeAtt = graph->findVertexAttribute(timeAttName);
    if (!timeAtt) {
        throw McException(QString("No vertex attribute found with name %1").arg(QString::fromLatin1(timeAttName)));
    }
    const int timeLabelId = FilopodiaFunctions::getTimeIdFromTimeStep(graph, time);

    std::vector<int> endNodes;
    findEndNodes(graph, endNodes, time);

    for (int n=0; n<endNodes.size(); ++n) {

        const int endNode = endNodes[n];
        std::vector<int> edges;

        findEdgesFromEndToRoot(graph, endNode, rootNode, edges);

        const SpatialGraphPoint intersectionPoint = findIntersectionPoint(graph, edges);

        if ( !(intersectionPoint == SpatialGraphPoint(-1, -1)) ) {
            const int clipEdge = intersectionPoint.edgeNum;
            const int clipPoint = intersectionPoint.pointNum;
            pAttClip->setFloatDataAtPoint(200.0f, clipEdge, clipPoint);
        } else {
            theMsg->printf("No intersection for end node %i found", n);
        }

    }


    SpatialGraphSelection pointsToConvert(graph);
    findStartingPoints(graph, rootNode, -1, pointsToConvert);

    SpatialGraphPoint sgp = findNextPointToConvert(graph);
    while (!(sgp == SpatialGraphPoint(-1, -1))) {

        PointToVertexOperation* convertOp = FilopodiaFunctions::convertPointToNode(graph, sgp);
        const int newNode = convertOp->getNewVertexNum();

        nodeAtt->setIntDataAtIdx(newNode, startingNodeLabel);
        timeAtt->setIntDataAtIdx(newNode, timeLabelId);
        sgp = findNextPointToConvert(graph);
    }

}


void HxClipSpatialGraphByThickness::compute()
{
    if (portAction.wasHit()) {
        HxSpatialGraph* inputGraph = hxconnection_cast<HxSpatialGraph> (portData);
        if (!inputGraph) {
            throw McException(QString("No input graph"));
        }

        McHandle<HxSpatialGraph> outputGraph = dynamic_cast<HxSpatialGraph*>(getResult());
        if (outputGraph) {
            outputGraph->copyFrom(inputGraph);
        }
        else {
            outputGraph = inputGraph->duplicate();
        }
        outputGraph->composeLabel(inputGraph->getLabel(), "clipped");

        QString path(inputGraph->getFilename());
        int gc, time;
        FilopodiaFunctions::getGrowthConeNumberAndTimeFromFileName(path, gc, time);

        const TimeMinMax tMinMax = FilopodiaFunctions::getTimeMinMaxFromGraphLabels(inputGraph);

        for (int time=tMinMax.minT; time<tMinMax.maxT+1; ++time) {

            const int rootNode = FilopodiaFunctions::getRootNodeFromTimeStep(outputGraph, time);

            insertStartingNodes(outputGraph, rootNode, time);

        }
        setResult(outputGraph);
    }
}

