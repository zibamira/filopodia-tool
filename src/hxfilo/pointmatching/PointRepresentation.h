/////////////////////////////////////////////////////////////////
// 
// PointRepresentation.h
//
// Main Authors: Baum
// 
/////////////////////////////////////////////////////////////////

#ifndef POINT_REPRESENTATION_H
#define POINT_REPRESENTATION_H

#include <mclib/McHandable.h>
#include <mclib/McVec3.h>
#include <mclib/McVec2i.h>
#include <mclib/McBitfield.h>

#include "api.h"

#include <vector>

class PointRepresentation;
class PointMatchingDataStruct;
class PointMatchingScoringFunction;
class Transformation;
class CacheObject;

class POINTMATCHING_API PointRepresentation : public McHandable {

public:
    virtual int getNumPoints() const = 0;

    virtual void computeTransformation(
                const PointRepresentation     * pointRep2,
                const PointMatchingDataStruct * pointMatching,
                Transformation                * pointRep2Transform) const = 0;

    virtual void setScoreDivisor(
                const PointRepresentation    * pointRep2, 
                PointMatchingScoringFunction * scoringFunction) const = 0;

    virtual CacheObject * startGreedyPointMatching(
                const PointRepresentation * pointRep2, 
                const Transformation      * pointRep2StartTransform) const = 0;

    virtual CacheObject * startExactPointMatching(
                const PointRepresentation * pointRep2, 
                const Transformation      * pointRep2StartTransform) const = 0;

    virtual bool getNextMatchingPair(
                CacheObject * cacheObject, 
                int         & refPointIx, 
                int         & queryPointIx, 
                double      & dist2) const = 0;

    virtual const std::vector<McVec3f> & getCoords() const = 0;

    virtual void computeCorrespondenceGraphVertices(
                const PointRepresentation * pointSet2, 
                std::vector<McVec2i>         & corrGraph) const = 0;

    virtual void computeCorrespondenceGraphEdges(
                const PointRepresentation * pointSet2, 
                const std::vector<McVec2i>   & corrGraph,
                std::vector<McBitfield>      & connected) const = 0;

    virtual void finishGreedyPointMatching() const = 0;

    virtual void finishExactPointMatching() const = 0;

    virtual const std::vector<std::vector<double> > & getDistMatrix() const = 0;

    virtual bool canPointsBeMatched(
	       const int pointRep1PointIdx,
	       const PointRepresentation * pointRep2,
	       const int pointRep2PointIdx,
	       CacheObject * cacheObject,
	       double & squaredEdgeWeight) const = 0;

    virtual double getSquaredEdgeWeight(
               const int pointRep1PointIdx,
	       const PointRepresentation * pointRep2,
	       const int pointRep2PointIdx,
	       CacheObject * cacheObject) const = 0;
};

#endif
