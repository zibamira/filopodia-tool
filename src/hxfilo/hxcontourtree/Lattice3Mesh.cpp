#include <hxcontourtree/Lattice3Mesh.h>

#include <cmath>
#include <algorithm>

#include <hxfield/HxLattice3.h>
#include <hxcontourtree/CompareCheckFunctors.h>




Lattice3MeshBase::Lattice3MeshBase(const HxLattice3* lattice)
    : m_interpretation{Interpretation::SPATIAL}
    , m_connectivity{Connectivity::CORNER}
    , m_storeOffsetNeighbours{false}
    , m_useGlobalRange(false)
{
    assert(lattice);
    m_dims = lattice->getDims();
    assert(lattice->coords()->coordType() == C_UNIFORM);
    m_voxelSize = dynamic_cast<HxUniformCoord3*>(lattice->coords())->getVoxelSize();

    lattice->computeRange(m_dataMin, m_dataMax);
}


void Lattice3MeshBase::setDeformation(const HxLattice3* backwardDeformation, const HxLattice3* forwardDeformation)
{
    assert(backwardDeformation);
    assert(forwardDeformation);

    assert(backwardDeformation->getDims() == forwardDeformation->getDims());
    assert(backwardDeformation->getDims() == m_dims);

    assert(backwardDeformation->coords()->coordType() == forwardDeformation->coords()->coordType());
    assert(backwardDeformation->coords()->coordType() == C_UNIFORM);

    assert(dynamic_cast<HxUniformCoord3*>(backwardDeformation->coords())->getVoxelSize() == dynamic_cast<HxUniformCoord3*>(forwardDeformation->coords())->getVoxelSize());
    assert(dynamic_cast<HxUniformCoord3*>(backwardDeformation->coords())->getVoxelSize() == m_voxelSize);

    assert(backwardDeformation->nDataVar() == forwardDeformation->nDataVar());

    m_nDataVarDeformation = backwardDeformation->nDataVar();
    m_meshVertexBackwardData = static_cast<const float*>(backwardDeformation->dataPtr());
    m_meshVertexFordwardData = static_cast<const float*>(forwardDeformation->dataPtr());
}


void Lattice3MeshBase::unsetDeformation()
{
    m_nDataVarDeformation = 0;
    m_meshVertexBackwardData = nullptr;
    m_meshVertexFordwardData = nullptr;
}


size_t Lattice3MeshBase::getMeshVertexIdx(const size_t nodeIdx) const
{
    if(m_useGlobalRange)
        return m_node2meshVertex.at(nodeIdx);
    else
        return nodeIdx;
}


long long Lattice3MeshBase::getNodeIdx(const size_t meshVertexIdx) const
{
    if(m_useGlobalRange)
        return m_meshVertex2node.at(meshVertexIdx);
    else
        return static_cast<long long>(meshVertexIdx);
}


std::array<long long, 3> Lattice3MeshBase::linear2components(const size_t linear) const
{
    std::array<long long, 3> components{ {0, 0, static_cast<long long>(linear) / (m_dims.ny * m_dims.nx)} };
    components[0] = (static_cast<long long>(linear) - components[2] * ((m_dims.ny * m_dims.nx)));
    components[1] = components[0] / m_dims.nx;
    components[0] -= components[1] * m_dims.nx;

    return components;
}


bool Lattice3MeshBase::validComponents(const std::array<long long, 3>& components) const
{
    if(components[0] < 0 || components[1] < 0 || components[2] < 0 ||
       components[0] >= m_dims.nx || components[1] >= m_dims.ny || components[2] >= m_dims.nz)
        return false;

    return true;
}


size_t Lattice3MeshBase::components2linear(const std::array<long long, 3>& components) const
{
    assert(validComponents(components));

    return components2linear(static_cast<size_t>(components[0]),
                             static_cast<size_t>(components[1]),
                             static_cast<size_t>(components[2]));
}


size_t Lattice3MeshBase::components2linear(const size_t x, const size_t y, const size_t z) const
{
    return (((z)*m_dims.ny + y)*m_dims.nx + x);
}




template <typename T>
Lattice3Mesh<T>::Lattice3Mesh(const HxLattice3* lattice)
    : Lattice3MeshBase(lattice)
    , m_globalMin{0.0f}
    , m_globalMax{0.0f}
{
    m_meshVertexData = static_cast<const T*>(lattice->dataPtr());

    m_nDataVarDeformation = 0;
    m_meshVertexBackwardData = nullptr;
    m_meshVertexFordwardData = nullptr;
}


