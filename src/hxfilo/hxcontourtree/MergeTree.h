#ifndef MERGETREE_H
#define MERGETREE_H


#include <hxcontourtree/SetUnionDataStructure.h>
#include <hxcontourtree/Lattice3Mesh.h>
#include <hxcontourtree/CompareCheckFunctors.h>
#include <hxfield/HxUniformLabelField3.h>

#include <vector>
#include <algorithm>
#include <iostream>




enum class MergeMode
{
    JOIN_TREE,
    SPLIT_TREE,
    NUM_MERGEMODI
};

enum class SortNeighbours
{
    BY_VALUE,
    BY_VOTE,
    BY_EXTREMA,
    NUM_NEIGHBOURMODI
};

enum class PersistenceMode
{
    GLOBAL,
    ADAPTIVE,
    NUM_PERSISTANCEMODI
};

enum class SegmentationMode
{
    DISJOINT,
    NESTED,
    NESTED_CORES,
    NUM_SEGMENTATIONMODI
};


struct MergeTreeNode
{
    inline bool isUpperLeaf() const { return (m_parentNodes.size() == 0 && m_childNodes.size() == 1); }

    std::vector<size_t> m_parentNodes;
    std::vector<size_t> m_childNodes;

//    std::vector<size_t> m_parentSupernodes;
//    std::vector<size_t> m_childSupernodes;
};




class MergeTreeBase
{

public:
    MergeTreeBase();
    virtual ~MergeTreeBase();

    inline void setMergeMode(MergeMode mergeMode) { m_mergeMode = mergeMode; }
    inline void setSortNeighbours(SortNeighbours sortNeighbours) { m_sortNeighbours = sortNeighbours; }
    inline void setPersistenceMode(PersistenceMode persistenceMode) { m_persistenceMode = persistenceMode; }
    inline void setSegmentationMode(SegmentationMode segmentationMode) { m_segmentationMode = segmentationMode; }
    inline void setConsistentExtremaLabels(bool consistentExtremaLabels) { m_consistentExtremaLabels = consistentExtremaLabels; }
    inline void setNestedCoresWithSideBranches(bool nestedCoresWithSideBranches) { m_nestedCoresWithSideBranches = nestedCoresWithSideBranches; }
    inline size_t getNumLabels() { return m_nNodes; }
    inline const std::vector<size_t>& getLabelHistogram() { return m_labelHistogram; }
    inline std::vector<MergeTreeNode>& getTreeNodes() { return m_treeNodes; }

    virtual void computeMergeTree(float rangeMin, float rangeMax) = 0;
    virtual void computeFastMergeSegmentation(float rangeMin, float rangeMax, float persistenceValue, HxUniformLabelField3* res) = 0;

//    virtual void computeJoinTree(float rangeMin, float rangeMax) = 0;
//    virtual void computeSplitTree(float rangeMin, float rangeMax) = 0;

    void contractIncidentEdges(const size_t nodeIdx);

protected:
    void addEdge(const size_t lowerNodeIdx, const size_t upperNodeIdx);
//    void reduceIncidentEdges(const size_t nodeIdx);

    MergeMode m_mergeMode;
    SortNeighbours m_sortNeighbours;
    PersistenceMode m_persistenceMode;
    SegmentationMode m_segmentationMode;
    bool m_consistentExtremaLabels;
    bool m_nestedCoresWithSideBranches;

    size_t m_nNodes;
    std::vector<size_t> m_labelHistogram;
    std::vector<long long> m_nestedLabel2extremaIdx;

    std::vector<MergeTreeNode> m_treeNodes;
};




template <typename T>
class MergeTree : public MergeTreeBase
{

public:
    MergeTree(Lattice3Mesh<T>* const mesh);
    virtual ~MergeTree();

