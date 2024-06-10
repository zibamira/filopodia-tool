/////////////////////////////////////////////////////////////////
// 
// ExactPointMatchingAlgorithm.h
//
// Main Authors: Baum
// 
/////////////////////////////////////////////////////////////////

#ifndef EXACT_POINT_MATCHING_ALGORITHM_H
#define EXACT_POINT_MATCHING_ALGORITHM_H

#include <mclib/McBitfield.h>

#include "hxgraphalgorithms/McDijkstraShortestPath.h"

#include "PointMatchingAlgorithm.h"
#include "PointMatchingScoringFunction.h"
#include "CacheObject.h"
#include "api.h"

class Transformation;
class PointMatchingDataStruct;
class PointRepresentation;

class POINTMATCHING_API ExactPointMatchingAlgorithm : public PointMatchingAlgorithm {
    
public:
    ExactPointMatchingAlgorithm();

    void setUserDefinedEdgeWeights(const std::vector< std::vector<double> > & edgeWeights);
    
    void computePointMatching(const PointRepresentation * pointRep1, 
                              const PointRepresentation * pointRep2,
                              const Transformation      * pointRep2Transform,
                              PointMatchingDataStruct   * pointMatching);

    void computeMaxCardinalityPointMatching(const PointRepresentation * pointRep1, 
                                            const PointRepresentation * pointRep2,
                                            const Transformation      * pointRep2Transform,
                                            PointMatchingDataStruct   * pointMatching);

private:
    std::vector<McDijkstraVertex>   m_vertices;
    std::vector<McDijkstraEdge>     m_edges;
    int                          m_startVertex;
    int                          m_terminalVertex;
                                 
    McHandle< CacheObject >      m_cacheObject;
    bool                         m_useUserDefinedEdgeWeights;
    std::vector< std::vector<double> > m_userDefinedEdgeWeights;

protected:
    void createBipartiteGraph(const PointRepresentation * pointRep1,
                              const PointRepresentation * pointRep2);

    double computeBestMatching(const PointRepresentation * pointRep1,
                               const PointRepresentation * pointRep2,
                               std::vector<int> & refPoints,
                               std::vector<int> & queryPoints);

    double computeMaxCardinalityMatching(const PointRepresentation * pointRep1,
                                         const PointRepresentation * pointRep2,
                                         std::vector<int> & refPoints,
                                         std::vector<int> & queryPoints);

    double getScoreOfMatching(const PointRepresentation * pointRep1,
                              const PointRepresentation * pointRep2,
                              std::vector<McDijkstraEdge *> matching);

    double getEdgeWeight(const PointRepresentation * pointRep1,
                         const int                   pointRep1PointIdx,
                         const PointRepresentation * pointRep2,
                         const int                   pointRep2PointIdx);

    void getRefAndQueryPointsFromMatching(const PointRepresentation * pointRep1,
					  std::vector<McDijkstraEdge *> matching,
					  std::vector<int> & refPoints,
                                          std::vector<int> & queryPoints);

    bool findMatching(std::vector<McDijkstraEdge *> & matching);
};

#endif