template <typename T>
void Lattice3Mesh<T>::resetStoreOffsetNeighbours()
{
    m_node2inverseMeshVertexNeighbours.assign(m_sortedNodeIndices.size(), std::vector<std::pair<size_t, T> >());
    m_storeOffsetNeighbours = true;
}


template <typename T>
void Lattice3Mesh<T>::unsetStoreOffsetNeighbours()
{
    m_storeOffsetNeighbours = false;
}


template <typename T>
void Lattice3Mesh<T>::setGlobalRange(const float min, const float max)
{
    m_globalMin = min;
    m_globalMax = max;
    m_useGlobalRange = true;
}


template <typename T>
void Lattice3Mesh<T>::unsetGlobalRange()
{
    m_useGlobalRange = false;
    m_globalMin = 0.0f;
    m_globalMax = 0.0f;
}


template <typename T>
void Lattice3Mesh<T>::sortNodeIndices()
{
    const size_t nMeshVertices{getNumMeshVertices()};

    m_meshVertex2node.assign(nMeshVertices, -1);
    m_node2meshVertex.clear();
    m_node2meshVertex.reserve(nMeshVertices);
    m_sortedNodeIndices.clear();
    m_sortedNodeIndices.reserve(nMeshVertices);

    T value;
    float fValue;
    size_t nodeIdx{0};

    for(size_t meshVertexIdx = 0; meshVertexIdx < nMeshVertices; ++meshVertexIdx)
    {
        value = m_meshVertexData[meshVertexIdx];
        fValue = static_cast<float>(value);

        if(m_useGlobalRange && (fValue < m_globalMin || fValue > m_globalMax))
            continue;

        m_sortedNodeIndices.emplace_back(nodeIdx++, value);

        if(m_useGlobalRange)
        {
            m_meshVertex2node[meshVertexIdx] = m_node2meshVertex.size();
            m_node2meshVertex.emplace_back(meshVertexIdx);
        }
    }

    m_node2meshVertex.shrink_to_fit();
    m_sortedNodeIndices.shrink_to_fit();

    std::sort(m_sortedNodeIndices.begin(), m_sortedNodeIndices.end(), IndexValueSmallerComparator<T>());
}


template <typename T>
const std::vector<std::pair<size_t, T> >& Lattice3Mesh<T>::getSortedNodeIndices() const
{
    return m_sortedNodeIndices;
}


