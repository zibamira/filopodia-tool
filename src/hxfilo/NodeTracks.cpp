#include "NodeTracks.h"
#include <mclib/McException.h>
#include <QString>
#include <QDebug>

Tracks::Tracks(const TimeMinMax &timeMinMax) :
    mTimeMinMax(timeMinMax)
{
}


int Tracks::addTrack() {
    const int numItems = mTimeMinMax.maxT - mTimeMinMax.minT + 1;

    Track t;
    t.id = nextUnusedTrackId();
    t.items.reserve(numItems);
    for (int i=0; i<numItems; ++i) {
        t.items.append(-1);
    }

    mTracksMap.insert(t.id, t);

    return t.id;
}


void Tracks::mergeTracks(const int t1, const int t2) {
    TracksMap::Iterator it1 = mTracksMap.find(t1);
    if (it1 == mTracksMap.end()) {
        qDebug() << "NodeTracks::mergeTracks: Track" << t1 << "does not exist";
        throw McException(QString("NodeTracks::mergeTracks: Track %1 does not exist").arg(t1));
    }

    TracksMap::ConstIterator it2 = mTracksMap.constFind(t2);
    if (it2 == mTracksMap.constEnd()) {
        qDebug() << "NodeTracks::mergeTracks: Track" << t2 << "does not exist";
        throw McException(QString("NodeTracks::mergeTracks: Track %1 does not exist").arg(t2));
    }

    Track& track1 = it1.value();
    const Track& track2 = it2.value();

    for (int i=0; i<track1.items.size(); ++i) {
        if ((track2.items[i] != -1) &&
            (track1.items[i] != -1) &&
            (track1.items[i] != track2.items[i]))
        {
            throw McException(QString("NodeTracks::mergeTracks: Cannot merge (node %1) of track %2 because it has already been matched.\nPlease check for nodes with same matchId in this timestep")
                                  .arg(track1.items[i])
                                  .arg(t1));
        }
    }

    for (int i=0; i<track1.items.size(); ++i) {
        if (track2.items[i] != -1) {
            track1.items[i] = track2.items[i];
            mItemToTrackMap[track2.items[i]] = t1;
        }
    }

    mTracksMap.remove(t2);
}


void Tracks::mergeTracks(const QSet<int> &trackIds)
{
    int targetTrack = -1;
    for (QSet<int>::ConstIterator it = trackIds.constBegin(); it!=trackIds.end(); ++it) {
        const int track = *it;
        if (targetTrack == -1) {
            targetTrack = track;
        }
        else {
            mergeTracks(targetTrack, track);
        }
    }
}


bool Tracks::tracksCanBeMerged(const QSet<int> &trackIds) const {
    int targetTrack = -1;
    ItemList merged;
    for (QSet<int>::ConstIterator it = trackIds.constBegin(); it!=trackIds.end(); ++it) {
        const int track = *it;
        if (targetTrack == -1) {
            targetTrack = track;
            merged = mTracksMap.value(track).items;
        }
        else {
            const ItemList& itemsToMerge = mTracksMap.value(track).items;
            for (int i=0; i<itemsToMerge.size(); ++i) {
                if ((merged[i] != -1) &&
                    (itemsToMerge[i] != -1) &&
                    (merged[i] != itemsToMerge[i]))
                {
                    return false;
                }
                merged[i] = itemsToMerge[i];
            }
        }
    }

    return true;
}


ItemList Tracks::getMatchedItems(const int trackId) const
{
    if (!mTracksMap.contains(trackId)) {
        qDebug() << "NodeTracks::getMatchedNodes: Track" << trackId << "does not exist";
        throw McException(QString("NodeTracks::getMatchedNodes: Track %1 does not exist").arg(trackId));
    }
    const ItemList& items = mTracksMap.value(trackId).items;
    ItemList matchedNodes;
    for (int i=0; i<items.size(); ++i) {
        if (items[i] >= 0) {
            matchedNodes.append(items[i]);
        }
    }
    return matchedNodes;
}


bool Tracks::exists(const int trackId) const {
    return mTracksMap.contains(trackId);
}


TimeMinMax Tracks::getTimeMinMax() const {
    return mTimeMinMax;
}


