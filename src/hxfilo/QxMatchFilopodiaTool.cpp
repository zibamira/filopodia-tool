#include "QxMatchFilopodiaTool.h"
#include "FilopodiaFunctions.h"
#include "FilopodiaOperationSet.h"
#include "HxFilopodiaTrack.h"
#include <hxneuroneditor/internal/HxNeuronEditorSubApp.h>
#include <mclib/McException.h>
#include <QDebug>
#include <QTime>


QxMatchFilopodiaTool::QxMatchFilopodiaTool(HxNeuronEditorSubApp* editor)
: QxNeuronEditorTriggerTool(editor)
{
}


QxMatchFilopodiaTool::~QxMatchFilopodiaTool()
{
}


bool QxMatchFilopodiaTool::supportsViewer(HxViewerBase* baseViewer) {
    if (!mEditor) {
        return false;
    }

    HxSpatialGraph* graph = mEditor->getSpatialGraph();
    return (graph && (graph->getNumEdges() > 0));
}


void QxMatchFilopodiaTool::onTrigger(HxViewerBase* activeViewer, int modifiers) {

    HxSpatialGraph* graph = mEditor->getSpatialGraph();
    if (!graph)
        return;

    if (!FilopodiaFunctions::isFilopodiaGraph(graph)) {
        return;
    }

    matchFilopodia(modifiers);
}


SpatialGraphSelection getBaseNodesForSelectedElements(const HxSpatialGraph* graph,
                                                      const SpatialGraphSelection& selection)
{
    SpatialGraphSelection handled(graph);
    SpatialGraphSelection baseNodesSel(graph);

    SpatialGraphSelection::Iterator it(selection);
    for (int v=it.vertices.nextSelected(); v!=-1; v=it.vertices.nextSelected()) {
        if (FilopodiaFunctions::nodeHasLocation(FILOPODIUM, v, graph) && !handled.isSelectedVertex(v)) {
            const int currentTime = FilopodiaFunctions::getTimeOfNode(graph, v);
            const int rootNode = FilopodiaFunctions::getRootNodeFromTimeStep(graph, currentTime);
            const SpatialGraphSelection pathToRoot = graph->getShortestPath(v, rootNode);
            const SpatialGraphSelection baseSel = FilopodiaFunctions::getNodesOfTypeInSelection(graph, pathToRoot, BASE_NODE);

            if (baseSel.getNumSelectedVertices() == 1) {
                baseNodesSel.selectVertex(baseSel.getSelectedVertex(0));
                const SpatialGraphSelection filo = FilopodiaFunctions::getFilopodiaSelectionOfNode(graph, baseSel.getSelectedVertex(0));
                handled.addSelection(filo);
            }
        }
    }

    for (int e=it.edges.nextSelected(); e!=-1; e=it.edges.nextSelected()) {
        if (FilopodiaFunctions::edgeHasLocation(FILOPODIUM, e, graph) && !handled.isSelectedEdge(e)) {
            const int currentTime = FilopodiaFunctions::getTimeOfNode(graph, graph->getEdgeSource(e));
            const int rootNode = FilopodiaFunctions::getRootNodeFromTimeStep(graph, currentTime);
            const SpatialGraphSelection pathToRoot = graph->getShortestPath(graph->getEdgeSource(e), rootNode);
            const SpatialGraphSelection baseSel = FilopodiaFunctions::getNodesOfTypeInSelection(graph, pathToRoot, BASE_NODE);

            if (baseSel.getNumSelectedVertices() == 1) {
                baseNodesSel.selectVertex(baseSel.getSelectedVertex(0));
                const SpatialGraphSelection filo = FilopodiaFunctions::getFilopodiaSelectionOfNode(graph, baseSel.getSelectedVertex(0));
                handled.addSelection(filo);
            }
        }
    }

    return baseNodesSel;
}

bool checkForTimeGaps(const QSet<int> timeSteps) {
    // Checks if all selected timesteps are successive
    // Returns true if there is a timestep missing inbetween
    // Returns false if there is no gap in list of timesteps

    int minTime = std::numeric_limits<int>::max();
    int maxTime = std::numeric_limits<int>::min();

    for (QSet<int>::const_iterator it=timeSteps.constBegin(); it!=timeSteps.constEnd(); ++it) {

        if (*it < minTime) minTime = *it;
        if (*it > maxTime) maxTime = *it;

    }

    if (float(float(maxTime-minTime + 1)/float(timeSteps.size())) != 1.0) return true;
    else return false;

}

// Given a selection of nodes or segments, the related base nodes are determined.
// These nodes are given the same ManualNodeMatch ID, so they are assigned the same
// FilopodiaID during tracking.
//
// In addition, when holding SHIFT, other base nodes that have the same Filopodia ID as one of the computed
// starting nodes are added to the set of base nodes and assigned the same manual node match ID.