template <typename T>
void Lattice3Mesh<T>::getAllZNeighboursOfMeshVertex(const size_t meshVertexIdx, std::vector<size_t>& meshVertexNeighbours)
{
    std::vector<std::pair<size_t, T> > tmpNeighbours;
    tmpNeighbours.reserve(18 + m_node2inverseMeshVertexNeighbours[static_cast<size_t>(getNodeIdx(meshVertexIdx))].size());


    long long offsetBackwardX;
    long long offsetBackwardY;
    long long offsetForwardX;
    long long offsetForwardY;


    if(m_nDataVarDeformation && (m_interpretation == Interpretation::XY_SLICES || m_interpretation == Interpretation::SPATIOTEMPORAL) )
    {
        assert(m_meshVertexBackwardData);
        assert(m_meshVertexFordwardData);

        const std::array<float, 2> backwardDeformation{ {m_meshVertexBackwardData[meshVertexIdx * m_nDataVarDeformation + 0], m_meshVertexBackwardData[meshVertexIdx * m_nDataVarDeformation + 1]} };
        offsetBackwardX = static_cast<long long>(std::round(backwardDeformation[0] / m_voxelSize[0]));
        offsetBackwardY = static_cast<long long>(std::round(backwardDeformation[1] / m_voxelSize[1]));

        const std::array<float, 2> forwardDeformation{ {m_meshVertexFordwardData[meshVertexIdx * m_nDataVarDeformation + 0], m_meshVertexFordwardData[meshVertexIdx * m_nDataVarDeformation + 1]} };
        offsetForwardX = static_cast<long long>(std::round(forwardDeformation[0] / m_voxelSize[0]));
        offsetForwardY = static_cast<long long>(std::round(forwardDeformation[1] / m_voxelSize[1]));
    }
    else
    {
        offsetBackwardX = 0;
        offsetBackwardY = 0;
        offsetForwardX = 0;
        offsetForwardY = 0;
    }


    addNeighbour(meshVertexIdx, { {offsetBackwardX, offsetBackwardY, -1} }, tmpNeighbours, nullptr, nullptr);
    addNeighbour(meshVertexIdx, { { offsetForwardX,  offsetForwardY, +1} }, tmpNeighbours, nullptr, nullptr);

    if(m_interpretation == Interpretation::SPATIAL || m_interpretation == Interpretation::SPATIOTEMPORAL)
    {
        if(m_connectivity == Connectivity::EDGE || m_connectivity == Connectivity::CORNER)
        {
            addNeighbour(meshVertexIdx, { {offsetBackwardX    , offsetBackwardY - 1, -1} }, tmpNeighbours, nullptr, nullptr);
            addNeighbour(meshVertexIdx, { {offsetBackwardX - 1, offsetBackwardY    , -1} }, tmpNeighbours, nullptr, nullptr);
            addNeighbour(meshVertexIdx, { {offsetBackwardX + 1, offsetBackwardY    , -1} }, tmpNeighbours, nullptr, nullptr);
            addNeighbour(meshVertexIdx, { {offsetBackwardX    , offsetBackwardY + 1, -1} }, tmpNeighbours, nullptr, nullptr);

            addNeighbour(meshVertexIdx, { { offsetForwardX    ,  offsetForwardY - 1, +1} }, tmpNeighbours, nullptr, nullptr);
            addNeighbour(meshVertexIdx, { { offsetForwardX - 1,  offsetForwardY    , +1} }, tmpNeighbours, nullptr, nullptr);
            addNeighbour(meshVertexIdx, { { offsetForwardX + 1,  offsetForwardY    , +1} }, tmpNeighbours, nullptr, nullptr);
            addNeighbour(meshVertexIdx, { { offsetForwardX    ,  offsetForwardY + 1, +1} }, tmpNeighbours, nullptr, nullptr);
        }
        if(m_connectivity == Connectivity::CORNER)
        {
            addNeighbour(meshVertexIdx, { {offsetBackwardX - 1, offsetBackwardY - 1, -1} }, tmpNeighbours, nullptr, nullptr);
            addNeighbour(meshVertexIdx, { {offsetBackwardX + 1, offsetBackwardY - 1, -1} }, tmpNeighbours, nullptr, nullptr);
            addNeighbour(meshVertexIdx, { {offsetBackwardX - 1, offsetBackwardY + 1, -1} }, tmpNeighbours, nullptr, nullptr);
            addNeighbour(meshVertexIdx, { {offsetBackwardX + 1, offsetBackwardY + 1, -1} }, tmpNeighbours, nullptr, nullptr);

            addNeighbour(meshVertexIdx, { { offsetForwardX - 1,  offsetForwardY - 1, +1} }, tmpNeighbours, nullptr, nullptr);
            addNeighbour(meshVertexIdx, { { offsetForwardX + 1,  offsetForwardY - 1, +1} }, tmpNeighbours, nullptr, nullptr);
            addNeighbour(meshVertexIdx, { { offsetForwardX - 1,  offsetForwardY + 1, +1} }, tmpNeighbours, nullptr, nullptr);
            addNeighbour(meshVertexIdx, { { offsetForwardX + 1,  offsetForwardY + 1, +1} }, tmpNeighbours, nullptr, nullptr);
        }
    }


    meshVertexNeighbours.reserve(tmpNeighbours.size());
    std::transform(tmpNeighbours.begin(), tmpNeighbours.end(), std::back_inserter(meshVertexNeighbours),
                   [] (const std::pair<size_t, T>& pair) -> size_t { return pair.first; });
}


template <typename T>
size_t Lattice3Mesh<T>::getMeshVertexIdxOfMinValue(const std::vector<size_t>& meshVertexIndices) const
{
    return *std::min_element(meshVertexIndices.cbegin(), meshVertexIndices.cend(),
                            [this] (size_t a, size_t b) -> bool { return m_meshVertexData[a] < m_meshVertexData[b]; } );
}


template <typename T>
size_t Lattice3Mesh<T>::getMeshVertexIdxOfMaxValue(const std::vector<size_t>& meshVertexIndices) const
{
    return *std::max_element(meshVertexIndices.cbegin(), meshVertexIndices.cend(),
                            [this] (size_t a, size_t b) -> bool { return m_meshVertexData[a] < m_meshVertexData[b]; } );
}