bool Tracks::isSubtrackEnd(const int item) const {
    ItemToTrackMap::ConstIterator itemIt = mItemToTrackMap.constFind(item);
    if (itemIt == mItemToTrackMap.constEnd()) {
        qDebug() << "Tracks::getMatchedItems: Item" << item << "not in map";
        throw McException(QString("Tracks::getMatchedItems: Item %1 not in map").arg(item));
    }

    const int track = itemIt.value();
    TracksMap::ConstIterator trackIt = mTracksMap.constFind(track);
    if (trackIt == mTracksMap.constEnd()) {

        qDebug() << "Tracks::getMatchedItems: Track" << track << "does not exist";
        throw McException(QString("Tracks::getMatchedItems: Track %1 does not exist").arg(track));
    }

    const ItemList& items = trackIt.value().items;
    const int index = items.indexOf(item);
    if (index == -1) {
        qDebug() << "Tracks::getMatchedItems: Item" << item << "not in track" << track;
        throw McException(QString("Tracks::getMatchedItems: Item %1 not in track %2").arg(item).arg(track));
    }

    return isSubtrackEnd(items, index);
}


bool Tracks::isSubtrackStart(const int item) const {
    ItemToTrackMap::ConstIterator itemIt = mItemToTrackMap.constFind(item);
    if (itemIt == mItemToTrackMap.constEnd()) {
        qDebug() << "Tracks::getMatchedItems: Item" << item << "not in map";
        throw McException(QString("Tracks::getMatchedItems: Item %1 not in map").arg(item));
    }

    const int track = itemIt.value();
    TracksMap::ConstIterator trackIt = mTracksMap.constFind(track);
    if (trackIt == mTracksMap.constEnd()) {
        qDebug() << "Tracks::getMatchedItems: Track" << track << "does not exist";
        throw McException(QString("Tracks::getMatchedItems: Track %1 does not exist").arg(track));
    }

    const ItemList& items = trackIt.value().items;
    const int index = items.indexOf(item);
    if (index == -1) {
        throw McException(QString("Tracks::getMatchedItems: Node %1 not in track %2").arg(item).arg(track));
    }

    return isSubtrackStart(items, index);
}


ItemList Tracks::getSubtrackEndItems(const int t) const {
    const int index = t - mTimeMinMax.minT;
    ItemList endItems;

    for (TracksMap::ConstIterator it=mTracksMap.constBegin(); it!=mTracksMap.constEnd(); ++it) {
        const Track& track = it.value();
        if (isSubtrackEnd(track.items, index)) {
            endItems.append(track.items[index]);
        }
    }
    return endItems;
}


ItemList Tracks::getSubtrackStartItems(const int t) const {
    const int index = t - mTimeMinMax.minT;
    ItemList startItems;

    for (TracksMap::ConstIterator it=mTracksMap.constBegin(); it!=mTracksMap.constEnd(); ++it) {
        const Track& track = it.value();
        if (isSubtrackStart(track.items, index)) {
            startItems.append(track.items[index]);
        }
    }
    return startItems;
}


ItemList Tracks::getTrackIds() const {
    return mTracksMap.keys();
}


int Tracks::getTrackOfItem(const int n) const {
    ItemToTrackMap::ConstIterator it = mItemToTrackMap.find(n);
    if (it == mItemToTrackMap.constEnd()) {
        return -1;
    }
    return it.value();
}


bool Tracks::isSubtrackEnd(const ItemList &items, const int index) const {
    if (items[index] == -1) {
        return false;
    }
    if (index == items.size()-1) {
        return false; // Do not consider node in last time step a track end, as it cannot be merged with track start
    }
    else if (items[index+1] == -1) {
        return true;
    }
    return false;
}


bool Tracks::isSubtrackStart(const ItemList &items, const int index) const {
    if (items[index] == -1) {
        return false;
    }
    if (index == 0) {
        return false; // Do not consider node in first time step a track start, as it cannot be merged with track end
    }
    if (items[index-1] == -1) {
        return true;
    }
    return false;
}


int Tracks::nextUnusedTrackId() const
{
    int id = mTracksMap.size();
    while (mTracksMap.contains(id)) {
        ++id;
    }
    return id;
}


void Tracks::addItemToTrack(const int trackId, const int time, const int item) {
    if (!mTracksMap.contains(trackId)) {
        qDebug() << "Tracks::addItemToTrack: Track" << trackId << "does not exist";
        throw McException(QString("Tracks::addItemToTrack: Track %1 does not exist").arg(trackId));
    }
    if (time < mTimeMinMax.minT || time > mTimeMinMax.maxT) {
        qDebug() << "Tracks::addItemToTrack: invalid time";
        throw McException(QString("Tracks::addItemToTrack: invalid time").arg(time));
    }
    ItemList& items = mTracksMap[trackId].items;
    items[time - mTimeMinMax.minT] = item;
    mItemToTrackMap[item] = trackId;
}


TrackingModel::TrackingModel(const TimeMinMax &timeMinMax) :
    mNodeTracks(timeMinMax),
    mFiloTracks(timeMinMax)
{
}


TimeMinMax TrackingModel::getTimeMinMax() const {
    return mFiloTracks.getTimeMinMax();
}


