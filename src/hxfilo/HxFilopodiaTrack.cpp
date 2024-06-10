#include "HxFilopodiaTrack.h"
#include "FilopodiaFunctions.h"
#include <hxspatialgraph/internal/HxSpatialGraph.h>
#include "pointmatching/ExactPointMatchingAlgorithm.h"
#include "pointmatching/IterativePointMatching.h"
#include "pointmatching/PointMatchingDataStruct.h"
#include "pointmatching/SimplePointRepresentation3d.h"
#include "pointmatching/Transformation.h"
#include <mclib/McException.h>
#include <mclib/McBitfield.h>
#include <QDebug>
#include <QTime>
#include <QtAlgorithms>

#include "ConvertVectorAndMcDArray.h"

typedef QMap<int, QFileInfo> FileInfoMap; /// Key is time step


HX_INIT_CLASS(HxFilopodiaTrack,HxCompModule)

HxFilopodiaTrack::HxFilopodiaTrack() :
    HxCompModule(HxSpatialGraph::getClassTypeId()),
    portDistanceThreshold(this, tr("DistanceThreshold"), tr("DistanceThreshold")),
    portMergeManualNodeMatch(this, tr("MergeManualNodeMatch"), tr("Allow merge of different manual node matches"), 1),
    portAction(this, tr("action"), tr("Action"))
{
    portDistanceThreshold.setValue(1.0f);
}

HxFilopodiaTrack::~HxFilopodiaTrack()
{
}


struct PairDistance {
    int v1;
    int v2;
    float dist;

    bool operator<(const PairDistance& other) const {
        return dist < other.dist;
    }
};


// Set all nodes and segments that are part of growth cone to IGNORED.
void assignIgnoredFilopodiaLabels(HxSpatialGraph* graph) {
    EdgeVertexAttribute* vFiloAtt = graph->findVertexAttribute(FilopodiaFunctions::getFilopodiaAttributeName());
    EdgeVertexAttribute* eFiloAtt = graph->findEdgeAttribute(FilopodiaFunctions::getFilopodiaAttributeName());
    if ( (!vFiloAtt) || (!eFiloAtt) ){
        throw McException(QString("Attribute %1 not found.").arg(QString::fromLatin1(FilopodiaFunctions::getFilopodiaAttributeName())));
    }

    const int ignoredLabel   = FilopodiaFunctions::getFilopodiaLabelId(graph, IGNORED);

    for (int v=0; v<graph->getNumVertices(); ++v) {
        if (FilopodiaFunctions::nodeHasLocation(GROWTHCONE, v, graph)) {
            vFiloAtt->setIntDataAtIdx(v, ignoredLabel);
        }
    }

    for (int e=0; e<graph->getNumEdges(); ++e) {
        if (FilopodiaFunctions::edgeHasLocation(GROWTHCONE, e, graph)) {
            eFiloAtt->setIntDataAtIdx(e, ignoredLabel);
        }
    }
}


TrackingModel HxFilopodiaTrack::createTrackingModel(const HxSpatialGraph* graph) {
    const EdgeVertexAttribute* typeAtt = graph->findVertexAttribute(FilopodiaFunctions::getTypeLabelAttributeName());
    if (!typeAtt) {
        throw McException("createTrackingModel: Missing node type attribute.");
    }

    const TimeMinMax timeMinMax = FilopodiaFunctions::getTimeMinMaxFromGraphLabels(graph);
    TrackingModel model(timeMinMax);

    for (int v=0; v<graph->getNumVertices(); ++v) {
        if (FilopodiaFunctions::hasNodeType(graph, BASE_NODE, v)) {
            const int time = FilopodiaFunctions::getTimeOfNode(graph, v);
            const SpatialGraphSelection sel = FilopodiaFunctions::getFilopodiumSelectionFromNode(graph, v);
            QList<int> nodes;

            SpatialGraphSelection::Iterator it(sel);
            for (int n=it.vertices.nextSelected(); n!=-1; n=it.vertices.nextSelected()) {
                nodes.append(n);
            }

            model.addFilo(v, time, nodes);
        }
    }

    return model;
}


