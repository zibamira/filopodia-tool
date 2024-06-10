#ifndef SIMPLICIAL_MESH_2D_TEST_H
#define SIMPLICIAL_MESH_2D_TEST_H

#include <mclib/McVec3.h>

#include "SimplicialMesh.h"
#include "AugmentedContourTree.h"

#include <vector>

class SimplicialMesh2D_Test : public SimplicialMesh
{
public:
    SimplicialMesh2D_Test();
    ~SimplicialMesh2D_Test();
    /* Overloaded functions */
    void setThreshold(const double threshold);
    void getSortedListOfVertices(const bool            computeSplitTree,
                                 std::vector< mculong > & sortedListOfVertices);
    void getNeighborsOfNode(const bool            neighborsWithSmallerValue,
                            const mculong         nodeIdx,
                            std::vector< mculong > & neighbors,
                            std::vector< float >   & values) const;
    mclong getMeshVertexIdx(const mclong nodeIdx) const;
    mclong getNodeIdx(const mclong vertexIdx) const;
    void setNeighborhood(const int neighborhood);
    int getNeighborhood() const;

    /* Non-overloaded functions */
    const std::vector<float>   & getValues() const;
    const std::vector<McVec3f> & getCoords() const;

private:
    void test_neighbors();

private:
    bool                             m_useThreshold;
    double                           m_threshold;
    mculong                          m_numNodes;
    std::vector< float >                m_nodeValues;
    std::vector< McVec3f >              m_nodeCoords;
    std::vector< std::vector< mculong > >  m_nodeNeighbors;
};

#endif
