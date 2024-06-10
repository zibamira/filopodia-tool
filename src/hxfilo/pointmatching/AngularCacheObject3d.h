/////////////////////////////////////////////////////////////////
// 
// AngularCacheObject3d.h
//
// Main Authors: Baum
// 
/////////////////////////////////////////////////////////////////

#ifndef ANGULAR_CACHE_OBJECT_3D_H
#define ANGULAR_CACHE_OBJECT_3D_H

#include <mclib/McVec2i.h>
#include <mclib/McVec3.h>
#include <mclib/McMat4.h>
#include <mclib/McBitfield.h>

#include "api.h"
#include "CacheObject.h"

#include <vector>

class POINTMATCHING_API AngularCacheObject3d : public CacheObject 
{
public:
    AngularCacheObject3d();
    ~AngularCacheObject3d();

    void setCoordsAndDirections(
            const std::vector<McVec3f> & coordinates,
            const std::vector<McVec3f> & origCoordinates,
            const std::vector<McVec3f> & directions,
            const McMat4f           & transform);

    const std::vector<McVec3f> & getCoords() const;

    const std::vector<McVec3f> & getDirections() const;

    void computeDistancesAndSort(const std::vector<McVec3f> & coords,
                                 const std::vector<McVec3f> & origCoords,
                                 const std::vector<McVec3f> & directions1,
                                 const double              maxDistance,
                                 const double              minAngleForOptMatching);

    bool getNextMatchingPair(int    & set1PointIx,
                             int    & set2PointIx,
                             double & dist2);

protected:
    std::vector<McVec3f> mCoords;
    std::vector<McVec3f> mOrigCoords;
    std::vector<McVec3f> mDirections;
    
    double getDistance(const McVec3f coord1, const McVec3f coord2, const McVec3f origCoord1, const McVec3f origCoord2, const McVec3f dir1, const McVec3f dir2, double maxDistance);
  
private:
    std::vector<McVec2i> pointCorrespondence;
    std::vector<double>  pointDist2;
    std::vector<int>     sortedDist2List;
    McBitfield        pointSet1IsMatched;
    McBitfield        pointSet2IsMatched;
    int               currListIndex;
};
#endif
