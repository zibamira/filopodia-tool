#include "QxMoveNodeTool.h"
#include "FilopodiaFunctions.h"
#include "FilopodiaOperationSet.h"
#include <hxneuroneditor/internal/HxNeuronEditorSubApp.h>
#include <mclib/McException.h>
#include <QDebug>

QxMoveNodeTool::QxMoveNodeTool(HxNeuronEditorSubApp* editor)
    : QxNeuronEditorTriggerTool(editor)
{
}

QxMoveNodeTool::~QxMoveNodeTool()
{
}

bool
QxMoveNodeTool::supportsViewer(HxViewerBase* baseViewer)
{
    if (!mEditor)
    {
        return false;
    }

    HxSpatialGraph* graph = mEditor->getSpatialGraph();
    return (graph && (graph->getNumEdges() > 0));
}

void
QxMoveNodeTool::onTrigger(HxViewerBase* activeViewer, int modifiers)
{
    HxSpatialGraph* graph = mEditor->getSpatialGraph();
    if (!graph)
        return;

    moveNode();
}

bool
checkMoveRequirements(HxSpatialGraph* graph, const SpatialGraphSelection selectedElements)
{
    const QString nodeTypeLabels = QString::fromLatin1(FilopodiaFunctions::getTypeLabelAttributeName());
    const QString geoLabels = QString::fromLatin1(FilopodiaFunctions::getManualGeometryLabelAttributeName());

    const EdgeVertexAttribute* vTypeAtt = graph->findVertexAttribute(qPrintable(nodeTypeLabels));
    if (!vTypeAtt)
    {
        theMsg->printf(QString("Could not move node: Attribute %1 not found.").arg(nodeTypeLabels));
        HxMessage::error(QString("Could not move node:\n No attribute found with name %1.").arg(nodeTypeLabels), "ok");
        return false;
    }

    const EdgeVertexAttribute* vGeoAtt = graph->findVertexAttribute(geoLabels.toLatin1().constData());
    const EdgeVertexAttribute* eGeoAtt = graph->findEdgeAttribute(geoLabels.toLatin1().constData());
    if ((!vGeoAtt) || (!eGeoAtt))
    {
        theMsg->printf(QString("Could not move node: Attribute %1 not found.").arg(geoLabels));
        HxMessage::error(QString("Could not move node:\n No attribute found with name %1.").arg(geoLabels), "ok");
        return false;
    }

    if (selectedElements.getNumSelectedVertices() == 1 && selectedElements.getNumSelectedEdges() == 0 && selectedElements.getNumSelectedPoints() == 1)
    {
        const int vertexId = selectedElements.getSelectedVertex(0);
        const SpatialGraphPoint sgp = selectedElements.getSelectedPoint(0);
        const int edgeId = sgp.edgeNum;
        const int pointId = sgp.pointNum;
        const int incidentEdge = graph->getIncidentEdges(vertexId)[0];

        if ((!FilopodiaFunctions::hasNodeType(graph, BASE_NODE, vertexId) && !FilopodiaFunctions::hasNodeType(graph, TIP_NODE, vertexId)))
        {
            theMsg->printf("Could not move node: selected node has to be base or tip.");
            HxMessage::error(QString("Could not move node:\n selected node has to be base or tip."), "ok");
            return false;
        }
        else if (!(FilopodiaFunctions::getTimeIdOfNode(graph, vertexId) == FilopodiaFunctions::getTimeIdOfEdge(graph, edgeId)))
        {
            theMsg->printf("Could not move node: selected node and edgepoint have to have the same time label.");
            HxMessage::error(QString("Could not move node:\n Selected node and edgepoint have to have the same time label."), "ok");
            return false;
        }
        else if ((pointId == 0) || (pointId == graph->getNumEdgePoints(edgeId) - 1))
        {
            theMsg->printf("Could not move node: selected edgepoint is not convertable.");
            HxMessage::error(QString("Could not move node:\n selected edgepoint is not convertable."), "ok");
            return false;
        }
        else if (FilopodiaFunctions::hasNodeType(graph, TIP_NODE, vertexId) && edgeId != incidentEdge)
        {
            theMsg->printf("Could not move node: selected edgepoint must be located at incident edge.");
            HxMessage::error(QString("Could not move node:\n selected edgepoint must be located at incident edge."), "ok");
            return false;
        }
        else
        {
            return true;
        }
    }
    else if (selectedElements.getNumSelectedVertices() == 0 && selectedElements.getNumSelectedEdges() == 0 && selectedElements.getNumSelectedPoints() == 1)
    {
        return true;
    }
    else
    {
        theMsg->printf("Could not move node: conversion requires a single selected node and a single selected edge point.");
        HxMessage::error("Could not move node: conversion requires a single selected node and a single selected edge point.", "ok");
        return false;
    }
}

