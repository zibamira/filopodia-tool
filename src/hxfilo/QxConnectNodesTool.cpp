#include "QxConnectNodesTool.h"
#include "FilopodiaFunctions.h"
#include "FilopodiaOperationSet.h"
#include "ExFilopodiaTraceTool.h"
#include <hxneuroneditor/internal/HxNeuronEditorSubApp.h>
#include <QDebug>


QxConnectNodesTool::QxConnectNodesTool(HxNeuronEditorSubApp* editor)
: QxNeuronEditorTriggerTool(editor)
{
}


QxConnectNodesTool::~QxConnectNodesTool()
{
}


bool QxConnectNodesTool::supportsViewer(HxViewerBase* baseViewer) {
    if (!mEditor) {
        return false;
    }

    HxSpatialGraph* graph = mEditor->getSpatialGraph();
    return (graph && (graph->getNumEdges() > 0));
}


void QxConnectNodesTool::onTrigger(HxViewerBase* activeViewer, int modifiers) {

    HxSpatialGraph* graph = mEditor->getSpatialGraph();
    if (!graph)
        return;

    connectNodes();
}


bool checkConnectRequirements(HxSpatialGraph* graph, const SpatialGraphSelection selectedElements, SpatialGraphSelection &deleteSel, int &branchNode) {

    const char* nodeTypeLabels = FilopodiaFunctions::getTypeLabelAttributeName();
    const char* geoLabels = FilopodiaFunctions::getManualGeometryLabelAttributeName();

    const EdgeVertexAttribute* vTypeAtt = graph->findVertexAttribute(nodeTypeLabels);
    if (!vTypeAtt){
        theMsg->printf("Could not connect node: Attribute %s not found.", nodeTypeLabels);
        HxMessage::error(QString("Could not connect node:\n No attribute found with name %1.").arg(QString::fromLatin1(nodeTypeLabels)), "ok" );
        return false;
    }

    const EdgeVertexAttribute* vGeoAtt = graph->findVertexAttribute(geoLabels);
    const EdgeVertexAttribute* eGeoAtt = graph->findEdgeAttribute(geoLabels);
    if ( (!vGeoAtt) || (!eGeoAtt) ){
        theMsg->printf("Could not connect node: Attribute %s not found.", geoLabels);
        HxMessage::error(QString("Could not connect node:\n No attribute found with name %1.").arg(QString::fromLatin1(geoLabels)), "ok" );
        return false;
    }

    if (selectedElements.getNumSelectedVertices() == 1 && selectedElements.getNumSelectedEdges() == 0 && selectedElements.getNumSelectedPoints() == 1) {

        const int vertexId = selectedElements.getSelectedVertex(0);
        const SpatialGraphPoint sgp = selectedElements.getSelectedPoint(0);
        const int edgeId = sgp.edgeNum;
        const int pointId = sgp.pointNum;

        if (!FilopodiaFunctions::hasNodeType(graph, TIP_NODE, vertexId)) {
            theMsg->printf("Could not connect node: selected node has to be ending.");
            HxMessage::error(QString("Could not connect node:\n selected node has to be ending node."), "ok" );
            return false;
        } else if (graph->getIncidentEdges(vertexId).size() != 1) {
            theMsg->printf("Could not connect node: selected node has more %i incident edges.", graph->getIncidentEdges(vertexId).size() );
            HxMessage::error(QString("Could not connect node:\n selected node has more than one incident edges."), "ok" );
            return false;
        } else if ( !(FilopodiaFunctions::getTimeIdOfNode(graph, vertexId) == FilopodiaFunctions::getTimeIdOfEdge(graph, edgeId)) ) {
            theMsg->printf("Could not connect node: selected node and edgepoint have to have the same time label.");
            HxMessage::error(QString("Could not connect node:\n Selected node and edgepoint have to have the same time label."), "ok" );
            return false;

        } else if ( (pointId == 0) || (pointId == graph->getNumEdgePoints(edgeId)-1) ) {
            theMsg->printf("Could not connect node: selected edgepoint is not convertable.");
            HxMessage::error(QString("Could not connect node:\n selected edgepoint is not convertable."), "ok" );
            return false;

        } else {

            branchNode = FilopodiaFunctions::getRootNodeFromTimeStep(graph, FilopodiaFunctions::getTimeOfNode(graph, vertexId));
            deleteSel = FilopodiaFunctions::getSelectionTilBranchingNode(graph,vertexId, branchNode);
            deleteSel.deselectVertex(vertexId);
            return true;

        }

    } else {
        theMsg->printf("Could not connect node: conversion requires a single selected ending node and a single selected edge point.");
        HxMessage::error("Could not connect node: conversion requires a single selected ending node and a single selected edge point.", "ok" );
        return false;
    }
}


void QxConnectNodesTool::connectNodes() {

    qDebug() << "Connect nodes tool is outdated. Please use tracing tool instead.";
    theMsg->printf("Connect nodes tool is outdated. Please use tracing tool instead.");
}