void HxFilopodiaTrack::processManualNodeMatch(const HxSpatialGraph* graph, TrackingModel& model) {
    const EdgeVertexAttribute* matchAtt = graph->findVertexAttribute(FilopodiaFunctions::getManualNodeMatchAttributeName());
    if (!matchAtt) {
        throw McException("FilopodiaTrack: Missing node match attribute.");
    }

    const int unassigned = FilopodiaFunctions::getMatchLabelId(graph, UNASSIGNED);
    const int ignored = FilopodiaFunctions::getMatchLabelId(graph, IGNORED);

    QHash<int, QList<int> > matchGroups;

    for (int v=0; v<graph->getNumVertices(); ++v) {
        const int matchId = FilopodiaFunctions::getMatchIdOfNode(graph, v);
        if ( (matchId == unassigned) || (matchId == ignored) || (matchId == 0) ) {
            continue;
        }

        QHash<int, QList<int> >::iterator it = matchGroups.find(matchId);

        if (it == matchGroups.end()) {
            QList<int> list;
            list.append(v);
            matchGroups.insert(matchId, list);
        }
        else {
            QList<int>& list = it.value();
            list.append(v);
        }
    }

    for (QHash<int, QList<int> >::ConstIterator iter=matchGroups.constBegin(); iter!=matchGroups.constEnd(); ++iter) {
        try {
            model.setMatchedNodes(iter.value());
        }
        catch (McException& e) {
            HxMessage::error(QString("Error during processing manual node match: cannot match %1 nodes. Skipping.\n(%2)").arg(iter.value().size()).arg(e.what()), "ok" );
        }
    }
}


// Set all nodes/segments that are either part of growth cone or DISCARDED to IGNORED.
// Set all nodes/segments that are part of filopodium to UNASSIGNED.
void initializeMatchLabels(HxSpatialGraph* graph, const char* attName) {
    EdgeVertexAttribute* vMatchAtt = graph->findVertexAttribute(attName);
    EdgeVertexAttribute* eMatchAtt = graph->findEdgeAttribute(attName);
    if (!vMatchAtt && !eMatchAtt) {
        throw McException(QString("Match attribute %1 not found.").arg(QString::fromLatin1(attName)));
    }

    const int unassignedLabel = FilopodiaFunctions::getMatchLabelId(graph, UNASSIGNED);
    const int ignoredLabel    = FilopodiaFunctions::getMatchLabelId(graph, IGNORED);

    if (vMatchAtt) {
        for (int v=0; v<graph->getNumVertices(); ++v) {
            if (FilopodiaFunctions::nodeHasLocation(GROWTHCONE, v, graph)) {
                vMatchAtt->setIntDataAtIdx(v, ignoredLabel);
            }
            else if (FilopodiaFunctions::nodeHasLocation(FILOPODIUM, v, graph)) {
                vMatchAtt->setIntDataAtIdx(v, unassignedLabel);
            }
            else {
                throw McException(QString("Invalid location label (node %1)").arg(v));
            }
        }
    }

    if (eMatchAtt) {
        for (int e=0; e<graph->getNumEdges(); ++e) {
            if (FilopodiaFunctions::edgeHasLocation(GROWTHCONE, e, graph)) {
                eMatchAtt->setIntDataAtIdx(e, ignoredLabel);
            }
            else if (FilopodiaFunctions::edgeHasLocation(FILOPODIUM, e, graph)) {
                eMatchAtt->setIntDataAtIdx(e, unassignedLabel);
            }
            else {
                throw McException(QString("Invalid location label (segment %1)").arg(e));
            }
        }
    }
}


struct SelectionMatch {
    float matchScore;
    int trackId;
    int labelId;
};

bool selectionMatchGreaterThan(const SelectionMatch& m1, const SelectionMatch& m2) {
    return m1.matchScore > m2.matchScore;
}