    virtual void computeMergeTree(float globalMin, float globalMax);
    virtual void computeFastMergeSegmentation(float rangeMin, float rangeMax, float persistenceValue, HxUniformLabelField3* res);

//    virtual void computeJoinTree(float rangeMin, float rangeMax);
//    virtual void computeSplitTree(float rangeMin, float rangeMax);

private:
    void sortNeighboursAccordingToMajorityVote(std::vector<std::pair<size_t, T> >& meshVertexNeighbours, SetUnionDataStructure& setUnion);
    void sortNeighboursAccordingToExtrema(std::vector<std::pair<size_t, T> >& meshVertexNeighbours, SetUnionDataStructure& setUnion, IndexValueAbstractBaseComparator<T>* comparator);
    void computeSortedMaximaAndSaddles();


    template <typename U>
    U abs(U a, U b) { return std::max(a, b) - std::min(a, b); }


    template <typename IT>
    void computeMergeTree(IT begin, IT end, IndexValueAbstractBaseComparator<T>* comparator, float rangeMin, float rangeMax)
    {
        // for storing merge tree
        SetUnionDataStructure setUnion;
        setUnion.setNumElements(m_nNodes);
        std::vector<size_t> lowestNodeIdxOfComponent(m_nNodes);
        m_treeNodes.assign(m_nNodes, MergeTreeNode());

        // for storing finest disjoint segmentation
        m_disjointSetUnion.setNumElements(m_nNodes);
        m_extremeValueOfDisjointComponent.assign(m_nNodes, T{});

        // for storing finest nested segmentation
        m_nestedSetUnion.setNumElements(m_nNodes + 1);          // +1 for background setId
        m_nestedSetUnion.setSetIdOfElement(m_nNodes, m_nNodes); // set background setId
        m_extremeValueOfNestedComponent.assign(m_nNodes, T{});


        size_t nodeIdx;
        T nodeValue;
        size_t nodeComponent;

        size_t meshVertexIdx;
        std::vector<std::pair<size_t, T> > meshVertexNeighbours;

        size_t nodeNeighbourIdx;
        size_t nodeNeighbourComponent;


        for(IT it = begin; it != end; ++it)
        {
            nodeIdx = it->first;
            nodeValue = it->second;


            // for storing merge tree
            setUnion.setSetIdOfElement(nodeIdx, nodeIdx);
            lowestNodeIdxOfComponent[nodeIdx] = nodeIdx;

            // for storing finest disjoint segmentation
            m_disjointSetUnion.setSetIdOfElement(nodeIdx, nodeIdx);
            m_extremeValueOfDisjointComponent[nodeIdx] = nodeValue;

            // for storing finest nested segmentation
            m_nestedSetUnion.setSetIdOfElement(nodeIdx, nodeIdx);
            m_extremeValueOfNestedComponent[nodeIdx] = nodeValue;


            meshVertexIdx = m_mesh->getMeshVertexIdx(nodeIdx);
            meshVertexNeighbours.clear();
            m_mesh->getNeighboursOfMeshVertex(meshVertexIdx, meshVertexNeighbours, rangeMin, rangeMax, comparator);

            if(meshVertexNeighbours.empty())
                continue;


            if(m_sortNeighbours == SortNeighbours::BY_VALUE)
                std::sort(meshVertexNeighbours.begin(), meshVertexNeighbours.end(), IndexValueInvertedComparator<T>(comparator));
            else if(m_sortNeighbours == SortNeighbours::BY_VOTE)
                sortNeighboursAccordingToMajorityVote(meshVertexNeighbours, m_disjointSetUnion);
            else if(m_sortNeighbours == SortNeighbours::BY_EXTREMA)
                sortNeighboursAccordingToExtrema(meshVertexNeighbours, m_disjointSetUnion, comparator);

            // for storing merge tree
            for(const std::pair<size_t, T>& meshVertexNeighbour : meshVertexNeighbours)
            {
                nodeNeighbourIdx = static_cast<size_t>(m_mesh->getNodeIdx(meshVertexNeighbour.first));

                nodeComponent = setUnion.findSetId(nodeIdx);
                nodeNeighbourComponent = setUnion.findSetId(nodeNeighbourIdx);

                if(nodeComponent == nodeNeighbourComponent)
                    continue;

                addEdge(nodeIdx, static_cast<size_t>(lowestNodeIdxOfComponent[nodeNeighbourComponent]));

                setUnion.mergeSetsOfElements(nodeIdx, nodeNeighbourIdx);
                lowestNodeIdxOfComponent[nodeNeighbourComponent] = nodeIdx;
            }


            // for storing finest disjoint segmentation
            assert(!meshVertexNeighbours.empty());
            nodeNeighbourIdx = static_cast<size_t>(m_mesh->getNodeIdx(meshVertexNeighbours.back().first));

            nodeComponent = m_disjointSetUnion.findSetId(nodeIdx);
            assert(nodeComponent == nodeIdx);

            nodeNeighbourComponent = m_disjointSetUnion.findSetId(nodeNeighbourIdx);
            assert(nodeComponent != nodeNeighbourComponent);

            assert(!(comparator->operator ()(m_extremeValueOfDisjointComponent[nodeComponent], m_extremeValueOfDisjointComponent[nodeNeighbourComponent])));
            m_disjointSetUnion.mergeSetsOfElements(nodeIdx, nodeNeighbourIdx);


            // for storing finest nested segmentation
            const MergeTreeNode& treeNode = m_treeNodes[nodeIdx];
            assert(!treeNode.m_parentNodes.empty());

            nodeComponent = m_nestedSetUnion.findSetId(nodeIdx);
            assert(nodeComponent == nodeIdx);

            if(treeNode.m_parentNodes.size() == 1)  // regular node
            {
                size_t nodeParentIdx = treeNode.m_parentNodes.front();

//                size_t nodeParentComponent = m_nestedSetUnion.findSetId(nodeParentIdx);
//                assert(nodeComponent != nodeParentComponent);
                assert(nodeComponent != m_nestedSetUnion.findSetId(nodeParentIdx));

//                assert(!(comparator->operator ()(m_extremeValueOfNestedComponent[nodeComponent], m_extremeValueOfNestedComponent[nodeParentComponent])));
                assert(!(comparator->operator ()(m_extremeValueOfNestedComponent[nodeComponent], m_extremeValueOfNestedComponent[m_nestedSetUnion.findSetId(nodeParentIdx)])));
                m_nestedSetUnion.mergeSetsOfElements(nodeIdx, nodeParentIdx);
            }
            else    // saddle node
            {
                for(size_t nodeParentIdx : treeNode.m_parentNodes)
                {
                    size_t nodeParentComponent = m_nestedSetUnion.findSetId(nodeParentIdx);
                    assert(nodeComponent != nodeParentComponent);

                    if(comparator->operator ()(m_extremeValueOfNestedComponent[nodeParentComponent], m_extremeValueOfNestedComponent[nodeComponent]))
                        m_extremeValueOfNestedComponent[nodeComponent] = m_extremeValueOfNestedComponent[nodeParentComponent];
                }
            }
        }
    }


