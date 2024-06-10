/////////////////////////////////////////////////////////////////
// 
// AngularPointRepresentation3d.h
//
// Main Authors: Baum
// 
/////////////////////////////////////////////////////////////////

#ifndef ANGULAR_POINT_REPRESENTATION_3D_H
#define ANGULAR_POINT_REPRESENTATION_3D_H

#include <mclib/McVec3.h>
#include <mclib/McVec2i.h>
#include <mclib/McBox3f.h>
#include <mclib/McBitfield.h>

#include "PointRepresentation.h"
#include "api.h"

#include <vector>

class POINTMATCHING_API AngularPointRepresentation3d : public PointRepresentation {

public:
    AngularPointRepresentation3d(const double maxPointDistance,
                                 const double cliqueDistThreshold,
                                 const double maxAngleDistForInitMatching,
                                 const double minAngleForOptMatching,
                                 const double maxDistForAngle);

    void setCoords(const std::vector<McVec3f> & coords);
    void setOrigCoords(const std::vector<McVec3f> & origCoords);
    void setDirections(const std::vector<McVec3f> & directions);
    const std::vector<McVec3f> & getCoords() const;
    const std::vector<McVec3f> & getOrigCoords() const;
    const std::vector<McVec3f> & getDirections() const;
    const std::vector<std::vector<double> > & getDistMatrix() const;

    virtual int getNumPoints() const;

    virtual void computeCorrespondenceGraphVertices(
                const PointRepresentation * pointSet2, 
                std::vector<McVec2i>                 & corrGraph) const;

    virtual void computeCorrespondenceGraphEdges(
                const PointRepresentation * pointSet2, 
                const std::vector<McVec2i>           & corrGraph,
                std::vector<McBitfield>              & connected) const;

    virtual void computeTransformation(
                const PointRepresentation     * pointSet2,
                const PointMatchingDataStruct * pointMatching,
                Transformation                * pointSet2Transform) const;

    /* Virtual functions inherited from PointRepresentation. */

    virtual void setScoreDivisor(
                const PointRepresentation    * pointSet2, 
                PointMatchingScoringFunction * scoringFunction) const;

    virtual CacheObject * startGreedyPointMatching(
                const PointRepresentation * pointSet2,
                const Transformation      * pointSet2StartTransform) const;

    virtual CacheObject * startExactPointMatching(
                const PointRepresentation * pointSet2,
                const Transformation      * pointSet2StartTransform) const;

    virtual bool getNextMatchingPair(
                CacheObject * cacheObject,
                int         & refPointIx,
                int         & queryPointIx,
                double      & dist2) const;

    virtual void finishGreedyPointMatching() const;

    virtual void finishExactPointMatching() const;

    bool canPointsBeMatched(const int pointRep1PointIdx,
			    const PointRepresentation * pointRep2,
			    const int pointRep2PointIdx,
			    CacheObject * cacheObject,
			    double & squaredEdgeWeight) const;

    double getSquaredEdgeWeight(const int pointRep1PointIdx,
			       const PointRepresentation * pointRep2,
			       const int pointRep2PointIdx,
			       CacheObject * cacheObject) const;

    /// Type of transformation used to align points.
    /// 0=rigid, 1=rigid + iso-scale, 2=affine. 
    /// Default is rigid.
    void setTransformType(const int transType);

    bool connectByEdge( const int vertex1, 
                        const int vertex2, 
                        const PointRepresentation * otherPoints,
                        const std::vector<McVec2i>   & corrGraph) const;

protected:
    /// Compute bounding box of coordinates and distance matrix.
    void computeBBoxAndDistMatrix();

private:
    std::vector<McVec3f>           mCoords;
    std::vector<McVec3f>           mOrigCoords;
    std::vector<McVec3f>           mDirections;
    McBox3f                     bbox;
    double                      maximumDistance;
    std::vector<std::vector<double> > distMatrix;
    int                         transformType;
    double                      mMaxPointDistance;
    double                      mMaxPointDistance2;
    double                      mCliqueDistThreshold;
    double                      mMaxAngleDistForInitMatching;
    double                      mMinAngleForOptMatching;
    double                      mMaxDistForAngle;
};

#endif