int TrackingModel::addFilo(const int startingNode, const int time, const ItemList &nodes)
{
    Filo filo;
    filo.id = mFiloMap.size();
    filo.nodes = nodes;
    filo.time = time;
    filo.startingNode = startingNode;

    mFiloMap.insert(filo.id, filo);

    const int filoTrackId = mFiloTracks.addTrack();
    mFiloTracks.addItemToTrack(filoTrackId, filo.time, filo.id);

    for (int n=0; n<nodes.size(); ++n) {
        mNodeToFiloMap.insert(nodes[n], filo.id);

        const int nodeTrackId = mNodeTracks.addTrack();
        mNodeTracks.addItemToTrack(nodeTrackId, filo.time, nodes[n]);
    }
    return filo.id;
}


void TrackingModel::setMatchedNodes(const ItemList &nodes)
{
    QSet<int> filoTracks;
    QSet<int> nodeTracks;
    QSet<int> startingNodeTracks;

    bool containsStartingNode = false;
    bool containsNonStartingNode = false;

    for (int n=0; n<nodes.size(); ++n) {
        const int filo = getFiloOfNode(nodes[n]);
        const int filoTrack = mFiloTracks.getTrackOfItem(filo);
        filoTracks.insert(filoTrack);

        const int startingNode = getBase(filo);
        if (startingNode == nodes[n]) {
            containsStartingNode = true;
        }
        else {
            containsNonStartingNode = true;
            const int startingNodeTrack = mNodeTracks.getTrackOfItem(startingNode);
            startingNodeTracks.insert(startingNodeTrack);
        }

        const int nodeTrack = mNodeTracks.getTrackOfItem(nodes[n]);
        nodeTracks.insert(nodeTrack);
    }

    if (containsStartingNode && containsNonStartingNode) {
        qDebug() << "setMatchedNodes: contains starting and non starting nodes!";
        for (int i=0; i<nodes.size();++i) {
            qDebug() << "\t" << nodes[i];
        }
        throw McException(QString("Cannot set nodes to matched: contains both starting and non-starting nodes"));
    }

    if (mNodeTracks.tracksCanBeMerged(nodeTracks) &&
        mNodeTracks.tracksCanBeMerged(startingNodeTracks) &&
        mFiloTracks.tracksCanBeMerged(filoTracks))
    {
        mNodeTracks.mergeTracks(nodeTracks);
        mNodeTracks.mergeTracks(startingNodeTracks);
        mFiloTracks.mergeTracks(filoTracks);
    }
}


int TrackingModel::getFiloOfNode(const int n) const {
    NodeToFiloMap::ConstIterator it = mNodeToFiloMap.find(n);
    if (it == mNodeToFiloMap.constEnd()) {
        qDebug() << "getFiloOfNode: invalid node" << n;
        throw McException(QString("TrackingModel::getFiloOfNode: invalid node %1").arg(n));
    }
    return it.value();
}


int TrackingModel::getBase(const int filoId) const {
    FiloMap::ConstIterator it = mFiloMap.find(filoId);
    if (it == mFiloMap.constEnd()) {
        qDebug() << "TrackingModel::getBase: invalid filoId" << filoId;
        throw McException(QString("TrackingModel::getBase: invalid filoId %1").arg(filoId));
    }
    return it.value().startingNode;
}


ItemList TrackingModel::getBases(const int filoTrackId) const
{
    ItemList startingNodes;
    const ItemList filosInTrack = mFiloTracks.getMatchedItems(filoTrackId);
    for (int i=0; i<filosInTrack.size(); ++i) {
        const int filoId = filosInTrack[i];
        startingNodes.append(getBase(filoId));
    }
    return startingNodes;
}


int TrackingModel::getTrackOfFilo(const int filoId) const {
    return mFiloTracks.getTrackOfItem(filoId);
}


ItemList TrackingModel::getNodeTrackIds() const {
    return mNodeTracks.getTrackIds();
}


ItemList TrackingModel::getMatchedNodes(const int trackId) const {
    return mNodeTracks.getMatchedItems(trackId);
}


ItemList TrackingModel::getFilopodiaTrackIds() const {
    return mFiloTracks.getTrackIds();
}


ItemList TrackingModel::getMatchedFilopodia(const int trackId) const {
    return mFiloTracks.getMatchedItems(trackId);
}


ItemList TrackingModel::getFilopodiaSubtrackEnds(const int time) const {
    return mFiloTracks.getSubtrackEndItems(time);
}


ItemList TrackingModel::getFilopodiaSubtrackStarts(const int time) const {
    return mFiloTracks.getSubtrackStartItems(time);
}