HxNeuronEditorSubApp::SpatialGraphChanges addFilopodiaLabels(HxSpatialGraph* graph, const TrackingModel& model) {
    qDebug() << "    ASSIGN FILOPODIA LABELS";
    const char* filopodiaAttName = FilopodiaFunctions::getFilopodiaAttributeName();

    HxNeuronEditorSubApp::SpatialGraphChanges changes = 0;

    const EdgeVertexAttribute* eAtt = graph->findEdgeAttribute(filopodiaAttName);
    const EdgeVertexAttribute* vAtt = graph->findVertexAttribute(filopodiaAttName);

    if (!eAtt || !vAtt) {
        FilopodiaFunctions::addOrClearFilopodiaLabelAttribute(graph); // Resets everything to UNASSIGNED
        changes = HxNeuronEditorSubApp::SpatialGraphLabelTreeChange;
    }

    QMap<int, SpatialGraphSelection> selectionPerLabel;
    QMap<int, SpatialGraphSelection> selectionPerTrack;

    // Fill selection per existing label
    const HierarchicalLabels* filoLabels = graph->getLabelGroup(filopodiaAttName);
    const McDArray<int> existingLabelIds = filoLabels->getChildIds(0);

    for (int i=0; i<existingLabelIds.size(); ++i) {
        selectionPerLabel.insert(existingLabelIds[i], SpatialGraphSelection(graph));
    }
    for (int v=0; v<graph->getNumVertices(); ++v) {
        const int filoId = vAtt->getIntDataAtIdx(v);
        SpatialGraphSelection& sel = selectionPerLabel[filoId];
        sel.selectVertex(v);
    }
    for (int e=0; e<graph->getNumEdges(); ++e) {
        const int filoId = eAtt->getIntDataAtIdx(e);
        SpatialGraphSelection& sel = selectionPerLabel[filoId];
        sel.selectEdge(e);
    }

    const SpatialGraphSelection oldIgnored = selectionPerLabel[FilopodiaFunctions::getFilopodiaLabelId(graph, IGNORED)];
    const SpatialGraphSelection oldUnassigned = selectionPerLabel[FilopodiaFunctions::getFilopodiaLabelId(graph, UNASSIGNED)];

    selectionPerLabel.remove(0); // Do not reuse root label
    selectionPerLabel.remove(FilopodiaFunctions::getFilopodiaLabelId(graph, UNASSIGNED));
    selectionPerLabel.remove(FilopodiaFunctions::getFilopodiaLabelId(graph, AXON));
    selectionPerLabel.remove(FilopodiaFunctions::getFilopodiaLabelId(graph, IGNORED));

    // Fill selection per track ID
    const ItemList trackIds = model.getFilopodiaTrackIds();
    for (int i=0; i<trackIds.size(); ++i) {
        const int trackId = trackIds.at(i);

        SpatialGraphSelection sel(graph);
        const ItemList matchedFilos = model.getMatchedFilopodia(trackId);
        for (int n=0; n<matchedFilos.size(); ++n) {
            const int filo = matchedFilos.at(n);
            const int baseNode = model.getBase(filo);
            sel.addSelection(FilopodiaFunctions::getFilopodiumSelectionFromNode(graph, baseNode));
        }

        selectionPerTrack.insert(trackId, sel);
    }

    // In 'assigned', we collect all vertices and edges which have been checked.
    // When a vertex/edge is selected in 'assigned', it may or may not have become a
    // new label. It is marked as assigned, so we can ensure that all vertices/edges
    // have been processed.
    SpatialGraphSelection assigned(graph);

    QList<SelectionMatch> potentialMatches;
    QSet<int> tracksToAssign = trackIds.toSet();

    for (QMap<int, SpatialGraphSelection>::ConstIterator trackIt=selectionPerTrack.constBegin();
         trackIt != selectionPerTrack.constEnd(); ++trackIt)
    {
        for (QMap<int, SpatialGraphSelection>::ConstIterator labelIt=selectionPerLabel.constBegin();
             labelIt != selectionPerLabel.constEnd(); ++labelIt)
        {
            const SpatialGraphSelection& trackSel = trackIt.value();
            const SpatialGraphSelection& labelSel = labelIt.value();
            if (trackSel == labelSel) {
                // Track and label selection are the same -> reuse label
                assigned.addSelection(trackSel);
                selectionPerLabel.remove(labelIt.key());
                tracksToAssign.remove(trackIt.key());
                break;
            }
            else {
                SpatialGraphSelection inCommon(trackSel);
                inCommon &= labelSel;
                if (inCommon.getNumSelectedEdges() > 0) {
                    // Reuse as many labels as possible. Try to keep existing label ids as much as possible.
                    // Therefore, sort by number of edges that track and label selection have in common.
                    SelectionMatch m;
                    m.matchScore = inCommon.getNumSelectedEdges();
                    m.labelId = labelIt.key();
                    m.trackId = trackIt.key();
                    potentialMatches.append(m);
                }
            }
        }
    }

    QSet<int> labelsForReuse = selectionPerLabel.keys().toSet();
    qSort(potentialMatches.begin(), potentialMatches.end(), selectionMatchGreaterThan);

    // Reuse existing labels considering existing assignment
    for (int i=0; i<potentialMatches.size(); ++i) {
        const SelectionMatch& m = potentialMatches[i];
        if (tracksToAssign.contains(m.trackId) &&
            labelsForReuse.contains(m.labelId))
        {
            AssignLabelOperation op(graph, selectionPerTrack[m.trackId], SpatialGraphSelection(graph),
                                    QString::fromLatin1(filopodiaAttName), m.labelId);
            op.exec();
            assigned.addSelection(selectionPerTrack[m.trackId]);
            labelsForReuse.remove(m.labelId);
            tracksToAssign.remove(m.trackId);
            changes |= HxNeuronEditorSubApp::SpatialGraphLabelChange;
        }
    }

    // Reuse existing labels for any remaining tracks
    for (QSet<int>::ConstIterator trackIt = tracksToAssign.constBegin(); trackIt!=tracksToAssign.constEnd(); ++trackIt) {
        if (labelsForReuse.isEmpty()) {
            const int labelId = FilopodiaFunctions::getNextAvailableFilopodiaId(graph);
            McString newName;
            filoLabels->getLabelName(labelId, newName);
            qDebug() << "        NEW LABEL:   " << labelId << newName.getString();

            if (!selectionPerTrack[*trackIt].isEmpty()) {
                AssignLabelOperation op(graph, selectionPerTrack[*trackIt], SpatialGraphSelection(graph),
                                        QString::fromLatin1(filopodiaAttName), labelId);
                op.exec();
                assigned.addSelection(selectionPerTrack[*trackIt]);
                changes |= HxNeuronEditorSubApp::SpatialGraphLabelTreeChange;
            }
        }
        else {
            const int labelId = labelsForReuse.values().first();
            AssignLabelOperation op(graph, selectionPerTrack[*trackIt], SpatialGraphSelection(graph),
                                    QString::fromLatin1(filopodiaAttName), labelId);
            op.exec();
            assigned.addSelection(selectionPerTrack[*trackIt]);
            labelsForReuse.remove(labelId);
            changes |= HxNeuronEditorSubApp::SpatialGraphLabelChange;
        }
    }

    SpatialGraphSelection unassigned(assigned);
    unassigned.toggleAll();

    SpatialGraphSelection newUnassigned(graph);
    SpatialGraphSelection newIgnored(graph);

    // Set remaining unassigned vertices/edges to
    // UNASSIGNED if part of Filopodium
    // IGNORED    if part of GrowthCone
    // But only if they are not labeled as such already.
    SpatialGraphSelection::Iterator selIt(unassigned);
    for (int v=selIt.vertices.nextSelected(); v!=-1; v=selIt.vertices.nextSelected()) {
        if (FilopodiaFunctions::nodeHasLocation(GROWTHCONE, v, graph)) {
            if (oldIgnored.isSelectedVertex(v)) {
                assigned.selectVertex(v);
            }
            else {
                newIgnored.selectVertex(v);
            }
        }
        else if (FilopodiaFunctions::nodeHasLocation(FILOPODIUM, v, graph)) {
            if (oldUnassigned.isSelectedVertex(v)) {
                assigned.selectVertex(v);
            }
            else {
                newUnassigned.selectVertex(v);
            }
        }
    }
    for (int e=selIt.edges.nextSelected(); e!=-1; e=selIt.edges.nextSelected()) {
        if (FilopodiaFunctions::edgeHasLocation(GROWTHCONE, e, graph)) {
            if (oldIgnored.isSelectedEdge(e)) {
                assigned.selectEdge(e);
            }
            else {
                newIgnored.selectEdge(e);
            }
        }
        else if (FilopodiaFunctions::edgeHasLocation(FILOPODIUM, e, graph)) {
            if (oldUnassigned.isSelectedEdge(e)) {
                assigned.selectEdge(e);
            }
            else {
                newUnassigned.selectEdge(e);
            }
        }
    }

    if (!newUnassigned.isEmpty()) {
        AssignLabelOperation op(graph, newUnassigned, SpatialGraphSelection(graph),
                                QString::fromLatin1(filopodiaAttName),
                                FilopodiaFunctions::getFilopodiaLabelId(graph, UNASSIGNED));
        op.exec();
        changes |= HxNeuronEditorSubApp::SpatialGraphLabelChange;
        assigned.addSelection(newUnassigned);
    }
    if (!newIgnored.isEmpty()) {
        AssignLabelOperation op(graph, newIgnored, SpatialGraphSelection(graph),
                                QString::fromLatin1(filopodiaAttName),
                                FilopodiaFunctions::getFilopodiaLabelId(graph, IGNORED));
        op.exec();
        changes |= HxNeuronEditorSubApp::SpatialGraphLabelChange;
        assigned.addSelection(newIgnored);
    }

    if (assigned.getNumSelectedVertices() != assigned.getNumVertices()) {
        qDebug() << "        ERROR: not all vertices assigned. Missing:" << assigned.getNumVertices() - assigned.getNumSelectedVertices();
    }
    if (assigned.getNumSelectedEdges() != assigned.getNumEdges()) {
        qDebug() << "        ERROR: not all edges assigned. Missing:" << assigned.getNumEdges() - assigned.getNumSelectedEdges();
    }

    return changes;
}


