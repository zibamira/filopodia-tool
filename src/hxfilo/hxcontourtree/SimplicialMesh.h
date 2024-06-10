#ifndef SIMPLICIAL_MESH_H
#define SIMPLICIAL_MESH_H

#include <mclib/McHandable.h>
#include <mclib/McPrimType.h>

#include <vector>

class SimplicialMesh : public McHandable
{
public:
    ~SimplicialMesh() {};
    virtual void setThreshold(const double threshold) = 0;
    virtual void getSortedListOfVertices(const bool            computeSplitTree,
                                         std::vector< mculong > & sortedListOfVertices) = 0;
    virtual void getNeighborsOfNode(const bool            neighborsWithSmallerValue,
                                    const mculong         nodeIdx,
                                    std::vector< mculong > & neighbors,
                                    std::vector< float >   & values) const = 0;
    virtual mclong getMeshVertexIdx(const mclong nodeIdx) const = 0;
    virtual mclong getNodeIdx(const mclong vertexIdx) const = 0;
    virtual void setNeighborhood(const int neighborhood) = 0;
    virtual int getNeighborhood() const = 0;
};

#endif
