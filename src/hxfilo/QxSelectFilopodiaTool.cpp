#include "QxSelectFilopodiaTool.h"
#include "FilopodiaFunctions.h"
#include <hxneuroneditor/internal/HxNeuronEditorSubApp.h>
#include <Inventor/SoPickedPoint.h>
#include <Inventor/events/SoMouseButtonEvent.h>


QxSelectFilopodiaTool::QxSelectFilopodiaTool(HxNeuronEditorSubApp* editor)
: QxNeuronEditorModalTool(editor)
{
}


QxSelectFilopodiaTool::~QxSelectFilopodiaTool()
{
}


void QxSelectFilopodiaTool::onActivate(HxViewerBase* activeViewer)
{
    HxSpatialGraph* graph = mEditor->getSpatialGraph();
    if (!graph) {
        return;
    }

    if (!FilopodiaFunctions::isFilopodiaGraph(graph)) {
        return;
    }

    mEditor->setInteractive(true);
}


void QxSelectFilopodiaTool::onDeactivate(HxViewerBase* activeViewer) {
}


bool QxSelectFilopodiaTool::supportsViewer(HxViewerBase *viewer)  {
    if (!mEditor) {
        return false;
    }

    HxSpatialGraph *graph = mEditor->getSpatialGraph();
    if (!graph) {
        return false;
    }

    if (!graph->getLabelGroup(FilopodiaFunctions::getFilopodiaAttributeName()) ||
        !graph->getLabelGroup(FilopodiaFunctions::getTimeStepAttributeName()))
    {
        return false;
    }

    return (graph->getNumVertices() > 0);
}


void QxSelectFilopodiaTool::pickCallback(const GraphViewNodeInterface *viewNode, SoEventCallback* node) {

    const SoEvent* e = node->getEvent();
    if (SO_MOUSE_PRESS_EVENT(e, BUTTON1)) {
        const SoPickedPoint* pickedPoint = node->getPickedPoint();
        if (!pickedPoint) {
            return;
        }

        SelectionHelpers::Modifier modKeys = SelectionHelpers::getModifier(e);

        const SpatialGraphSelection &visibleSel   = mEditor->getVisibleElements();
        const SpatialGraphSelection &highlightSel = mEditor->getHighlightedElements();

        int vertexId = -1;
        int edgeId = -1;
        SpatialGraphPoint pointId(-1, -1);

        if (viewNode->getPickedElement(&visibleSel, &highlightSel, pickedPoint, vertexId, edgeId, pointId)) {
            doSelection(vertexId, edgeId, pointId, modKeys);

            node->setHandled();

            const SbVec3f point = pickedPoint->getPoint();
            McVec3f pointInWorldCoord = McVec3f(point[0], point[1], point[2]);
            mEditor->focusSelectionAtPickedPoint(pointInWorldCoord, vertexId, edgeId, pointId);
        }
    }
}


void selectFilopodia(const HxSpatialGraph* graph,
                     SpatialGraphSelection& highlightSel,
                     const SpatialGraphSelection& visibleSel,
                     const int vertexId,
                     int edgeId,
                     const SpatialGraphPoint& pointId,
                     const SelectionMode mode)
{

    int filopodiaId = -1;
    int connectedNodeId = -1;

    if (vertexId >= 0) {
        filopodiaId = FilopodiaFunctions::getFilopodiaIdOfNode(graph, vertexId);
        connectedNodeId = vertexId;
    }
    else if (edgeId >= 0) {
        filopodiaId = FilopodiaFunctions::getFilopodiaIdOfEdge(graph, edgeId);
        connectedNodeId = graph->getEdgeSource(edgeId);
    }
    else if (!(pointId == SpatialGraphPoint(-1, -1))) {
        edgeId = pointId.edgeNum;
        filopodiaId = FilopodiaFunctions::getFilopodiaIdOfEdge(graph, edgeId);
        connectedNodeId = graph->getEdgeSource(edgeId);
    }

    SpatialGraphSelection filopodiaSel, tmp;

    switch(mode) {
        case SELECT_FILOPODIA_SINGLE_TIMESTEP:
            highlightSel = FilopodiaFunctions::getFilopodiaSelectionOfNode(graph, connectedNodeId);
            highlightSel &= visibleSel;
            break;

        case TOGGLE_FILOPODIA_SINGLE_TIMESTEP:
            filopodiaSel = FilopodiaFunctions::getFilopodiaSelectionOfNode(graph, connectedNodeId);
            filopodiaSel &= visibleSel;

            tmp = filopodiaSel;
            tmp &= highlightSel;
            if (filopodiaSel == tmp) {
                highlightSel.subtractSelection(filopodiaSel); // Entire filopodia selected -> deselect
            }
            else {
                highlightSel.addSelection(filopodiaSel); // Filopodia not or only partly selected -> select
            }
            break;

        case SELECT_FILOPODIA_ALL_TIMESTEPS:
            highlightSel = FilopodiaFunctions::getFilopodiaSelectionForAllTimeSteps(graph, filopodiaId);
            highlightSel &= visibleSel;
            break;

        case TOGGLE_FILOPODIA_ALL_TIMESTEPS:
            filopodiaSel = FilopodiaFunctions::getFilopodiaSelectionForAllTimeSteps(graph, filopodiaId);
            filopodiaSel &= visibleSel;

            tmp = filopodiaSel;
            tmp &= highlightSel;
            if (filopodiaSel == tmp) {
                highlightSel.subtractSelection(filopodiaSel); // Entire filopodia selected -> deselect
            }
            else {
                highlightSel.addSelection(filopodiaSel); // Filopodia not or only partly selected -> select
            }
            break;

        default:
            break;
    }
}

void QxSelectFilopodiaTool::doSelection(const int vertexId,
                                        const int edgeId,
                                        const SpatialGraphPoint &pointId,
                                        const SelectionHelpers::Modifier modKeys) const
{
    HxSpatialGraph *graph = mEditor->getSpatialGraph();
    SpatialGraphSelection highlightSel = mEditor->getHighlightedElements();
    const SpatialGraphSelection& visibleSel = mEditor->getVisibleElements();

    switch(modKeys) {
        case SelectionHelpers::NO_MODIFIER:
            selectFilopodia(graph, highlightSel, visibleSel, vertexId, edgeId, pointId, SELECT_FILOPODIA_SINGLE_TIMESTEP);
            break;
        case SelectionHelpers::CONTROL:
            selectFilopodia(graph, highlightSel, visibleSel, vertexId, edgeId, pointId, TOGGLE_FILOPODIA_SINGLE_TIMESTEP);
            break;
        case SelectionHelpers::SHIFT:
            selectFilopodia(graph, highlightSel, visibleSel, vertexId, edgeId, pointId, SELECT_FILOPODIA_ALL_TIMESTEPS);
            break;
        case SelectionHelpers::SHIFT_CONTROL:
            selectFilopodia(graph, highlightSel, visibleSel, vertexId, edgeId, pointId, TOGGLE_FILOPODIA_ALL_TIMESTEPS);
            break;

        default: ; // do nothing
    }

    mEditor->setHighlightedElements(highlightSel);
}