void matchNonBaseNodesForMatchedFilo(const HxSpatialGraph* graph,
                                         const int filo1, const int filo2,
                                         const float distThreshold,
                                         const bool mergeManualNodeTracks,
                                         TrackingModel& model)
{

    const int ignored = FilopodiaFunctions::getMatchLabelId(graph, IGNORED);
    const int unassigned = FilopodiaFunctions::getMatchLabelId(graph, UNASSIGNED);

    const int base1 = model.getBase(filo1);
    const int base2 = model.getBase(filo2);

    const SpatialGraphSelection filoSel1 = FilopodiaFunctions::getFilopodiumSelectionFromNode(graph, base1);
    const SpatialGraphSelection filoSel2 = FilopodiaFunctions::getFilopodiumSelectionFromNode(graph, base2);

    std::vector<PairDistance> distances;

    SpatialGraphSelection::Iterator it1(filoSel1);
    for (int v1=it1.vertices.nextSelected(); v1!=-1; v1=it1.vertices.nextSelected()) {
        const int type1 = FilopodiaFunctions::getTypeIdOfNode(graph, v1);
        if (!FilopodiaFunctions::hasNodeType(graph, TIP_NODE, v1) && !FilopodiaFunctions::hasNodeType(graph, BRANCHING_NODE, v1)) {
            continue;
        }
        const McVec3f p1 = graph->getVertexCoords(v1);

        SpatialGraphSelection::Iterator it2(filoSel2);
        for (int v2=it2.vertices.nextSelected(); v2!=-1; v2=it2.vertices.nextSelected()) {
            const int type2 =  FilopodiaFunctions::getTypeIdOfNode(graph, v2);
            if (!FilopodiaFunctions::hasNodeType(graph, TIP_NODE, v2) && !FilopodiaFunctions::hasNodeType(graph, BRANCHING_NODE, v2)) {
                continue;
            }

            if (type1 != type2) {
                continue;
            }

            const int manualMatch1 = FilopodiaFunctions::getMatchIdOfNode(graph, v1);
            const int manualMatch2 = FilopodiaFunctions::getMatchIdOfNode(graph, v2);

            if (manualMatch1 != ignored && manualMatch1 != unassigned &&
                manualMatch2 != ignored && manualMatch2 != unassigned &&
                manualMatch1 != manualMatch2 &&
                !mergeManualNodeTracks)
            {
                // Nodes have different manual match id
                // and user does not want to merge their tracks.
                continue;
            }

            const McVec3f p2 = graph->getVertexCoords(v2);

            PairDistance d;
            d.v1 = v1;
            d.v2 = v2;
            d.dist = (p1-p2).length();
            if (d.dist < distThreshold) {
                auto it = std::upper_bound(distances.begin(), distances.end(), d);
                distances.insert(it,d);
            }
        }
    }

    for (int i=0; i<distances.size(); ++i) {
        const PairDistance& pd = distances[i];
        ItemList matchedNodes;
        matchedNodes.append(pd.v1);
        matchedNodes.append(pd.v2);
        try {
            model.setMatchedNodes(matchedNodes);
        }
        catch (McException& e) {
            HxMessage::error(QString("Error matching nodes: %1 and %2 are not bases. Skipping.\n(%3)")
                             .arg(pd.v1).arg(pd.v2).arg(e.what()), "ok" );
        }
    }
}


