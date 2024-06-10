#include <hxcontourtree/MergeTree.h>

#include <map>
#include <functional>




MergeTreeBase::MergeTreeBase()
    : m_mergeMode{MergeMode::JOIN_TREE}, m_sortNeighbours{SortNeighbours::BY_VALUE}, m_segmentationMode{SegmentationMode::DISJOINT}, m_consistentExtremaLabels{false}, m_nestedCoresWithSideBranches{false}, m_nNodes{0}
{

}


MergeTreeBase::~MergeTreeBase()
{

}


void MergeTreeBase::contractIncidentEdges(const size_t nodeIdx)
{
    // This PUBLIC method is used by the contour tree class when join and split trees are merged.
    // The contour tree class is not pushed to zib-amira yet!

    MergeTreeNode& node = m_treeNodes[nodeIdx];

    assert(node.m_parentNodes.size() < 2);
    assert(node.m_childNodes.size() < 2);

    long long parentNodeIdx{-1};
    long long childNodeIdx{-1};

    if(node.m_parentNodes.size() > 0)
    {
        parentNodeIdx = node.m_parentNodes.back();
        node.m_parentNodes.pop_back();
        assert(node.m_parentNodes.empty());
    }
    if(node.m_childNodes.size() > 0)
    {
        childNodeIdx = node.m_childNodes.back();
        node.m_childNodes.pop_back();
        assert(node.m_childNodes.empty());
    }

    if(parentNodeIdx != -1 && childNodeIdx != -1)
    {
        for(std::vector<size_t>::iterator it = m_treeNodes[parentNodeIdx].m_childNodes.begin(); it != m_treeNodes[parentNodeIdx].m_childNodes.end(); ++it)
        {
            if(*it == nodeIdx)
            {
                *it = childNodeIdx;
                break;
            }
        }
        for(std::vector<size_t>::iterator it = m_treeNodes[childNodeIdx].m_parentNodes.begin(); it != m_treeNodes[childNodeIdx].m_parentNodes.end(); ++it)
        {
            if(*it == nodeIdx)
            {
                *it = parentNodeIdx;
                break;
            }
        }
    }
    else if(parentNodeIdx != -1)
    {
        for(std::vector<size_t>::iterator it = m_treeNodes[parentNodeIdx].m_childNodes.begin(); it != m_treeNodes[parentNodeIdx].m_childNodes.end(); ++it)
        {
            if(*it == nodeIdx)
            {
                m_treeNodes[parentNodeIdx].m_childNodes.erase(it);
                return;
            }
        }
    }
    else if(childNodeIdx != -1)
    {
        for(std::vector<size_t>::iterator it = m_treeNodes[childNodeIdx].m_parentNodes.begin(); it != m_treeNodes[childNodeIdx].m_parentNodes.end(); ++it)
        {
            if(*it == nodeIdx)
            {
                m_treeNodes[childNodeIdx].m_parentNodes.erase(it);
                return;
            }
        }
    }
}


void MergeTreeBase::addEdge(const size_t lowerNodeIdx, const size_t upperNodeIdx)
{
    m_treeNodes[lowerNodeIdx].m_parentNodes.push_back(upperNodeIdx);
    m_treeNodes[upperNodeIdx].m_childNodes.push_back(lowerNodeIdx);

    assert(m_treeNodes[lowerNodeIdx].m_childNodes.size() <= 1);
    assert(m_treeNodes[upperNodeIdx].m_childNodes.size() <= 1);

//    m_treeNodes[lowerNodeIdx].m_parentSupernodes.push_back(upperNodeIdx);
//    m_treeNodes[upperNodeIdx].m_childSupernodes.push_back(lowerNodeIdx);
}


//void MergeTreeBase::reduceIncidentEdges(const size_t nodeIdx)
//{
//    MergeTreeNode& treeNode = m_treeNodes[nodeIdx];

//    assert(treeNode.m_parentSupernodes.size() == 1);
//    assert(treeNode.m_childSupernodes.size() == 1);

//    size_t parentTreeNodeIdx{treeNode.m_parentSupernodes.back()};
//    treeNode.m_parentSupernodes.pop_back();