template <typename T>
void Lattice3Mesh<T>::getSmallerNeighboursOfMeshVertex(const size_t meshVertexIdx, std::vector<std::pair<size_t, T> >& meshVertexNeighbours, float rangeMin, float rangeMax)
{
    OutOfRangeChecker<T> outOfRangeChecker{rangeMin, rangeMax};
    IndexValueSmallerComparator<T> comparator{};
    getNeighboursOfMeshVertex(meshVertexIdx, meshVertexNeighbours, &outOfRangeChecker, &comparator);
}


template <typename T>
void Lattice3Mesh<T>::getGreaterNeighboursOfMeshVertex(const size_t meshVertexIdx, std::vector<std::pair<size_t, T> >& meshVertexNeighbours, float rangeMin, float rangeMax)
{
    OutOfRangeChecker<T> outOfRangeChecker{rangeMin, rangeMax};
    IndexValueGreaterComparator<T> comparator{};
    getNeighboursOfMeshVertex(meshVertexIdx, meshVertexNeighbours, &outOfRangeChecker, &comparator);
}


template <typename T>
void Lattice3Mesh<T>::getNeighboursOfMeshVertex(const size_t meshVertexIdx, std::vector<std::pair<size_t, T> >& meshVertexNeighbours, float rangeMin, float rangeMax, const IndexValueAbstractBaseComparator<T>* comparator)
{
    OutOfRangeChecker<T> outOfRangeChecker{rangeMin, rangeMax};
    getNeighboursOfMeshVertex(meshVertexIdx, meshVertexNeighbours, &outOfRangeChecker, comparator);
}


template <typename T>
void Lattice3Mesh<T>::getNeighboursOfMeshVertex(const size_t meshVertexIdx, std::vector<std::pair<size_t, T> >& meshVertexNeighbours, const OutOfRangeChecker<T>* outOfRangeChecker, const IndexValueAbstractBaseComparator<T>* comparator)
{
    meshVertexNeighbours.reserve(26 + m_node2inverseMeshVertexNeighbours[static_cast<size_t>(getNodeIdx(meshVertexIdx))].size());
    getXYNeighboursOfMeshVertex(meshVertexIdx, meshVertexNeighbours, outOfRangeChecker, comparator);
    getZNeighboursOfMeshVertex(meshVertexIdx, meshVertexNeighbours, outOfRangeChecker, comparator);
}


template <typename T>
void Lattice3Mesh<T>::getXYNeighboursOfMeshVertex(const size_t meshVertexIdx, std::vector<std::pair<size_t, T> >& meshVertexNeighbours, const OutOfRangeChecker<T>* outOfRangeChecker, const IndexValueAbstractBaseComparator<T>* comparator)
{
    if(m_interpretation == Interpretation::XY_SLICES)
    {
        assert(m_connectivity != Connectivity::FACE);
        assert(m_connectivity != Connectivity::EXP_8P2);

        if(m_connectivity == Connectivity::EDGE || m_connectivity == Connectivity::CORNER)
        {
            addNeighbour(meshVertexIdx, { {  0, -1,  0} }, meshVertexNeighbours, outOfRangeChecker, comparator);
            addNeighbour(meshVertexIdx, { { -1,  0,  0} }, meshVertexNeighbours, outOfRangeChecker, comparator);
            addNeighbour(meshVertexIdx, { { +1,  0,  0} }, meshVertexNeighbours, outOfRangeChecker, comparator);
            addNeighbour(meshVertexIdx, { {  0, +1,  0} }, meshVertexNeighbours, outOfRangeChecker, comparator);
        }

        if(m_connectivity == Connectivity::CORNER)
        {
            addNeighbour(meshVertexIdx, { { -1, -1,  0} }, meshVertexNeighbours, outOfRangeChecker, comparator);
            addNeighbour(meshVertexIdx, { { +1, -1,  0} }, meshVertexNeighbours, outOfRangeChecker, comparator);
            addNeighbour(meshVertexIdx, { { -1, +1,  0} }, meshVertexNeighbours, outOfRangeChecker, comparator);
            addNeighbour(meshVertexIdx, { { +1, +1,  0} }, meshVertexNeighbours, outOfRangeChecker, comparator);
        }
    }
    else // m_interpretation == Interpretation::VOLUME || m_interpretation == Interpretation::EXPERIMENTAL
    {

        if(m_connectivity == Connectivity::FACE || m_connectivity == Connectivity::EDGE || m_connectivity == Connectivity::CORNER || m_connectivity == Connectivity::EXP_8P2)
        {
            addNeighbour(meshVertexIdx, { {  0, -1,  0} }, meshVertexNeighbours, outOfRangeChecker, comparator);
            addNeighbour(meshVertexIdx, { { -1,  0,  0} }, meshVertexNeighbours, outOfRangeChecker, comparator);
            addNeighbour(meshVertexIdx, { { +1,  0,  0} }, meshVertexNeighbours, outOfRangeChecker, comparator);
            addNeighbour(meshVertexIdx, { {  0, +1,  0} }, meshVertexNeighbours, outOfRangeChecker, comparator);

        }
        if(m_connectivity == Connectivity::EDGE || m_connectivity == Connectivity::CORNER || m_connectivity == Connectivity::EXP_8P2)
        {
            addNeighbour(meshVertexIdx, { { -1, -1,  0} }, meshVertexNeighbours, outOfRangeChecker, comparator);
            addNeighbour(meshVertexIdx, { { +1, -1,  0} }, meshVertexNeighbours, outOfRangeChecker, comparator);
            addNeighbour(meshVertexIdx, { { -1, +1,  0} }, meshVertexNeighbours, outOfRangeChecker, comparator);
            addNeighbour(meshVertexIdx, { { +1, +1,  0} }, meshVertexNeighbours, outOfRangeChecker, comparator);
        }
    }
}


