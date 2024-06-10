#include "HxSetFilopodiaBaseNodes.h"
#include "FilopodiaFunctions.h"
#include <hxspatialgraph/internal/HxSpatialGraph.h>
#include <hxspatialgraph/internal/SpatialGraphSelection.h>
#include <hxcore/HxObjectPool.h>
#include <mclib/McException.h>
#include <QDebug>


HX_INIT_CLASS(HxSetFilopodiaBaseNodes,HxCompModule)

HxSetFilopodiaBaseNodes::HxSetFilopodiaBaseNodes() :
    HxCompModule(HxSpatialGraph::getClassTypeId()),
    portInputDir(this, tr("inputDirectory"), tr("Input Directory"), HxPortFilename::LOAD_DIRECTORY),
    portThreshold(this, tr("Threshold"), tr("Threshold")),
    portAction(this, tr("action"), tr("Action"))
{
    portThreshold.setValue(3000.0f);
}


HxSetFilopodiaBaseNodes::~HxSetFilopodiaBaseNodes()
{
}


std::vector<std::vector<PointOnPath> > sortByInitialSegment(const std::vector<PointOnPath>& pathPoints) {
    std::vector<std::vector<PointOnPath> >  sorted;
    QHash<int, int> segmentNumToIndex;
    for (int p=0; p<pathPoints.size(); ++p) {
        const int segmentNum = pathPoints[p].path.segmentNums.front();
        if (segmentNumToIndex.contains(segmentNum)) {
            const int index = segmentNumToIndex.value(segmentNum);
            sorted[index].push_back(pathPoints[p]);
        }
        else {
            std::vector<PointOnPath> arr;
            arr.push_back(pathPoints[p]);
            const int index = sorted.size();
            sorted.push_back(arr);
            segmentNumToIndex.insert(segmentNum, index);
        }
    }
    return sorted;
}


void printPointOnPath(const PointOnPath& pop, const HxSpatialGraph* graph) {
    printf("\n---------------\n");
    printf("NodeNums: ");
    for (int i=0; i<pop.path.nodeNums.size(); ++i) {
        printf(" %d ", pop.path.nodeNums[i]);
    }
    printf("\nSegmentNums: ");
    for (int i=0; i<pop.path.segmentNums.size(); ++i) {
        printf(" %d ", pop.path.segmentNums[i]);
    }
    printf("\nSegmentReversed: ");
    for (int i=0; i<pop.path.segmentReversed.size(); ++i) {
        printf(" %d ", pop.path.segmentReversed[i]);
    }
    printf("\nStarting point: (%d, %d)", pop.point.edgeNum, pop.point.pointNum);
    printf("\nNumEdgePoints: %d\n", graph->getNumEdgePoints(pop.point.edgeNum));
}


SpatialGraphPoint getPointOnInitialSegment(const std::vector<PointOnPath>& pops,
                                           const HxSpatialGraph* graph)
{
    if (pops.size() == 0) {
        return SpatialGraphPoint(-1, -1);
    }

    const int initialSegmentNum = pops.front().path.segmentNums.front();
    const bool initialSegmentReversed = pops.front().path.segmentReversed.front();

    std::vector<SpatialGraphPoint> candidates;

    for (int p=0; p<pops.size(); ++p) {
        if (pops[p].path.segmentNums.front() != initialSegmentNum) {
            throw McException("Error in getPointOnInitialSegment: invalid initial segment");
        }
        if (pops[p].point.edgeNum == initialSegmentNum) {
            candidates.push_back(pops[p].point);
        }
    }

    if (candidates.size() > 0) {
        std::sort(candidates.begin(),candidates.end());
        if (initialSegmentReversed) {
            return candidates.back();
        }
        else {
            return candidates.front();
        }
    }

    // Check whether one of the starting points is on the node following the initial segment.
    for (int p=0; p<pops.size(); ++p) {
        const PointOnPath& pop = pops[p];
        if (pop.path.segmentNums.size() > 1) {
            const int nextSegment = pop.path.segmentNums[1];
            if (pop.point.edgeNum == nextSegment) {
                if ((pop.path.segmentReversed[1]) && (pop.point.pointNum == (graph->getNumEdgePoints(nextSegment)-1))) {
                    return pop.point;
                }
                else if (!(pop.path.segmentReversed[1]) && pop.point.pointNum == 0) {
                    return pop.point;
                }
            }
        }
    }

    return SpatialGraphPoint(-1, -1);
}