    template <typename IT>
    void computeFastDisjointMergeSegmentation(IT begin, IT end, IndexValueAbstractBaseComparator<T>* comparator, float dataOpposite, float rangeMin, float rangeMax, float persistenceValue, HxUniformLabelField3* res)
    {
        SetUnionDataStructure setUnion = m_disjointSetUnion;

        size_t saddleIdx;
        T saddleValue;
        size_t saddleComponent;
        T saddleComponentLength;
        float saddleDataHeight;

//        size_t saddleParentComponent;
//        T saddleParentComponentLength;
//        float saddleParentDataHeight;

        size_t meshVertexIdx;
        std::vector<std::pair<size_t, T> > meshVertexNeighbours;

        size_t saddleNeighbourIdx;
        size_t saddleNeighbourComponent;
        T saddleNeighbourComponentLength;
        float saddleNeighbourDataHeight;

        T minComponentLength;
        float minDataHeight;


        for(IT it = begin; it != end; ++it)
        {
            saddleIdx = it->first;
            saddleValue = it->second;


//            for(size_t saddleParentIdx : m_treeNodes[saddleIdx].m_parentNodes)
//            {
//                saddleComponent = setUnion.findSetId(saddleIdx);
//                saddleParentComponent = setUnion.findSetId(saddleParentIdx);

//                if(saddleComponent == saddleParentComponent)
//                    continue;


//                saddleComponentLength = abs(m_extremeValueOfDisjointComponent[saddleComponent], saddleValue);
//                saddleDataHeight = abs(static_cast<float>(m_extremeValueOfDisjointComponent[saddleComponent]), dataOpposite);

//                saddleParentComponentLength = abs(m_extremeValueOfDisjointComponent[saddleParentComponent], saddleValue);
//                saddleParentDataHeight = abs(static_cast<float>(m_extremeValueOfDisjointComponent[saddleParentComponent]), dataOpposite);

//                if(saddleComponentLength < saddleParentComponentLength)
//                {
//                    minComponentLength = saddleComponentLength;
//                    assert(saddleDataHeight < saddleParentDataHeight);
//                    minDataHeight = saddleDataHeight;
//                }
//                else
//                {
//                    minComponentLength = saddleParentComponentLength;
//                    assert(saddleParentDataHeight < saddleDataHeight);
//                    minDataHeight = saddleParentDataHeight;
//                }


//                if( (m_persistenceMode == PersistenceMode::GLOBAL && static_cast<float>(minComponentLength) < persistenceValue) ||
//                    (m_persistenceMode == PersistenceMode::ADAPTIVE && (static_cast<float>(minComponentLength) / minDataHeight) < persistenceValue) )
//                {
//                    if(comparator->operator ()(m_extremeValueOfDisjointComponent[saddleParentComponent], m_extremeValueOfDisjointComponent[saddleComponent]))
//                        setUnion.mergeSetsOfElements(saddleIdx, saddleParentIdx);
//                    else
//                        setUnion.mergeSetsOfElements(saddleParentIdx, saddleIdx);
//                }
//            }


            meshVertexIdx = m_mesh->getMeshVertexIdx(saddleIdx);
            meshVertexNeighbours.clear();
            m_mesh->getNeighboursOfMeshVertex(meshVertexIdx, meshVertexNeighbours, rangeMin, rangeMax, comparator);

            for(const std::pair<size_t, T>& meshVertexNeighbour : meshVertexNeighbours)
            {
                saddleNeighbourIdx = static_cast<size_t>(m_mesh->getNodeIdx(meshVertexNeighbour.first));

                saddleComponent = setUnion.findSetId(saddleIdx);
                saddleNeighbourComponent = setUnion.findSetId(saddleNeighbourIdx);

                if(saddleComponent == saddleNeighbourComponent)
                    continue;


//                if(m_extremeValueOfDisjointComponent[saddleComponent] != m_extremeValueOfNestedComponent[saddleComponent])
//                {
//                    std::cout << "saddle disjoint extrema: " << m_extremeValueOfDisjointComponent[saddleComponent] << std::endl;
//                    std::cout << "saddle nested extrema: " << m_extremeValueOfNestedComponent[saddleComponent] << std::endl;
//                }

//                if(m_extremeValueOfDisjointComponent[saddleNeighbourComponent] != m_extremeValueOfNestedComponent[saddleNeighbourComponent])
//                {
//                    std::cout << "neighbour disjoint extrema: " << m_extremeValueOfDisjointComponent[saddleNeighbourComponent] << std::endl;
//                    std::cout << "neighbour nested extrema: " << m_extremeValueOfNestedComponent[saddleNeighbourComponent] << std::endl;
//                }

//                assert(!(m_extremeValueOfDisjointComponent[saddleComponent] != m_extremeValueOfNestedComponent[saddleComponent]));
//                assert(!(m_extremeValueOfDisjointComponent[saddleNeighbourComponent] != m_extremeValueOfNestedComponent[saddleNeighbourComponent]));



                saddleComponentLength = abs(m_extremeValueOfDisjointComponent[saddleComponent], saddleValue);
                saddleDataHeight = abs(static_cast<float>(m_extremeValueOfDisjointComponent[saddleComponent]), dataOpposite);

                saddleNeighbourComponentLength = abs(m_extremeValueOfDisjointComponent[saddleNeighbourComponent], saddleValue);
                saddleNeighbourDataHeight = abs(static_cast<float>(m_extremeValueOfDisjointComponent[saddleNeighbourComponent]), dataOpposite);

                if(saddleComponentLength < saddleNeighbourComponentLength)
                {
                    minComponentLength = saddleComponentLength;
                    assert(saddleDataHeight < saddleNeighbourDataHeight);
                    minDataHeight = saddleDataHeight;
                }
                else
                {
                    minComponentLength = saddleNeighbourComponentLength;
                    assert(!(saddleDataHeight < saddleNeighbourDataHeight));
                    minDataHeight = saddleNeighbourDataHeight;
                }


                if( (m_persistenceMode == PersistenceMode::GLOBAL && static_cast<float>(minComponentLength) < persistenceValue) ||
                    (m_persistenceMode == PersistenceMode::ADAPTIVE && (static_cast<float>(minComponentLength) / minDataHeight) < persistenceValue ) )
                {
                    if(comparator->operator ()(m_extremeValueOfDisjointComponent[saddleNeighbourComponent], m_extremeValueOfDisjointComponent[saddleComponent]))
                        setUnion.mergeSetsOfElements(saddleIdx, saddleNeighbourIdx);
                    else
                        setUnion.mergeSetsOfElements(saddleNeighbourIdx, saddleIdx);
                }
            }
        }




        const std::vector<std::pair<size_t, T> >& sortedNodeIndices = m_mesh->getSortedNodeIndices();

        typename std::vector<std::pair<size_t, T> >::const_iterator lowerBoundIt = std::lower_bound(sortedNodeIndices.cbegin(), sortedNodeIndices.cend(), std::pair<size_t, float>(0, rangeMin),
                                                                                                    [](const std::pair<size_t, T>& lhs, const std::pair<size_t, float>& rhs) -> bool { return static_cast<float>(lhs.second) < rhs.second; });
        typename std::vector<std::pair<size_t, T> >::const_iterator upperBoundIt = std::upper_bound(lowerBoundIt, sortedNodeIndices.cend(), std::pair<size_t, float>(0, rangeMax),
                                                                                                    [](const std::pair<size_t, float>& lhs, const std::pair<size_t, T>& rhs) -> bool { return lhs.second < static_cast<float>(rhs.second); });

        assert(res);
        memset(res->lattice().dataPtr(), 0, res->lattice().getDims().nbVoxel() * res->lattice().primType().size());
        int* resDataPtr{static_cast<int*>(res->lattice().dataPtr())};

        m_labelHistogram.assign(m_nNodes, 0);

        size_t nodeIdx;
        size_t label;

        for(typename std::vector<std::pair<size_t, T> >::const_iterator it = lowerBoundIt; it != upperBoundIt; ++it)
        {
            nodeIdx = it->first;
            label = setUnion.findSetId(nodeIdx);

            ++m_labelHistogram[label];
            resDataPtr[m_mesh->getMeshVertexIdx(nodeIdx)] = label;
        }
    }


