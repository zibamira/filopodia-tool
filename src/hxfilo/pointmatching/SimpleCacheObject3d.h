/////////////////////////////////////////////////////////////////
// 
// SimpleCacheObject3d.h
//
// Main Authors: Baum
// 
/////////////////////////////////////////////////////////////////

#ifndef SIMPLE_CACHE_OBJECT_3D_H
#define SIMPLE_CACHE_OBJECT_3D_H

#include <mclib/McVec2i.h>
#include <mclib/McVec3.h>
#include <mclib/McMat4.h>
#include <mclib/McBitfield.h>

#include "api.h"
#include "CacheObject.h"

#include <vector>

class POINTMATCHING_API SimpleCacheObject3d : public CacheObject 
{
public:
    SimpleCacheObject3d();
    ~SimpleCacheObject3d();

    void setCoords(const std::vector<McVec3f> & coords,
                   const McMat4f           & transform);

    const std::vector<McVec3f> & getCoords() const;

    void computeDistancesAndSort(const std::vector<McVec3f> & coords,
                                 const double              maxDistance);

    bool getNextMatchingPair(int    & set1PointIx,
                             int    & set2PointIx,
                             double & dist2);

protected:
    std::vector<McVec3f> coords;
    
private:
    std::vector<McVec2i> pointCorrespondence;
    std::vector<double>  pointDist2;
    std::vector<int>     sortedDist2List;
    McBitfield        pointSet1IsMatched;
    McBitfield        pointSet2IsMatched;
    int               currListIndex;
};
#endif