//    size_t childTreeNodeIdx{treeNode.m_childSupernodes.back()};
//    treeNode.m_childSupernodes.pop_back();

//    for(std::vector<size_t>::iterator it = m_treeNodes[parentTreeNodeIdx].m_childSupernodes.begin(); it != m_treeNodes[parentTreeNodeIdx].m_childSupernodes.end(); ++it)
//    {
//        if(*it == nodeIdx)
//        {
//            *it = childTreeNodeIdx;
//            break;
//        }
//    }
//    for(std::vector<size_t>::iterator it = m_treeNodes[childTreeNodeIdx].m_parentSupernodes.begin(); it != m_treeNodes[childTreeNodeIdx].m_parentSupernodes.end(); ++it)
//    {
//        if(*it == nodeIdx)
//        {
//            *it = parentTreeNodeIdx;
//            break;
//        }
//    }
//}


template <typename T>
MergeTree<T>::MergeTree(Lattice3Mesh<T>* const mesh)
    : m_mesh{mesh}
{

}


template <typename T>
MergeTree<T>::~MergeTree()
{

}


template <typename T>
void MergeTree<T>::computeMergeTree(float globalMin, float globalMax)
{
    assert(m_mergeMode != MergeMode::NUM_MERGEMODI);

    const std::vector<std::pair<size_t, T> >& sortedNodeIndices = m_mesh->getSortedNodeIndices();
    m_nNodes = sortedNodeIndices.size();

    assert(!(static_cast<float>(sortedNodeIndices.front().second) < globalMin));
    assert(!(static_cast<float>(sortedNodeIndices.back().second) > globalMax));

    if(m_mergeMode == MergeMode::JOIN_TREE)
    {
        IndexValueGreaterComparator<T> comparator;

        m_mesh->resetStoreOffsetNeighbours();
        computeMergeTree(sortedNodeIndices.crbegin(), sortedNodeIndices.crend(), &comparator, globalMin, globalMax);
        m_mesh->unsetStoreOffsetNeighbours();

        computeSortedMaximaAndSaddles();
    }
    else // m_mergeMode == SPLIT_TREE
    {
        IndexValueSmallerComparator<T> comparator;

        m_mesh->resetStoreOffsetNeighbours();
        computeMergeTree(sortedNodeIndices.cbegin(), sortedNodeIndices.cend(), &comparator, globalMin, globalMax);
        m_mesh->unsetStoreOffsetNeighbours();

        computeSortedMaximaAndSaddles();
    }
}


template <typename T>
void MergeTree<T>::computeFastMergeSegmentation(float rangeMin, float rangeMax, float persistenceValue, HxUniformLabelField3* res)
{
    assert(m_mergeMode != MergeMode::NUM_MERGEMODI);
    assert(res);

    typename std::vector<std::pair<size_t, T> >::const_iterator lowerBoundIt = std::lower_bound(m_saddles.cbegin(), m_saddles.cend(), std::pair<size_t, float>(0, rangeMin),
                                                                                                [](const std::pair<size_t, T>& lhs, const std::pair<size_t, float>& rhs) -> bool { return static_cast<float>(lhs.second) < rhs.second; });
    typename std::vector<std::pair<size_t, T> >::const_iterator upperBoundIt = std::upper_bound(lowerBoundIt, m_saddles.cend(), std::pair<size_t, float>(0, rangeMax),
                                                                                                [](const std::pair<size_t, float>& lhs, const std::pair<size_t, T>& rhs) -> bool { return lhs.second < static_cast<float>(rhs.second); });
    typename std::vector<std::pair<size_t, T> >::const_reverse_iterator revUpperBoundIt = std::reverse_iterator< typename std::vector<std::pair<size_t, T> >::const_iterator >(upperBoundIt);
    typename std::vector<std::pair<size_t, T> >::const_reverse_iterator revLowerBoundIt = std::reverse_iterator< typename std::vector<std::pair<size_t, T> >::const_iterator >(lowerBoundIt);

    if(m_mergeMode == MergeMode::JOIN_TREE)
    {
        IndexValueGreaterComparator<T> comparator;
        if(m_segmentationMode == SegmentationMode::DISJOINT)
            computeFastDisjointMergeSegmentation(revUpperBoundIt, revLowerBoundIt, &comparator, m_mesh->getDataMin(), rangeMin, rangeMax, persistenceValue, res);
        else // m_segmentationMode == NESTED || m_segmentationMode == NESTED_CORES
            computeFastNestedMergeSegmentation(revUpperBoundIt, revLowerBoundIt, &comparator, m_mesh->getDataMin(), rangeMin, rangeMax, persistenceValue, res);
    }
    else // m_mergeMode == SPLIT_TREE
    {
        IndexValueSmallerComparator<T> comparator;
        if(m_segmentationMode == SegmentationMode::DISJOINT)
            computeFastDisjointMergeSegmentation(lowerBoundIt, upperBoundIt, &comparator, m_mesh->getDataMax(), rangeMin, rangeMax, persistenceValue, res);
        else // m_segmentationMode == NESTED || m_segmentationMode == NESTED_CORES
            computeFastNestedMergeSegmentation(lowerBoundIt, upperBoundIt, &comparator, m_mesh->getDataMax(), rangeMin, rangeMax, persistenceValue, res);
    }
}


