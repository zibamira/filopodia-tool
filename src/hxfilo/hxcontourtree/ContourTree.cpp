#include <mclib/internal/McAssert.h>
#include <mclib/McBitfield.h>

#include "ContourTree.h"
#include "AugmentedContourTree.h"

ContourTreeNode::ContourTreeNode()
{
}

ContourTreeNode::ContourTreeNode(const mclong meshVertexId)
{
    m_meshVertexId = meshVertexId;
}

ContourTree::ContourTree(const AugmentedContourTree & augContourTree)
{
    std::vector< mclong > mapAugmentedTreeNodesToNodes(augContourTree.getNumNodes(), -1);

    std::vector< mclong >  meshVertexIds;
    std::vector< McSmallArray< mclong, 2 > > edges;
    augContourTree.getContourTree(meshVertexIds, edges);

    for ( size_t i=0; i<meshVertexIds.size(); ++i )
        m_nodes.push_back(ContourTreeNode(meshVertexIds[i]));

    for ( size_t i=0; i < edges.size(); ++i )
        addEdge(edges[i][0], edges[i][1]);
}

mculong
ContourTree::getNumNodes() const
{
    return m_nodes.size();
}

void
ContourTree::addEdge(
    const mclong node1Id,
    const mclong node2Id)
{   
    m_nodes[node1Id].m_parentNodes.push_back(node2Id);
    m_nodes[node2Id].m_childNodes.push_back(node1Id);
}

void
ContourTree::getNodesAndEdges(
    std::vector< mculong > & maxima,
    std::vector< mculong > & minima,
    std::vector< mculong > & saddles,
    std::vector< McVec2i > & edges) const
{
    maxima.clear();
    minima.clear();
    saddles.clear();
    edges.clear();

    for ( size_t i=0; i < m_nodes.size(); ++i )
    {
        if (    m_nodes[i].m_parentNodes.size() > 1
             || m_nodes[i].m_childNodes.size() > 1 )
            saddles.push_back(i);
        else if ( m_nodes[i].m_parentNodes.size() == 0 )
            maxima.push_back(i);
        else if ( m_nodes[i].m_childNodes.size()  == 0 )
            minima.push_back(i);

        for ( size_t j=0; j<m_nodes[i].m_parentNodes.size(); ++j )
        {
            const mclong parentNode = m_nodes[i].m_parentNodes[j];
            edges.push_back( McVec2i(i, parentNode) );
        }
    }
}

void
ContourTree::getMaximaAndSaddles(
    std::vector< mculong > & maxima,
    std::vector< mculong > & saddles) const
{
    maxima.clear();
    saddles.clear();

    for ( mclong i=0; i < static_cast<mclong>(m_nodes.size()); ++i )
    {
        if ( m_nodes[i].m_parentNodes.size() == 0 )
            maxima.push_back(i);

        if ( m_nodes[i].m_parentNodes.size() > 1 )
            saddles.push_back(i);
    }
}

void
ContourTree::getMeshVertexIds(
    std::vector< mclong > & meshVertexIds) const
{
    const mculong numNodes = m_nodes.size();
    meshVertexIds.resize(numNodes);
    
    for ( mculong i=0; i<numNodes; ++i )
        meshVertexIds[i] = m_nodes[i].m_meshVertexId;
}

mclong
ContourTree::getMeshVertexId(const mclong & nodeId) const
{
    return m_nodes[nodeId].m_meshVertexId;
}
