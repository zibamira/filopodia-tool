#ifndef AUGMENTED_CONTOUR_TREE_H
#define AUGMENTED_CONTOUR_TREE_H

#include <mclib/internal/McSmallArray.h>
#include <mclib/McVec3.h>

#include "SimplicialMesh.h"
#include "SetUnionDataStructure.h"

#include "api.h"

#include <vector>

class McBitfield;

class AugmentedContourTreeNode
{
public:
    friend class AugmentedContourTree;

    AugmentedContourTreeNode();

private:
    bool isUpperLeaf() const;

private:
    std::vector< mclong >               m_parentNodes;
    std::vector< mclong >               m_childNodes;
};

class HXCONTOURTREE_API AugmentedContourTree
{
public:
    AugmentedContourTree(SimplicialMesh * mesh);

    void computeJoinTree();
    void computeSplitTree();
    void computeContourTree();

    mclong getNumNodes() const;
    void getEdges(std::vector< McSmallArray< mclong, 2 > > & edges) const;
    void getComponents(std::vector< mclong > & components) const;
    void getCriticalPointNodesAndEdges(std::vector< mculong > & maxima,
                                       std::vector< mculong > & minima,
                                       std::vector< mculong > & saddles,
                                       std::vector< McSmallArray< mclong, 2 > > & edges) const;
    void getEdgesBetweenCriticalPoints(std::vector< McSmallArray< mclong, 2 > > & edges) const;
    void getContourTree(std::vector< mclong >  & vertexIds,
                        std::vector< McSmallArray< mclong, 2 > > & edges) const;
    void getSortedListOfMeshVertices(std::vector< mculong > & vertexIds);

private:
    std::vector< AugmentedContourTreeNode > & getNodes();
    void computeJoinSplitTree(const bool computeSplitTree);
    void mergeJoinAndSplitTree(AugmentedContourTree & joinTree,
                               AugmentedContourTree & splitTree);
    void unionFindMerge(const mculong nodeIdx, const mculong newComponent);
    mculong findNodeComponent(const mculong nodeIdx);
    void addEdgeToTree(const mculong nodeIdx1, const mculong nodeIdx2);
    void deleteNode(const mclong nodeId);
    void findComponent(const mclong         startNode,
                       McBitfield         & hasBeenProcessed,
                       std::vector< mclong > & components) const;
    void findComponentElements(mclong             & node,
                               McBitfield         & hasBeenProcessed,
                               std::vector< mclong > & components,
                               std::vector< mclong > & elements) const;
    void getCriticalPointNodesAndEdges(mculong               nodeIdx,
                                       mculong               nodeIdx2,
                                       McBitfield          & hasBeenProcessed,
                                       std::vector< mculong > & maxima,
                                       std::vector< mculong > & saddles,
                                       std::vector< McSmallArray< mclong, 2 > > & edges) const;
    mclong handleLeafNode(const AugmentedContourTreeNode & joinTreeNode,
                          const AugmentedContourTreeNode & splitTreeNode,
                          const mclong                     nodeId);

private:
    SimplicialMesh                          * m_mesh;
    SetUnionDataStructure                     m_setUnion;
    std::vector< AugmentedContourTreeNode >   m_nodes;
    std::vector< mculong >                       m_lowestNodeOfComponent;
    std::vector< mculong >                       m_sortedNodeIdx;
};

#endif