void QxMatchFilopodiaTool::matchFilopodia(const int modifiers) {

    qDebug() << "\nMatch filopodia tool.";
    theMsg->printf("Match filopodia tool.");

    HxSpatialGraph* graph = mEditor->getSpatialGraph();
    const SpatialGraphSelection &selectedElements = mEditor->getHighlightedElements();

    SpatialGraphSelection baseNodesSel = getBaseNodesForSelectedElements(graph, selectedElements);
    if (baseNodesSel.getNumSelectedVertices() < 1) {
        return;
    }

    QSet<int> timeSteps;

    SpatialGraphSelection::Iterator it(baseNodesSel);
    for (int v=it.vertices.nextSelected(); v!=-1; v=it.vertices.nextSelected()) {
        const int t = FilopodiaFunctions::getTimeIdOfNode(graph, v);
        if (timeSteps.contains(t)) {
            HxMessage::error(QString("Could not match filopodia:\nCannot match node at same timestep %1.").arg(t), "ok" );
            return;
        }
        timeSteps.insert(t);
    }

    if (baseNodesSel.getNumSelectedVertices() > 1) {
        // If more than one base node is selected, check if timesteps are successive

        if (checkForTimeGaps(timeSteps)) {
            HxMessage::error(QString("Could not match filopodia:\nThe selected nodes have to be from successive timesteps.\n"
                                     "One or more timesteps are missing inbetween."), "ok" );
            return;
        }
    }

    const int unassignedId = FilopodiaFunctions::getMatchLabelId(graph, UNASSIGNED);

    if (modifiers & SHIFT_MODIFIER  && baseNodesSel.getNumSelectedVertices() == 2) {
        // If two base nodes from successive timesteps are selected and shift_modifier is pressed
        // the first base (smaller timestep ID) and its priors and the second base (higher timestep ID)
        // and its successors will get same ManualNodeMatch ID
        // nodes with UNASSIGNED label have no priors nor successors

        int time1 = FilopodiaFunctions::getTimeIdOfNode(graph, baseNodesSel.getSelectedVertex(0));
        int time2 = FilopodiaFunctions::getTimeIdOfNode(graph, baseNodesSel.getSelectedVertex(1));

        if (abs(time1-time2) != 1) {
            HxMessage::error(QString("Could not match filopodia:\nSelected nodes have to be from successive timesteps."), "ok" );
            return;
        }

        int node1 = baseNodesSel.getSelectedVertex(0);
        int node2 = baseNodesSel.getSelectedVertex(1);

        if (time1 > time2) {
            node1 = baseNodesSel.getSelectedVertex(1);
            node2 = baseNodesSel.getSelectedVertex(0);
            time1 = FilopodiaFunctions::getTimeIdOfNode(graph, node1);
            time2 = FilopodiaFunctions::getTimeIdOfNode(graph, node2);
        }

        SpatialGraphSelection addedSel(graph);

        if (modifiers & ALT_MODIFIER) {
            const int matchId1 = FilopodiaFunctions::getMatchIdOfNode(graph, node1);
            const int matchId2 = FilopodiaFunctions::getMatchIdOfNode(graph, node2);

            for (int v=0; v<graph->getNumVertices(); ++v) {
                if (v != node1 &&
                    FilopodiaFunctions::hasNodeType(graph, BASE_NODE, v) &&
                    FilopodiaFunctions::getMatchIdOfNode(graph, v) == matchId1 &&
                    matchId1 != unassignedId &&
                    FilopodiaFunctions::getTimeIdOfNode(graph, v) < time1)
                {
                    addedSel.selectVertex(v);
                }
                if (v != node2 &&
                    FilopodiaFunctions::hasNodeType(graph, BASE_NODE, v) &&
                    FilopodiaFunctions::getMatchIdOfNode(graph, v) == matchId2 &&
                    matchId2 != unassignedId &&
                    FilopodiaFunctions::getTimeIdOfNode(graph, v) > time2)
                {
                    addedSel.selectVertex(v);
                }
            }
        }
        else {
            const int filoId1 = FilopodiaFunctions::getFilopodiaIdOfNode(graph, node1);
            const int filoId2 = FilopodiaFunctions::getFilopodiaIdOfNode(graph, node2);

            for (int v=0; v<graph->getNumVertices(); ++v) {
                if (v != node1 &&
                    FilopodiaFunctions::hasNodeType(graph, BASE_NODE, v) &&
                    FilopodiaFunctions::getFilopodiaIdOfNode(graph, v) == filoId1 &&
                    filoId1 != unassignedId &&
                    FilopodiaFunctions::getTimeIdOfNode(graph, v) < time1)
                {
                    addedSel.selectVertex(v);
                }
                if (v != node2 &&
                    FilopodiaFunctions::hasNodeType(graph, BASE_NODE, v) &&
                    FilopodiaFunctions::getFilopodiaIdOfNode(graph, v) == filoId2 &&
                    filoId2 != unassignedId &&
                    FilopodiaFunctions::getTimeIdOfNode(graph, v) > time2)
                {
                    addedSel.selectVertex(v);
                }
            }
        }
        baseNodesSel.addSelection(addedSel);

    } else if ((modifiers & SHIFT_MODIFIER  && baseNodesSel.getNumSelectedVertices() == 1)) {
        // If one base node is selected and shift_modifier is pressed this node and its priors
        // and successors will get same ManualNodeMatch ID
        // nodes with UNASSIGNED label have no priors nor successors

        const int selectedNode = baseNodesSel.getSelectedVertex(0);
        SpatialGraphSelection addedSel(graph);

        if (modifiers & ALT_MODIFIER) {
            const int matchId = FilopodiaFunctions::getMatchIdOfNode(graph, selectedNode);

            for (int v=0; v<graph->getNumVertices(); ++v) {
                if (v != selectedNode &&
                    FilopodiaFunctions::hasNodeType(graph, BASE_NODE, v) &&
                    FilopodiaFunctions::getMatchIdOfNode(graph, v) == matchId &&
                    matchId != unassignedId)
                {
                    addedSel.selectVertex(v);
                }
            }
        }
        else {
            const int filoId = FilopodiaFunctions::getFilopodiaIdOfNode(graph, selectedNode);

            for (int v=0; v<graph->getNumVertices(); ++v) {
                if (v != selectedNode &&
                    FilopodiaFunctions::hasNodeType(graph, BASE_NODE, v) &&
                    FilopodiaFunctions::getFilopodiaIdOfNode(graph, v) == filoId &&
                    filoId != unassignedId)
                {
                    addedSel.selectVertex(v);
                }
            }
        }
        baseNodesSel.addSelection(addedSel);

    } else if ((modifiers & SHIFT_MODIFIER  && baseNodesSel.getNumSelectedVertices() > 2)) {
        HxMessage::error(QString("Could not match filopodia:\nPlease select at most two nodes if you want to match them with their priors and successors."), "ok" );
        return;

    } else if ((modifiers & CONTROL_MODIFIER  && baseNodesSel.getNumSelectedVertices() == 1)) {
        // If one base node is selected and ctrl_modifier is pressed this node and its successors
        // will be isolated by getting new ManualNodeMatch ID
        // nodes with UNASSIGNED label have no successors

        const int selectedNode = baseNodesSel.getSelectedVertex(0);
        const int timeId = FilopodiaFunctions::getTimeIdOfNode(graph, selectedNode);

        SpatialGraphSelection addedSel(graph);

        if (modifiers & ALT_MODIFIER) {
            const int matchId = FilopodiaFunctions::getMatchIdOfNode(graph, selectedNode);

            for (int v=0; v<graph->getNumVertices(); ++v) {
                if (v != selectedNode &&
                    FilopodiaFunctions::hasNodeType(graph, BASE_NODE, v) &&
                    FilopodiaFunctions::getMatchIdOfNode(graph, v) == matchId &&
                    matchId != unassignedId &&
                    FilopodiaFunctions::getTimeIdOfNode(graph, v) > timeId)
                {
                    addedSel.selectVertex(v);
                }
            }
        }
        else {
            const int filoId = FilopodiaFunctions::getFilopodiaIdOfNode(graph, selectedNode);

            for (int v=0; v<graph->getNumVertices(); ++v) {
                if (v != selectedNode &&
                    FilopodiaFunctions::hasNodeType(graph, BASE_NODE, v) &&
                    FilopodiaFunctions::getFilopodiaIdOfNode(graph, v) == filoId &&
                    filoId != unassignedId &&
                    FilopodiaFunctions::getTimeIdOfNode(graph, v) > timeId)
                {
                    addedSel.selectVertex(v);
                }
            }
        }

        baseNodesSel.addSelection(addedSel);

    } else if ((modifiers & CONTROL_MODIFIER  && baseNodesSel.getNumSelectedVertices() > 1)) {
        HxMessage::error(QString("Could not match filopodia:\nPlease select one node if you want to isolate it with its successors."), "ok" );
        return;
    }

    const int matchId = FilopodiaFunctions::getNextAvailableManualNodeMatchId(graph);
    QString matchName = QString::fromLatin1(FilopodiaFunctions::getManualNodeMatchAttributeName());
    try
    {
        AddAndAssignLabelOperationSet* labelOp = new AddAndAssignLabelOperationSet(graph,
                                                                                   baseNodesSel,
                                                                                   mEditor->getVisibleElements(),
                                                                                   matchName,
                                                                                   matchId);
        mEditor->execNewOp(labelOp);
    }
    catch (McException& e) {
        HxMessage::error(QString("Could not match filopodia:\n%1.").arg(e.what()), "ok" );
    }
}