//template <typename T>
//void MergeTree<T>::computeJoinTree(float rangeMin, float rangeMax)
//{
//    const std::vector<std::pair<size_t, T> >& sortedNodeIndices = m_mesh->getSortedNodeIndices();
//    m_nNodes = sortedNodeIndices.size();

//    IndexValueGreaterComparator<T> comparator;
//    computeFullMergeTree(sortedNodeIndices.crbegin(), sortedNodeIndices.crend(), &comparator, rangeMin, rangeMax);
//}


//template <typename T>
//void MergeTree<T>::computeSplitTree(float rangeMin, float rangeMax)
//{
//    const std::vector<std::pair<size_t, T> >& sortedNodeIndices = m_mesh->getSortedNodeIndices();
//    m_nNodes = sortedNodeIndices.size();

//    IndexValueSmallerComparator<T> comparator;
//    computeFullMergeTree(sortedNodeIndices.cbegin(), sortedNodeIndices.cend(), &comparator, rangeMin, rangeMax);
//}


template <typename T>
void MergeTree<T>::sortNeighboursAccordingToMajorityVote(std::vector<std::pair<size_t, T> >& meshVertexNeighbours, SetUnionDataStructure& setUnion)
{
    size_t nodeNeighbourIdx;
    size_t nodeNeighbourComponent;

    std::map<size_t, std::pair<size_t, std::pair<size_t, T> > > componentsMap;
    // ORDER IS ACCENDING!
    std::multimap<size_t, std::pair<size_t, T>, std::less<size_t> > counterMultimap;


    for(const std::pair<size_t, T>& meshVertexNeighbour : meshVertexNeighbours)
    {
        nodeNeighbourIdx = static_cast<size_t>(m_mesh->getNodeIdx(meshVertexNeighbour.first));
        nodeNeighbourComponent = setUnion.findSetId(nodeNeighbourIdx);

        typename std::map<size_t, std::pair<size_t, std::pair<size_t, T> > >::iterator lb = componentsMap.lower_bound(nodeNeighbourComponent);

        if(lb != componentsMap.end() && !(componentsMap.key_comp()(nodeNeighbourComponent, lb->first)))
        {
            ++((lb->second).first); // increment counter
        }
        else
        {
            componentsMap.insert(lb, typename std::map<size_t, std::pair<size_t, std::pair<size_t, T> > >::value_type(nodeNeighbourComponent, std::pair<size_t, std::pair<size_t, T> >(1, meshVertexNeighbour)));
        }
    }


    std::transform(componentsMap.begin(), componentsMap.end(), std::inserter(counterMultimap, counterMultimap.begin()),   // we do NOT want to give a hint (-> counterMultimap.begin()), be we HAVE TO
                   [] (const typename std::map<size_t, std::pair<size_t, std::pair<size_t, T> > >::value_type& pairPairPair) -> typename std::pair<size_t, std::pair<size_t, T> > { return pairPairPair.second; } );


    meshVertexNeighbours.clear();
    meshVertexNeighbours.reserve(counterMultimap.size());

    std::transform(counterMultimap.begin(), counterMultimap.end(), std::back_inserter(meshVertexNeighbours),
                   [] (const typename std::multimap<size_t, std::pair<size_t, T>, std::greater<size_t> >::value_type& pairPair) -> std::pair<size_t, T> { return pairPair.second; } );
}