void
QxMoveNodeTool::moveNode()
{
    const SpatialGraphSelection& selectedElements = mEditor->getHighlightedElements();
    const SpatialGraphSelection& visibleElements = mEditor->getVisibleElements();
    HxSpatialGraph* graph = mEditor->getSpatialGraph();

    if (!FilopodiaFunctions::isFilopodiaGraph(graph))
    {
        return;
    }

    if (checkMoveRequirements(graph, selectedElements))
    {
        if (selectedElements.getNumSelectedVertices() == 1 && selectedElements.getNumSelectedEdges() == 0 && selectedElements.getNumSelectedPoints() == 1)
        {
            const int vertexId = selectedElements.getSelectedVertex(0);

            if (FilopodiaFunctions::hasNodeType(graph, TIP_NODE, vertexId))
            {
                qDebug() << "Move node tool: move tip along edge";
                theMsg->printf("Move node tool: move tip along edge.");

                // Set required labels
                const int timeId = FilopodiaFunctions::getTimeIdOfNode(graph, vertexId);
                const int filoId = FilopodiaFunctions::getFilopodiaIdOfNode(graph, vertexId);
                const int rootNode = FilopodiaFunctions::getRootNodeFromTimeStep(graph, FilopodiaFunctions::getTimeStepFromTimeId(graph, timeId));

                McDArray<int> tipLabels;
                tipLabels.push_back(FilopodiaFunctions::getGcIdOfNode(graph, rootNode));
                tipLabels.push_back(timeId);
                tipLabels.push_back(FilopodiaFunctions::getTypeLabelId(graph, TIP_NODE));
                tipLabels.push_back(FilopodiaFunctions::getLocationLabelId(graph, FILOPODIUM));
                tipLabels.push_back(filoId);
                tipLabels.push_back(FilopodiaFunctions::getMatchLabelId(graph, IGNORED));
                tipLabels.push_back(FilopodiaFunctions::getManualGeometryLabelId(graph, MANUAL));

                try
                {
                    MoveTipAlongEdgeOperationSet* moveNodeOp = new MoveTipAlongEdgeOperationSet(graph, selectedElements, visibleElements, tipLabels);
                    if (moveNodeOp != 0) {
                        mEditor->execNewOp(moveNodeOp);
                    }
                }
                catch (McException& e) {
                    HxMessage::error(QString("Could not move tip along edge:\n%1.").arg(e.what()), "ok" );
                }
            }
            else if (FilopodiaFunctions::hasNodeType(graph, BASE_NODE, vertexId))
            {
                qDebug() << "Move node tool: move base.";
                theMsg->printf("Move node tool: move base.");

                // Set required labels
                const int timeId = FilopodiaFunctions::getTimeIdOfNode(graph, vertexId);
                const int filoId = FilopodiaFunctions::getFilopodiaIdOfNode(graph, vertexId);
                const int matchId = FilopodiaFunctions::getMatchIdOfNode(graph, vertexId);
                const int rootNode = FilopodiaFunctions::getRootNodeFromTimeStep(graph, FilopodiaFunctions::getTimeStepFromTimeId(graph, timeId));

                McDArray<int> baseLabels;
                baseLabels.push_back(FilopodiaFunctions::getGcIdOfNode(graph, rootNode));
                baseLabels.push_back(timeId);
                baseLabels.push_back(FilopodiaFunctions::getTypeLabelId(graph, BASE_NODE));
                baseLabels.push_back(FilopodiaFunctions::getLocationLabelId(graph, FILOPODIUM));
                baseLabels.push_back(filoId);
                baseLabels.push_back(matchId);
                baseLabels.push_back(FilopodiaFunctions::getManualGeometryLabelId(graph, MANUAL));

                try
                {
                    MoveBaseAlongEdgeOperationSet* moveNodeOp = new MoveBaseAlongEdgeOperationSet(graph, selectedElements, visibleElements, baseLabels);
                    if (moveNodeOp != 0) {
                        mEditor->execNewOp(moveNodeOp);
                    }
                }
                catch (McException& e) {
                    HxMessage::error(QString("Could not move base along edge:\n%1.").arg(e.what()), "ok" );
                }
            }
        }
        else if (selectedElements.getNumSelectedVertices() == 0 && selectedElements.getNumSelectedEdges() == 0 && selectedElements.getNumSelectedPoints() == 1)
        {
            qDebug() << "Move node tool: set new base along edge.";
            theMsg->printf("Move node tool: set new base along edge.");

            SpatialGraphPoint point = selectedElements.getSelectedPoint(0);

            // Set required labels
            const int timeId = FilopodiaFunctions::getTimeIdOfEdge(graph, point.edgeNum);
            const int rootNode = FilopodiaFunctions::getRootNodeFromTimeStep(graph, FilopodiaFunctions::getTimeStepFromTimeId(graph, timeId));

            McDArray<int> baseLabels;
            baseLabels.push_back(FilopodiaFunctions::getGcIdOfNode(graph, rootNode));
            baseLabels.push_back(timeId);
            baseLabels.push_back(FilopodiaFunctions::getTypeLabelId(graph, BASE_NODE));
            baseLabels.push_back(FilopodiaFunctions::getLocationLabelId(graph, FILOPODIUM));
            baseLabels.push_back(FilopodiaFunctions::getFilopodiaLabelId(graph, UNASSIGNED));
            baseLabels.push_back(FilopodiaFunctions::getMatchLabelId(graph, UNASSIGNED));
            baseLabels.push_back(FilopodiaFunctions::getManualGeometryLabelId(graph, MANUAL));

            // Set new base
            try {
                MoveBaseAlongEdgeOperationSet* moveNodeOp = new MoveBaseAlongEdgeOperationSet(graph, selectedElements, visibleElements, baseLabels);
                if (moveNodeOp != 0) {
                    mEditor->execNewOp(moveNodeOp);
                }
            }
            catch (McException& e) {
                HxMessage::error(QString("Could not set new base:\n%1.").arg(e.what()), "ok" );
            }
        }
    }
}