void HxFilopodiaTrack::matchAcrossSingleTimeStep(TrackingModel& model,
                               const HxSpatialGraph* graph,
                               const float distThreshold,
                               const bool mergeManualNodeTracks)
{
    const EdgeVertexAttribute* matchAtt = graph->findVertexAttribute(FilopodiaFunctions::getManualNodeMatchAttributeName());
    if (!matchAtt) {
        throw McException("matchAcrossSingleTimeStep: no ManualNodeMatch attribute ");
    }
    const int ignored = FilopodiaFunctions::getMatchLabelId(graph, IGNORED);
    const int unassigned = FilopodiaFunctions::getMatchLabelId(graph, UNASSIGNED);

    const TimeMinMax timeMinMax = model.getTimeMinMax();
    for (int time=timeMinMax.minT; time<timeMinMax.maxT; ++time) {
        const ItemList filoTrackEnds = model.getFilopodiaSubtrackEnds(time);
        const ItemList filoTrackStarts = model.getFilopodiaSubtrackStarts(time+1);

        std::vector<PairDistance> distances;

        // Match bases
        for (int fe=0; fe<filoTrackEnds.size(); ++fe) {
            const int filo1 = filoTrackEnds.at(fe);
            const int base1 = model.getBase(filo1);
            const McVec3f p1 = graph->getVertexCoords(base1);

            for (int fs=0; fs<filoTrackStarts.size(); ++fs) {
                const int filo2 = filoTrackStarts.at(fs);
                const int base2 = model.getBase(filo2);
                const McVec3f p2 = graph->getVertexCoords(base2);

                const int manualMatch1 = FilopodiaFunctions::getMatchIdOfNode(graph, base1);
                const int manualMatch2 = FilopodiaFunctions::getMatchIdOfNode(graph, base2);

                if (manualMatch1 != ignored && manualMatch1 != unassigned &&
                    manualMatch2 != ignored && manualMatch2 != unassigned &&
                    manualMatch1 != manualMatch2 &&
                    !mergeManualNodeTracks)
                {
                    // Nodes have different manual match id
                    // and user does not want to merge their tracks.
                    continue;
                }

                if (!mergeManualNodeTracks) {
                    const int track1 = model.getTrackOfFilo(filo1);
                    const int track2 = model.getTrackOfFilo(filo2);
                    const ItemList bases1 = model.getBases(track1);
                    const ItemList bases2 = model.getBases(track2);

                    QSet<int> manualMatchIdsInTrack1;
                    for (int i=0; i<bases1.size(); ++i) {
                        const int matchId = FilopodiaFunctions::getMatchIdOfNode(graph, bases1[i]);
                        if (matchId != ignored && matchId != unassigned) {
                            manualMatchIdsInTrack1.insert(matchId);
                        }
                    }

                    QSet<int> manualMatchIdsInTrack2;
                    for (int i=0; i<bases2.size(); ++i) {
                        const int matchId = FilopodiaFunctions::getMatchIdOfNode(graph, bases2[i]);
                        if (matchId != ignored && matchId != unassigned) {
                            manualMatchIdsInTrack2.insert(matchId);
                        }
                    }

                    if (manualMatchIdsInTrack1.size() >= 2 || manualMatchIdsInTrack2.size() >= 2) {
                        throw McException("Error matching across single time step: subtrack with multiple manual node match ids exists");
                    }

                    if ((manualMatchIdsInTrack1.size() == 1) &&
                        (manualMatchIdsInTrack2.size() == 1) &&
                        (manualMatchIdsInTrack1.values()[0] != manualMatchIdsInTrack2.values()[0]))
                    {
                        // The two subtracks each have a base with a manual node match id
                        // somewhere in the track. However, these manual node match ids differ.
                        // Therefore, the tracks cannot be merged.
                        continue;
                    }
                }

                PairDistance d;
                d.v1 = base1;
                d.v2 = base2;
                d.dist = (p1-p2).length();
                if (d.dist < distThreshold){
                    auto it = std::upper_bound(distances.begin(), distances.end(), d);
                    distances.insert(it,d);
                }
            }
        }

        // Match non-base nodes for matched filopodia
        for (int i=0; i<distances.size(); ++i) {
            const PairDistance& pd = distances[i];
            ItemList matchedNodes;
            matchedNodes.append(pd.v1);
            matchedNodes.append(pd.v2);
            try {
                model.setMatchedNodes(matchedNodes);
                matchNonBaseNodesForMatchedFilo(graph, model.getFiloOfNode(pd.v1), model.getFiloOfNode(pd.v2),
                                                    distThreshold, mergeManualNodeTracks, model);
            }
            catch (McException& e) {
                HxMessage::error(QString("Error matching bases: cannot match bases %1 and %2. Skipping.\n(%3)")
                                 .arg(pd.v1).arg(pd.v2).arg(e.what()), "ok" );
            }

        }
    }
}


