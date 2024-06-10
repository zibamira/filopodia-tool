#include "QxDeleteFilopodiaTool.h"
#include "FilopodiaFunctions.h"
#include "FilopodiaOperationSet.h"
#include <hxneuroneditor/internal/HxNeuronEditorSubApp.h>
#include <hxspatialgraph/internal/SpatialGraphFunctions.h>
#include <mclib/McException.h>
#include <QDebug>


QxDeleteFilopodiaTool::QxDeleteFilopodiaTool(HxNeuronEditorSubApp* editor)
: QxNeuronEditorTriggerTool(editor)
{
}


QxDeleteFilopodiaTool::~QxDeleteFilopodiaTool()
{
}


bool QxDeleteFilopodiaTool::supportsViewer(HxViewerBase* baseViewer) {
    if (!mEditor) {
        return false;
    }

    HxSpatialGraph* graph = mEditor->getSpatialGraph();
    return (graph && (graph->getNumEdges() > 0));
}


void QxDeleteFilopodiaTool::onTrigger(HxViewerBase* activeViewer, int modifiers) {

    HxSpatialGraph* graph = mEditor->getSpatialGraph();
    if (!graph)
        return;

    deleteFilopodia();
}


bool checkDeleteRequirements(HxSpatialGraph* graph, const SpatialGraphSelection selectedElements, SpatialGraphSelection& deleteSelection) {

    const char* timeLabels = FilopodiaFunctions::getTimeStepAttributeName();
    const char* nodeTypeLabels = FilopodiaFunctions::getTypeLabelAttributeName();

    const EdgeVertexAttribute* vTimeAtt = graph->findVertexAttribute(timeLabels);
    if (!vTimeAtt){
        theMsg->printf("Could not delete filopodium: Attribute %s not found.", timeLabels);
        HxMessage::error(QString("Could not delete filopodium:\n No attribute found with name %1.").arg(QString::fromLatin1(timeLabels)), "ok" );
        return false;
    }

    const EdgeVertexAttribute* vTypeAtt = graph->findVertexAttribute(nodeTypeLabels);
    if (!vTypeAtt){
        theMsg->printf("Could not delete filopodium: Attribute %s not found.", nodeTypeLabels);
        HxMessage::error(QString("Could not delete filopodium:\n No attribute found with name %1.").arg(QString::fromLatin1(nodeTypeLabels)), "ok" );
        return false;
    }

    // Check number of selected elements
    if ( (selectedElements.getNumSelectedEdges() == 0) || (selectedElements.getNumSelectedPoints() == 0) || (selectedElements.getNumSelectedVertices() == 1) ) {

        const int selectedNode = selectedElements.getSelectedVertex(0);
        const int currentTime = FilopodiaFunctions::getTimeOfNode(graph, selectedNode);

        if (FilopodiaFunctions::hasNodeType(graph, TIP_NODE, selectedNode)) {

            const int rootNode = FilopodiaFunctions::getRootNodeFromTimeStep(graph, currentTime);
            int branchNode = rootNode;

            deleteSelection = FilopodiaFunctions::getSelectionTilBranchingNode(graph, selectedNode, branchNode);
            deleteSelection.deselectVertex(rootNode); //Never delete root node

            if (deleteSelection.isEmpty()) {
                theMsg->printf("Could not delete filopodium:\nDelete selection is empty. Please check consistency.");
                HxMessage::error(QString("Could not delete filopodium:\nDelete selection is empty. Please check consistency."), "ok" );
                return false;
            }
            else {
                return true;
            }

        } else {
            theMsg->printf("Could not delete filopodia: Single tip node selection is required.");
            HxMessage::error(QString("Could not delete filopodia: Single tip node selection is required."), "ok" );
            return false;
        }

    } else {
        theMsg->printf("Could not delete filopodia: Single tip node selection is required.");
        HxMessage::error(QString("Could not delete filopodia: Single tip node selection is required."), "ok" );
        return false;
    }
    theMsg->printf("Could not delete filopodia: Unknown issue.");
    HxMessage::error(QString("Could not delete filopodia: Unknown issue."), "ok" );
    return false;
}


void QxDeleteFilopodiaTool::deleteFilopodia() {

    qDebug() << "Delete filopodium tool.";
    theMsg->printf("Delete filopodium tool.");
    const SpatialGraphSelection &selectedElements = mEditor->getHighlightedElements();
    const SpatialGraphSelection &visibleElements  = mEditor->getVisibleElements();
    HxSpatialGraph* graph = mEditor->getSpatialGraph();

    if (!FilopodiaFunctions::isFilopodiaGraph(graph)) {
        return;
    }

    SpatialGraphSelection deleteSelection(graph);

    if (checkDeleteRequirements(graph, selectedElements, deleteSelection)) {

        try
        {
            DeleteFilopodiaOperationSet* deleteFiloOp = new DeleteFilopodiaOperationSet(graph, deleteSelection, visibleElements);
            if (deleteFiloOp != 0) {
                mEditor->execNewOp(deleteFiloOp);
            }
        }
        catch (McException& e)
        {
            theMsg->printf(QString("%1").arg(e.what()));
            HxMessage::error(QString("Cannot delete filopodium: \n%1").arg(e.what()), "ok");
        }
    }
}