template <typename T>
void Lattice3Mesh<T>::getZNeighboursOfMeshVertex(const size_t meshVertexIdx, std::vector<std::pair<size_t, T> >& meshVertexNeighbours, const OutOfRangeChecker<T>* outOfRangeChecker, const IndexValueAbstractBaseComparator<T>* comparator)
{
    if(m_interpretation == Interpretation::XY_SLICES)
        return;

    long long offsetBackwardX;
    long long offsetBackwardY;
    long long offsetForwardX;
    long long offsetForwardY;


    if(m_nDataVarDeformation && m_interpretation == Interpretation::SPATIOTEMPORAL)
    {
        assert(m_meshVertexBackwardData);
        assert(m_meshVertexFordwardData);

        const std::array<float, 2> backwardDeformation{ {m_meshVertexBackwardData[meshVertexIdx * m_nDataVarDeformation + 0], m_meshVertexBackwardData[meshVertexIdx * m_nDataVarDeformation + 1]} };
        offsetBackwardX = static_cast<long long>(std::round(backwardDeformation[0] / m_voxelSize[0]));
        offsetBackwardY = static_cast<long long>(std::round(backwardDeformation[1] / m_voxelSize[1]));

        const std::array<float, 2> forwardDeformation{ {m_meshVertexFordwardData[meshVertexIdx * m_nDataVarDeformation + 0], m_meshVertexFordwardData[meshVertexIdx * m_nDataVarDeformation + 1]} };
        offsetForwardX = static_cast<long long>(std::round(forwardDeformation[0] / m_voxelSize[0]));
        offsetForwardY = static_cast<long long>(std::round(forwardDeformation[1] / m_voxelSize[1]));

        if(comparator)
        {
            size_t nodeIdx = static_cast<size_t>(getNodeIdx(meshVertexIdx));
            meshVertexNeighbours.insert(meshVertexNeighbours.end(), m_node2inverseMeshVertexNeighbours[nodeIdx].begin(), m_node2inverseMeshVertexNeighbours[nodeIdx].end());
        }
    }
    else
    {
        offsetBackwardX = 0;
        offsetBackwardY = 0;
        offsetForwardX = 0;
        offsetForwardY = 0;
    }


    addNeighbour(meshVertexIdx, { {offsetBackwardX, offsetBackwardY, -1} }, meshVertexNeighbours, outOfRangeChecker, comparator);
    addNeighbour(meshVertexIdx, { { offsetForwardX,  offsetForwardY, +1} }, meshVertexNeighbours, outOfRangeChecker, comparator);

    if(m_connectivity == Connectivity::EDGE || m_connectivity == Connectivity::CORNER)
    {
        addNeighbour(meshVertexIdx, { {offsetBackwardX    , offsetBackwardY - 1, -1} }, meshVertexNeighbours, outOfRangeChecker, comparator);
        addNeighbour(meshVertexIdx, { {offsetBackwardX - 1, offsetBackwardY    , -1} }, meshVertexNeighbours, outOfRangeChecker, comparator);
        addNeighbour(meshVertexIdx, { {offsetBackwardX + 1, offsetBackwardY    , -1} }, meshVertexNeighbours, outOfRangeChecker, comparator);
        addNeighbour(meshVertexIdx, { {offsetBackwardX    , offsetBackwardY + 1, -1} }, meshVertexNeighbours, outOfRangeChecker, comparator);

        addNeighbour(meshVertexIdx, { { offsetForwardX    ,  offsetForwardY - 1, +1} }, meshVertexNeighbours, outOfRangeChecker, comparator);
        addNeighbour(meshVertexIdx, { { offsetForwardX - 1,  offsetForwardY    , +1} }, meshVertexNeighbours, outOfRangeChecker, comparator);
        addNeighbour(meshVertexIdx, { { offsetForwardX + 1,  offsetForwardY    , +1} }, meshVertexNeighbours, outOfRangeChecker, comparator);
        addNeighbour(meshVertexIdx, { { offsetForwardX    ,  offsetForwardY + 1, +1} }, meshVertexNeighbours, outOfRangeChecker, comparator);
    }
    if(m_connectivity == Connectivity::CORNER)
    {
        addNeighbour(meshVertexIdx, { {offsetBackwardX - 1, offsetBackwardY - 1, -1} }, meshVertexNeighbours, outOfRangeChecker, comparator);
        addNeighbour(meshVertexIdx, { {offsetBackwardX + 1, offsetBackwardY - 1, -1} }, meshVertexNeighbours, outOfRangeChecker, comparator);
        addNeighbour(meshVertexIdx, { {offsetBackwardX - 1, offsetBackwardY + 1, -1} }, meshVertexNeighbours, outOfRangeChecker, comparator);
        addNeighbour(meshVertexIdx, { {offsetBackwardX + 1, offsetBackwardY + 1, -1} }, meshVertexNeighbours, outOfRangeChecker, comparator);

        addNeighbour(meshVertexIdx, { { offsetForwardX - 1,  offsetForwardY - 1, +1} }, meshVertexNeighbours, outOfRangeChecker, comparator);
        addNeighbour(meshVertexIdx, { { offsetForwardX + 1,  offsetForwardY - 1, +1} }, meshVertexNeighbours, outOfRangeChecker, comparator);
        addNeighbour(meshVertexIdx, { { offsetForwardX - 1,  offsetForwardY + 1, +1} }, meshVertexNeighbours, outOfRangeChecker, comparator);
        addNeighbour(meshVertexIdx, { { offsetForwardX + 1,  offsetForwardY + 1, +1} }, meshVertexNeighbours, outOfRangeChecker, comparator);
    }
}