// Finds all valid starting points from the candidates in pathPoints
// by ensuring there is only one starting point per subtree.
// Approach:
// (1) sort the paths by initial segment
// (2) if there is a candidate point on the initial segment, take it, discard rest
// (3) if not, remove the initial segment from the paths, repeat for the shortened paths
void prunePaths(const std::vector<PointOnPath>& pathPoints,
                const HxSpatialGraph* graph,
                std::vector<SpatialGraphPoint>& finalPoints)
{
    if (pathPoints.size() == 0) {
        return;
    }

    std::vector<std::vector<PointOnPath> > sortedPoPs = sortByInitialSegment(pathPoints);
    for (int s=0; s<sortedPoPs.size(); ++s) {
        const SpatialGraphPoint pointOnInitialSegment = getPointOnInitialSegment(sortedPoPs[s], graph);
        if (pointOnInitialSegment != SpatialGraphPoint(-1,-1)) {
            finalPoints.push_back(pointOnInitialSegment);
        } else {
            std::vector<PointOnPath> children;
            for (int i=0; i<sortedPoPs[s].size(); ++i) {
                PointOnPath child = sortedPoPs[s][i];
                if (child.path.segmentNums.size() > 1) {
                    child.path.nodeNums.erase(child.path.nodeNums.begin());
                    child.path.segmentNums.erase(child.path.segmentNums.begin());
                    child.path.segmentReversed.erase(child.path.segmentReversed.begin());
                    children.push_back(child);
                }
            }
            prunePaths(children, graph, finalPoints);
        }
    }
}



std::vector<SpatialGraphPoint> pruneStartingPoints(const HxSpatialGraph* outputGraph,
                                                const std::vector<PointOnPath>& pathPoints)
{
    std::vector<PointOnPath> pruned;

    for (int p=0; p<pathPoints.size(); ++p) {
        const SpatialGraphPoint& point = pathPoints[p].point;
        const Path& path = pathPoints[p].path;
        const int edgeNum = point.edgeNum;
        const int pointNum = point.pointNum;

        // Exclude invalid points
        if ((point == SpatialGraphPoint(-1, -1))) {
            continue;
        }

        // Exclude points that are root or end nodes
        if (point.edgeNum == path.segmentNums.front()) {
            if (path.segmentReversed[0] && point.pointNum == outputGraph->getNumEdgePoints(point.edgeNum)-1) {
                continue;
            }
            if (!path.segmentReversed[0] && point.pointNum == 0) {
                continue;
            }
        }

        // Exclude points that are nodes
        if (pointNum == 0 || pointNum == outputGraph->getNumEdgePoints(edgeNum)) {
            continue;
        }

        if (point.edgeNum == path.segmentNums.back()) {
            if (path.segmentReversed[0] && point.pointNum == 0) {
                continue;
            }
            if (!path.segmentReversed[0] && point.pointNum == outputGraph->getNumEdgePoints(point.edgeNum)-1) {
                continue;
            }
        }


        pruned.push_back(pathPoints[p]);
    }


    std::vector<SpatialGraphPoint> finalPoints;
    prunePaths(pathPoints, outputGraph, finalPoints);

    return finalPoints;
}


