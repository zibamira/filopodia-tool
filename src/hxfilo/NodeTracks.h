#ifndef TRACKS_H
#define TRACKS_H

#include "FilopodiaFunctions.h"
#include <QHash>
#include <QList>

typedef QList<int> ItemList;

struct Track {
    int id;
    ItemList items;
};


struct Filo {
    int id;
    int time;
    int startingNode;
    ItemList nodes;
};

typedef QHash<int, Track> TracksMap;
typedef QHash<int, int>   ItemToTrackMap;
typedef QHash<int, int>   NodeToFiloMap;
typedef QHash<int, Filo>  FiloMap;


/* Tracks manages a set of Track.
 * A Track is a list of ids of items (filos or nodes) that correspond in time.
 * A Track has value -1 in the list if no id is known for a particular time point.
 */
class Tracks {
public:
    Tracks(const TimeMinMax& timeMinMax);

    int addTrack();
    void mergeTracks(const int trackId1, const int trackId2); // merges t2 into t1, deletes t2.
    void mergeTracks(const QSet<int>& trackIds);

    bool tracksCanBeMerged(const QSet<int>& trackIds) const;

    ItemList getMatchedItems(const int trackId) const;
    void addItemToTrack(const int trackId, const int time, const int item);

    bool exists(const int trackId) const;
    TimeMinMax getTimeMinMax() const;

    bool isSubtrackEnd(const int item) const;
    bool isSubtrackStart(const int item) const;

    ItemList getSubtrackEndItems(const int t) const;
    ItemList getSubtrackStartItems(const int t) const;

    ItemList getTrackIds() const;

    int getTrackOfItem(const int item) const;

private:
    TimeMinMax     mTimeMinMax;
    TracksMap      mTracksMap;
    ItemToTrackMap mItemToTrackMap;

    bool isSubtrackEnd(const ItemList& items, const int index) const;
    bool isSubtrackStart(const ItemList& items, const int index) const;

    int nextUnusedTrackId() const;
};


class TrackingModel {
public:
    TrackingModel(const TimeMinMax& timeMinMax);

    TimeMinMax getTimeMinMax() const;

    int addFilo(const int startingNode, const int time, const ItemList& nodes);

    /// Set nodes in \a nodes to matched. This implies:
    /// (1) The nodes are assigned to the same node track.
    /// (2) The filo's that they are on are assigned to the same filo track.
    /// (3) If all nodes are non-starting nodes, the starting nodes
    ///     of the filos are assigned to the same node track.
    /// Throws an exception on failure.
    /// The model remains unmodified when a failure is detected.
    void setMatchedNodes(const ItemList& nodes);

    int getFiloOfNode(const int n) const;
    int getBase(const int filoId) const;
    ItemList getBases(const int filoTrackId) const;

    int getTrackOfFilo(const int filoId) const;

    ItemList getNodeTrackIds() const;
    ItemList getMatchedNodes(const int trackId) const;

    ItemList getFilopodiaTrackIds() const;
    ItemList getMatchedFilopodia(const int trackId) const;

    ItemList getFilopodiaSubtrackEnds(const int time) const;
    ItemList getFilopodiaSubtrackStarts(const int time) const;

private:
    Tracks        mNodeTracks;
    Tracks        mFiloTracks;
    FiloMap       mFiloMap;
    NodeToFiloMap mNodeToFiloMap;
};

#endif // TRACKS_H
