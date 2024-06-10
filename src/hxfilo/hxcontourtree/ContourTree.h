#ifndef CONTOUR_TREE_H
#define CONTOUR_TREE_H

#include <mclib/McVec2i.h>
#include <mclib/McHandable.h>

#include "api.h"

#include <vector>

class AugmentedContourTree;

class ContourTreeNode
{
public:
    friend class ContourTree;

    ContourTreeNode();
    ContourTreeNode(const mclong meshVertexId);

private:
    mclong                           m_meshVertexId;
    std::vector< mclong >               m_parentNodes;
    std::vector< mclong >               m_childNodes;
};

class HXCONTOURTREE_API ContourTree : public McHandable
{
public:
    ContourTree(const AugmentedContourTree & augContourTree);

    mculong getNumNodes() const;

    void getNodesAndEdges(std::vector< mculong > & maxima,
                          std::vector< mculong > & minima,
                          std::vector< mculong > & saddles,
                          std::vector< McVec2i > & edges) const;

    void getMaximaAndSaddles(std::vector< mculong > & maxima,
                             std::vector< mculong > & saddles) const;

    void getMeshVertexIds(std::vector< mclong > & meshVertexIds) const;
    mclong getMeshVertexId(const mclong & nodeId) const;

private:
    void   addEdge(const mclong node1Id, const mclong node2Id);

private:
    std::vector< ContourTreeNode >      m_nodes;
};

#endif