    template <typename IT>
    void computeFastNestedMergeSegmentation(IT begin, IT end, IndexValueAbstractBaseComparator<T>* comparator, float dataOpposite, float rangeMin, float rangeMax, float persistenceValue, HxUniformLabelField3* res)
    {
        SetUnionDataStructure setUnion = m_nestedSetUnion;

        size_t saddleIdx;
        T saddleValue;
//        size_t saddleComponent;

        size_t saddleParentComponent;
        T saddleParentComponentLength;
        float saddleParentDataHeight;

        bool saddleBecomesBackground;
        std::vector<size_t> unmergedParents;


        for(IT it = begin; it != end; ++it)
        {
            saddleIdx = it->first;
            saddleValue = it->second;

            saddleBecomesBackground = false;

            if(m_segmentationMode == SegmentationMode::NESTED_CORES)
            {
                for(size_t saddleParentIdx : m_treeNodes[saddleIdx].m_parentNodes)
                {
//                    saddleComponent = setUnion.findSetId(saddleIdx);
                    saddleParentComponent = setUnion.findSetId(saddleParentIdx);
//                    assert(saddleComponent != saddleParentComponent);
                    assert(setUnion.findSetId(saddleIdx) != saddleParentComponent);

                    if(saddleParentComponent == m_nNodes)
                    {
                        saddleBecomesBackground = true;
                        break;
                    }
                }
            }

            if(m_segmentationMode == SegmentationMode::NESTED_CORES && m_nestedCoresWithSideBranches && saddleBecomesBackground)
            {
                setUnion.mergeSetsOfElements(saddleIdx, m_nNodes);
                continue;
            }


            unmergedParents.clear();
            unmergedParents.reserve(m_treeNodes[saddleIdx].m_parentNodes.size());

            for(size_t saddleParentIdx : m_treeNodes[saddleIdx].m_parentNodes)
            {
//                saddleComponent = setUnion.findSetId(saddleIdx);
                saddleParentComponent = setUnion.findSetId(saddleParentIdx);
//                assert(saddleComponent != saddleParentComponent);
                assert(setUnion.findSetId(saddleIdx) != saddleParentComponent);

                if(saddleParentComponent == m_nNodes)
                    continue;

                saddleParentComponentLength = abs(m_extremeValueOfNestedComponent[saddleParentComponent], saddleValue);
                saddleParentDataHeight = abs(static_cast<float>(m_extremeValueOfNestedComponent[saddleParentComponent]), dataOpposite);

                if( (m_persistenceMode == PersistenceMode::GLOBAL && static_cast<float>(saddleParentComponentLength) < persistenceValue) ||
                    (m_persistenceMode == PersistenceMode::ADAPTIVE && (static_cast<float>(saddleParentComponentLength) / saddleParentDataHeight) < persistenceValue) )
                {
//                    assert(!(comparator->operator ()(m_extremeValueOfNestedComponent[saddleParentComponent], m_extremeValueOfNestedComponent[saddleComponent])));
                    assert(!(comparator->operator ()(m_extremeValueOfNestedComponent[saddleParentComponent], m_extremeValueOfNestedComponent[setUnion.findSetId(saddleIdx)])));
                    setUnion.mergeSetsOfElements(saddleParentIdx, saddleIdx);
                }
                else if(!saddleBecomesBackground)
                    unmergedParents.emplace_back(saddleParentIdx);    // only then it makes sense to merged after this loop
            }

            if(saddleBecomesBackground)
            {
                assert(unmergedParents.empty());
                setUnion.mergeSetsOfElements(saddleIdx, m_nNodes);
                continue;
            }

            if(unmergedParents.empty())
                continue;

            if(unmergedParents.size() == 1)
            {
//                saddleComponent = setUnion.findSetId(saddleIdx);
                saddleParentComponent = setUnion.findSetId(unmergedParents.front());

//                assert(!(comparator->operator ()(m_extremeValueOfNestedComponent[saddleParentComponent], m_extremeValueOfNestedComponent[saddleComponent])));
                assert(!(comparator->operator ()(m_extremeValueOfNestedComponent[saddleParentComponent], m_extremeValueOfNestedComponent[setUnion.findSetId(saddleIdx)])));
                setUnion.mergeSetsOfElements(unmergedParents.front(), saddleIdx);
            }
            else if(m_segmentationMode == SegmentationMode::NESTED_CORES)
                setUnion.mergeSetsOfElements(saddleIdx, m_nNodes);
        }




        if(m_consistentExtremaLabels)
        {
            m_nestedLabel2extremaIdx.assign(m_nNodes, -1);

            typename std::vector<std::pair<size_t, T> >::const_iterator lowerBoundExtremaIt = std::lower_bound(m_extrema.cbegin(), m_extrema.cend(), std::pair<size_t, float>(0, rangeMin),
                                                                                                               [](const std::pair<size_t, T>& lhs, const std::pair<size_t, float>& rhs) -> bool { return static_cast<float>(lhs.second) < rhs.second; });
            typename std::vector<std::pair<size_t, T> >::const_iterator upperBoundExtremaIt = std::upper_bound(lowerBoundExtremaIt, m_extrema.cend(), std::pair<size_t, float>(0, rangeMax),
                                                                                                               [](const std::pair<size_t, float>& lhs, const std::pair<size_t, T>& rhs) -> bool { return lhs.second < static_cast<float>(rhs.second); });
            typename std::vector<std::pair<size_t, T> >::const_reverse_iterator revUpperBoundExtremaIt = std::reverse_iterator< typename std::vector<std::pair<size_t, T> >::const_iterator >(upperBoundExtremaIt);
            typename std::vector<std::pair<size_t, T> >::const_reverse_iterator revLowerBoundExtremaIt = std::reverse_iterator< typename std::vector<std::pair<size_t, T> >::const_iterator >(lowerBoundExtremaIt);

            if(m_mergeMode == MergeMode::JOIN_TREE)
                computeNestedLabel2ExtremaIdxMap(revUpperBoundExtremaIt, revLowerBoundExtremaIt, setUnion);
            else // m_mergeMode == SPLIT_TREE
                computeNestedLabel2ExtremaIdxMap(lowerBoundExtremaIt, upperBoundExtremaIt, setUnion);
        }





        const std::vector<std::pair<size_t, T> >& sortedNodeIndices = m_mesh->getSortedNodeIndices();

        typename std::vector<std::pair<size_t, T> >::const_iterator lowerBoundIt = std::lower_bound(sortedNodeIndices.cbegin(), sortedNodeIndices.cend(), std::pair<size_t, float>(0, rangeMin),
                                                                                                    [](const std::pair<size_t, T>& lhs, const std::pair<size_t, float>& rhs) -> bool { return static_cast<float>(lhs.second) < rhs.second; });
        typename std::vector<std::pair<size_t, T> >::const_iterator upperBoundIt = std::upper_bound(lowerBoundIt, sortedNodeIndices.cend(), std::pair<size_t, float>(0, rangeMax),
                                                                                                    [](const std::pair<size_t, float>& lhs, const std::pair<size_t, T>& rhs) -> bool { return lhs.second < static_cast<float>(rhs.second); });

        assert(res);
        memset(res->lattice().dataPtr(), 0, res->lattice().getDims().nbVoxel() * res->lattice().primType().size());
        int* resDataPtr{static_cast<int*>(res->lattice().dataPtr())};

        m_labelHistogram.assign(m_nNodes, 0);

        size_t nodeIdx;
        size_t label;
        long long extremaIdx;

        for(typename std::vector<std::pair<size_t, T> >::const_iterator it = lowerBoundIt; it != upperBoundIt; ++it)
        {
            nodeIdx = it->first;
            label = setUnion.findSetId(nodeIdx);

            if(label == m_nNodes)
                continue;


            if(m_consistentExtremaLabels)
            {
                extremaIdx = m_nestedLabel2extremaIdx[label];
                if(extremaIdx > -1)
                    label = m_disjointSetUnion.findSetId(extremaIdx);
            }


            ++m_labelHistogram[label];
            resDataPtr[m_mesh->getMeshVertexIdx(nodeIdx)] = label;
        }
    }


    template <typename IT>
    void computeNestedLabel2ExtremaIdxMap(IT begin, IT end, SetUnionDataStructure& setUnion)
    {
        size_t extremaIdx;
        size_t extremaComponent;

        for(IT it = begin; it != end; ++it)
        {
            extremaIdx = it->first;
            extremaComponent = setUnion.findSetId(extremaIdx);

            if(extremaComponent == m_nNodes)
                continue;

            if(m_nestedLabel2extremaIdx[extremaComponent] > -1)
                continue;

            m_nestedLabel2extremaIdx[extremaComponent] = extremaIdx;
        }
    }




    Lattice3Mesh<T>* const m_mesh;

    SetUnionDataStructure m_disjointSetUnion;
    std::vector<T> m_extremeValueOfDisjointComponent;
    size_t m_nFinestDisjointLabels;

    SetUnionDataStructure m_nestedSetUnion;
    std::vector<T> m_extremeValueOfNestedComponent;
    size_t m_nFinestNestedLabels;

    std::vector<std::pair<size_t, T> > m_extrema;
    std::vector<std::pair<size_t, T> > m_saddles;
};


#endif // MERGETREE_H