template <typename T>
void MergeTree<T>::sortNeighboursAccordingToExtrema(std::vector<std::pair<size_t, T> >& meshVertexNeighbours, SetUnionDataStructure& setUnion, IndexValueAbstractBaseComparator<T>* comparator)
{
    size_t nodeNeighbourIdx;
    size_t nodeNeighbourComponent;

    std::map<size_t, std::pair<T, std::pair<size_t, T> > > componentsMap;
    std::multimap<T, std::pair<size_t, T>, IndexValueInvertedComparator<T> > extremaMap{IndexValueInvertedComparator<T>(comparator)};


    for(const std::pair<size_t, T>& meshVertexNeighbour : meshVertexNeighbours)
    {
        nodeNeighbourIdx = static_cast<size_t>(m_mesh->getNodeIdx(meshVertexNeighbour.first));
        nodeNeighbourComponent = setUnion.findSetId(nodeNeighbourIdx);

        typename std::map<size_t, std::pair<T, std::pair<size_t, T> > >::iterator lb = componentsMap.lower_bound(nodeNeighbourComponent);

        if(lb != componentsMap.end() && !(componentsMap.key_comp()(nodeNeighbourComponent, lb->first)))
        {
            // do nothing -- we already stored the corresponding extrema
        }
        else
        {
            componentsMap.insert(lb, typename std::map<size_t, std::pair<T, std::pair<size_t, T> > >::value_type(nodeNeighbourComponent, std::pair<T, std::pair<size_t, T> >(m_extremeValueOfDisjointComponent[nodeNeighbourComponent], meshVertexNeighbour)));
        }
    }


    std::transform(componentsMap.begin(), componentsMap.end(), std::inserter(extremaMap, extremaMap.begin()),   // we do NOT want to give a hint (-> extremaMap.begin()), be we HAVE TO
                   [] (const typename std::map<size_t, std::pair<T, std::pair<size_t, T> > >::value_type& pairPairPair) -> typename std::pair<T, std::pair<size_t, T> > { return pairPairPair.second; } );


    meshVertexNeighbours.clear();
    meshVertexNeighbours.reserve(extremaMap.size());

    std::transform(extremaMap.begin(), extremaMap.end(), std::back_inserter(meshVertexNeighbours),
                   [] (const typename std::multimap<T, std::pair<size_t, T>, IndexValueInvertedComparator<T> >::value_type& pairPair) -> typename std::pair<size_t, T> { return pairPair.second; } );
}


template <typename T>
void MergeTree<T>::computeSortedMaximaAndSaddles()
{
    m_extrema.clear();
    m_saddles.clear();  // This is just some heuristic
    m_saddles.reserve(static_cast<size_t>(std::round(0.1f * static_cast<float>(m_nNodes))));

    for(const std::pair<size_t, T>& pair : m_mesh->getSortedNodeIndices())
    {
        const MergeTreeNode& treeNode = m_treeNodes[pair.first];

        if(treeNode.m_parentNodes.size() < 1)
            m_extrema.emplace_back(pair);
        if(treeNode.m_parentNodes.size() > 1)
            m_saddles.emplace_back(pair);
//        else if(treeNode.m_parentNodes.size() == 1 && treeNode.m_childNodes.size() == 1)
//            reduceIncidentEdges(nodeIdx);
    }
}




template class MergeTree<char>;
template class MergeTree<unsigned char>;
template class MergeTree<short>;
template class MergeTree<unsigned short>;
template class MergeTree<int>;
template class MergeTree<unsigned int>;
template class MergeTree<long long>;
template class MergeTree<unsigned long long>;

template class MergeTree<float>;
template class MergeTree<double>;