template <typename T>
void Lattice3Mesh<T>::addNeighbour(const size_t meshVertexIdx, const std::array<long long, 3>& offsetComponents, std::vector<std::pair<size_t, T> >& meshVertexNeighbours, const OutOfRangeChecker<T>* outOfRangeChecker, const IndexValueAbstractBaseComparator<T>* comparator)
{
    std::array<long long, 3> components = linear2components(meshVertexIdx);
    components[0] += offsetComponents[0];
    components[1] += offsetComponents[1];
    components[2] += offsetComponents[2];

    if(!validComponents(components))
        return;

    const size_t meshVertexNeighbourIdx{components2linear(static_cast<size_t>(components[0]), static_cast<size_t>(components[1]), static_cast<size_t>(components[2]))};
    const T meshVertexNeighbourValue{m_meshVertexData[meshVertexNeighbourIdx]};

    if(outOfRangeChecker && outOfRangeChecker->operator ()(meshVertexNeighbourValue))
        return;

    if(comparator)
    {
        if(comparator->operator ()({meshVertexNeighbourIdx, meshVertexNeighbourValue}, {meshVertexIdx, m_meshVertexData[meshVertexIdx]}))
            meshVertexNeighbours.emplace_back( meshVertexNeighbourIdx, meshVertexNeighbourValue );
        else if(m_storeOffsetNeighbours)
            m_node2inverseMeshVertexNeighbours[getNodeIdx(meshVertexNeighbourIdx)].emplace_back( meshVertexIdx, m_meshVertexData[meshVertexIdx] );
    }
    else
    {
        assert(!m_storeOffsetNeighbours);
        meshVertexNeighbours.emplace_back( meshVertexNeighbourIdx, meshVertexNeighbourValue );
    }
}




template class Lattice3Mesh<char>;
template class Lattice3Mesh<unsigned char>;
template class Lattice3Mesh<short>;
template class Lattice3Mesh<unsigned short>;
template class Lattice3Mesh<int>;
template class Lattice3Mesh<unsigned int>;
template class Lattice3Mesh<long long>;
template class Lattice3Mesh<unsigned long long>;

template class Lattice3Mesh<float>;
template class Lattice3Mesh<double>;