void insertStartingNodes(HxSpatialGraph* graph, const std::vector<SpatialGraphPoint>& startingPoints) {
    SpatialGraphSelection edgeSel(graph);
    for (int sp=0; sp<startingPoints.size(); ++sp) {
        if (edgeSel.isSelectedEdge(startingPoints[sp].edgeNum)) {
            throw McException(QString("Cannot insert starting node: duplicate edge"));
        }
        edgeSel.selectEdge(startingPoints[sp].edgeNum);
    }

    const char* nodeAttName = FilopodiaFunctions::getTypeLabelAttributeName();
    EdgeVertexAttribute* nodeAtt = graph->findVertexAttribute(nodeAttName);
    if (!nodeAtt) {
        throw McException(QString("No vertex attribute found with name %1").arg(QString::fromLatin1(nodeAttName)));
    }

    const char* timeAttName = FilopodiaFunctions::getTimeStepAttributeName();
    EdgeVertexAttribute* timeVAtt = graph->findVertexAttribute(timeAttName);
    if (!timeVAtt) {
        throw McException(QString("No vertex attribute found with name %1").arg(QString::fromLatin1(timeAttName)));
    }

    const int startingNodeLabelId = FilopodiaFunctions::getTypeLabelId(graph, BASE_NODE);

    std::vector<SpatialGraphPoint> sortedStartingPoints(startingPoints);
    std::sort(sortedStartingPoints.begin(),sortedStartingPoints.end());


    for (int sp=sortedStartingPoints.size()-1; sp>=0; --sp) {
        const SpatialGraphPoint p = sortedStartingPoints[sp];

        if (p.pointNum == 0) {
            const int nodeNum = graph->getEdgeSource(p.edgeNum);
            nodeAtt->setIntDataAtIdx(nodeNum, startingNodeLabelId);
        }
        else if (p.pointNum == graph->getNumEdgePoints(p.edgeNum)-1) {
            const int nodeNum = graph->getEdgeTarget(p.edgeNum);
            nodeAtt->setIntDataAtIdx(nodeNum, startingNodeLabelId);
        }
        else {
            const int timeLabelId = FilopodiaFunctions::getTimeIdOfEdge(graph, p.edgeNum);
            SpatialGraphSelection pointSel(graph);
            pointSel.selectPoint(p);
            PointToVertexOperation* convertOp = new PointToVertexOperation(graph, pointSel, SpatialGraphSelection(graph));
            convertOp->exec();
            const int newNode = convertOp->getNewVertexNum();

            nodeAtt->setIntDataAtIdx(newNode, startingNodeLabelId);
            timeVAtt->setIntDataAtIdx(newNode, timeLabelId);
            delete convertOp;
        }
    }
    qDebug() << "Inserted" << sortedStartingPoints.size() << "starting nodes";
}


void HxSetFilopodiaBaseNodes::compute()
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
        outputGraph->composeLabel(inputGraph->getLabel(), "startingNodes");

        const TimeMinMax tMinMax = FilopodiaFunctions::getTimeMinMaxFromGraphLabels(inputGraph);
        const QString inputDir = portInputDir.getValue();
        const QMap<int, QFileInfo> filePerTimestep = FilopodiaFunctions::getFilePerTimestep(inputDir, tMinMax);

        const float significantThreshold = portThreshold.getValue();

        std::vector<SpatialGraphPoint> startingPoints;

        for (int time=tMinMax.minT; time<=tMinMax.maxT; ++time) {

            McHandle<HxUniformScalarField3> image = FilopodiaFunctions::loadUniformScalarField3(filePerTimestep.value(time).absoluteFilePath());

            const int rootNode = FilopodiaFunctions::getRootNodeFromTimeStep(outputGraph, time);
            SpatialGraphSelection endNodes = FilopodiaFunctions::getNodesOfTypeForTime(outputGraph, TIP_NODE, time);

            std::vector<PointOnPath> pathPoints;

            for (int e=0; e<endNodes.getNumSelectedVertices(); ++e) {
                const Path path = FilopodiaFunctions::getPathFromRootToTip(outputGraph, rootNode, endNodes.getSelectedVertex(e));
                const SpatialGraphPoint startingPoint = FilopodiaFunctions::getBasePointOnPath(path, outputGraph, image, significantThreshold);

                PointOnPath pop;
                pop.path = path;
                pop.point = startingPoint;

                pathPoints.push_back(pop);
            }

            const std::vector<SpatialGraphPoint> startingPointsForTimestep = pruneStartingPoints(outputGraph, pathPoints);

            qDebug() << "Insert starting nodes: timestep" << time << "\tNumber of starting nodes:" << startingPointsForTimestep.size();

            startingPoints.insert(startingPoints.end(),startingPointsForTimestep.begin(),startingPointsForTimestep.end());
            theObjectPool->removeObject(image);
        }
        insertStartingNodes(outputGraph, startingPoints);
        setResult(outputGraph);
    }
}