HxNeuronEditorSubApp::SpatialGraphChanges
HxFilopodiaTrack::trackFilopodia(HxSpatialGraph* graph,
                                 const float distThreshold,
                                 const bool mergeManualNodeTracks)
{
    qDebug() << "TRACKING START";
    const QTime startTimeTrack = QTime::currentTime();

    TrackingModel model = createTrackingModel(graph);

    processManualNodeMatch(graph, model);
    matchAcrossSingleTimeStep(model, graph, distThreshold, mergeManualNodeTracks);

    const HxNeuronEditorSubApp::SpatialGraphChanges changes = addFilopodiaLabels(graph, model);

    if (changes == 0) {
        qDebug() << "    CHANGES: ---";
    }
    if (changes & HxNeuronEditorSubApp::SpatialGraphLabelChange) {
        qDebug() << "    CHANGES: LabelChange";
    }
    if (changes & HxNeuronEditorSubApp::SpatialGraphLabelTreeChange) {
        qDebug() << "    CHANGES: LabelTreeChange";
    }

    const QTime endTimeTrack = QTime::currentTime();

    qDebug() << "    TIME:" << startTimeTrack.msecsTo(endTimeTrack) << "msec";
    qDebug() << "TRACKING END";

    return changes;
}



void HxFilopodiaTrack::compute() {
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
        outputGraph->composeLabel(inputGraph->getLabel(), "track");

        const float distThreshold = portDistanceThreshold.getValue();
        const bool mergeManualNodeTracks = portMergeManualNodeMatch.getValue();

        FilopodiaFunctions::addManualNodeMatchLabelAttribute(outputGraph);

        trackFilopodia(outputGraph, distThreshold, mergeManualNodeTracks);

        setResult(outputGraph);
    }
}

