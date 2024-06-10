#include <mclib/internal/McSorter.h>
#include <mclib/internal/McComparators.h>
#include <mclib/McVec2.h>
#include <mclib/McVec3.h>

#include <hxfield/HxLattice3.h>

#include "SimplicialMesh3DForHexahedralMesh.h"

template <typename T>
int
operator<(const McVec2<T>& a, const McVec2<T>& b)
{
    if (a.x != b.x)
        return (a.x < b.x);
    return (a.y < b.y);
}

template <typename T>
int
operator>(const McVec2<T>& a, const McVec2<T>& b)
{
    if (a.x != b.x)
        return (a.x > b.x);
    return (a.y > b.y);
}

SimplicialMesh3DForHexahedralMesh::SimplicialMesh3DForHexahedralMesh(
    HxLattice3* lattice)
{
    m_lattice = lattice;
    m_threshold = 0.0;
    m_useThreshold = false;
    m_findMax = true;

    const McDim3l& dims = m_lattice->getDims();
    m_dims[0] = dims[0];
    m_dims[1] = dims[1];
    m_dims[2] = dims[2];

    m_numNodes = 0;

    m_neighborhood = SimplicialMesh3DForHexahedralMesh::NEIGHBORHOOD_SIMPLICIALMESH;
}

SimplicialMesh3DForHexahedralMesh::~SimplicialMesh3DForHexahedralMesh()
{
}

void
SimplicialMesh3DForHexahedralMesh::setThreshold(
    const double threshold)
{
    m_threshold = threshold;
    m_useThreshold = true;
}

void
SimplicialMesh3DForHexahedralMesh::setFindMax(bool findMax){
    m_findMax = findMax;
}

mculong
SimplicialMesh3DForHexahedralMesh::getIdxFromGridPos(
    const mculong x,
    const mculong y,
    const mculong z) const
{
    mculong idx = z * (m_dims[1] * m_dims[0]) + y * m_dims[0] + x;

    return idx;
}

void
SimplicialMesh3DForHexahedralMesh::getGridPosFromIdx(
    const mculong idx,
    McVec3i& gridPos) const
{
    const mculong z = (mculong)idx / (m_dims[1] * m_dims[0]);

    const mculong rest = idx - z * (m_dims[1] * m_dims[0]);

    const mculong y = (mculong)rest / m_dims[0];

    const mculong x = rest - y * m_dims[0];

    gridPos[0] = int(x);
    gridPos[1] = int(y);
    gridPos[2] = int(z);
}

void
SimplicialMesh3DForHexahedralMesh::getSortedListOfVertices(const bool computeSplitTree,
    std::vector<mculong>& sortedListOfVertices)
{
    if (m_useThreshold)
    {
        const mculong size = m_dims[0] * m_dims[1] * m_dims[2];
        m_mapMeshVertexToNode.assign(size, -1);
    }

    std::vector<McVec2d> tuples;

    sortedListOfVertices.clear();
    m_mapNodeToMeshVertex.clear();

    m_numNodes = 0;
    for (mculong z = 0; z < m_dims[2]; ++z)
    {
        for (mculong y = 0; y < m_dims[1]; ++y)
        {
            mculong idx = getIdxFromGridPos(0, y, z);
            for (mculong x = 0; x < m_dims[0]; ++x, ++idx)
            {
                float value = -1.0;
                m_lattice->eval(idx, &value);

                if (!m_useThreshold || (m_findMax && double(value) >= m_threshold) || (!m_findMax &&  double(value) <= m_threshold))
                {
                    if (computeSplitTree)
                        tuples.emplace_back(McVec2d(-1.0 * double(value), -1.0 * double(idx)));
                    else
                        tuples.emplace_back(McVec2d(double(value), double(idx)));

                    sortedListOfVertices.push_back(m_numNodes);
                    ++m_numNodes;

                    if (m_useThreshold)
                    {
                        m_mapMeshVertexToNode[idx] = m_mapNodeToMeshVertex.size();
                        m_mapNodeToMeshVertex.push_back(idx);
                    }
                }
            }
        }
    }

    if (m_numNodes == 0)
        return;

    McIndexByValueDescendingComparator<McVec2d> comp(&tuples[0]);
    sort(&sortedListOfVertices[0],
         sortedListOfVertices.size(),
         comp);
}

void
SimplicialMesh3DForHexahedralMesh::getGridPos(
    const std::vector<mclong>& vertexIds,
    std::vector<McVec3i>& gridPos) const
{
    const mculong numVertices = vertexIds.size();
    gridPos.resize(numVertices);

    for (mculong i = 0; i < numVertices; ++i)
        getGridPosFromIdx(vertexIds[i], gridPos[i]);
}

void
SimplicialMesh3DForHexahedralMesh::getGridPos(
    std::vector<McVec3i>& gridPos) const
{
    gridPos.resize(m_numNodes);

    if (m_useThreshold)
    {
        for (mculong i = 0; i < m_numNodes; ++i)
            getGridPosFromIdx(m_mapNodeToMeshVertex[i], gridPos[i]);
    }
    else
    {
        for (mculong i = 0; i < m_numNodes; ++i)
            getGridPosFromIdx(i, gridPos[i]);
    }
}

mclong
SimplicialMesh3DForHexahedralMesh::getMeshVertexIdx(
    const mclong nodeIdx) const
{
    if (m_useThreshold)
        return m_mapNodeToMeshVertex[nodeIdx];
    else
        return nodeIdx;
}

mclong
SimplicialMesh3DForHexahedralMesh::getNodeIdx(
    const mclong vertexIdx) const
{
    if (m_useThreshold)
        return m_mapMeshVertexToNode[vertexIdx];
    else
        return vertexIdx;
}

void
SimplicialMesh3DForHexahedralMesh::get26NeighborHoodOfMeshVertexIgnoringThreshold(
    const mculong vertexIdx,
    std::vector<mculong>& neighbors)
{
    McVec3i gridPos;
    getGridPosFromIdx(vertexIdx, gridPos);

    // clang-format off

    // Add 6 direct neighbors of hexahedral mesh
    addNeighbor(gridPos, McVec3i( 1,  0,  0), neighbors);
    addNeighbor(gridPos, McVec3i(-1,  0,  0), neighbors);
    addNeighbor(gridPos, McVec3i( 0,  1,  0), neighbors);
    addNeighbor(gridPos, McVec3i( 0, -1,  0), neighbors);
    addNeighbor(gridPos, McVec3i( 0,  0,  1), neighbors);
    addNeighbor(gridPos, McVec3i( 0,  0, -1), neighbors);

    // Add 12 ring neighbors
    addNeighbor(gridPos, McVec3i( 1,  1,  0), neighbors);
    addNeighbor(gridPos, McVec3i(-1, -1,  0), neighbors);
    addNeighbor(gridPos, McVec3i(-1,  1,  0), neighbors);
    addNeighbor(gridPos, McVec3i( 1, -1,  0), neighbors);
    addNeighbor(gridPos, McVec3i( 0,  1,  1), neighbors);
    addNeighbor(gridPos, McVec3i( 0, -1, -1), neighbors);
    addNeighbor(gridPos, McVec3i( 0, -1,  1), neighbors);
    addNeighbor(gridPos, McVec3i( 0,  1, -1), neighbors);
    addNeighbor(gridPos, McVec3i( 1,  0,  1), neighbors);
    addNeighbor(gridPos, McVec3i(-1,  0, -1), neighbors);
    addNeighbor(gridPos, McVec3i(-1,  0,  1), neighbors);
    addNeighbor(gridPos, McVec3i( 1,  0, -1), neighbors);

    // Add 8 main diagonal neighbors
    addNeighbor(gridPos, McVec3i( 1,  1,  1), neighbors);
    addNeighbor(gridPos, McVec3i( 1,  1, -1), neighbors);
    addNeighbor(gridPos, McVec3i( 1, -1,  1), neighbors);
    addNeighbor(gridPos, McVec3i(-1,  1,  1), neighbors);
    addNeighbor(gridPos, McVec3i( 1, -1, -1), neighbors);
    addNeighbor(gridPos, McVec3i(-1,  1, -1), neighbors);
    addNeighbor(gridPos, McVec3i(-1, -1,  1), neighbors);
    addNeighbor(gridPos, McVec3i(-1, -1, -1), neighbors);

    // clang-format on
}

void
SimplicialMesh3DForHexahedralMesh::addNeighbor(
    const McVec3i& gridPos,
    const McVec3i& offSet,
    std::vector<mculong>& neighbors) const
{
    McVec3i neighborGridPos = gridPos + offSet;
    if (!isValidGridPos(neighborGridPos))
        return;

    const mculong neighborIdx = getIdxFromGridPos(neighborGridPos[0],
                                                  neighborGridPos[1],
                                                  neighborGridPos[2]);

    neighbors.push_back(neighborIdx);
}

void
SimplicialMesh3DForHexahedralMesh::getNeighborsOfNode(
    const bool neighborsWithSmallerValue,
    const mculong nodeIdx,
    std::vector<mculong>& neighbors,
    std::vector<float>& values) const
{
    const mculong vertexIdx = getMeshVertexIdx(nodeIdx);

    getNeighborsOfMeshVertex(neighborsWithSmallerValue, vertexIdx, neighbors, values);
}

void
SimplicialMesh3DForHexahedralMesh::getNeighborsOfMeshVertex(
    const bool neighborsWithSmallerValue,
    const mculong vertexIdx,
    std::vector<mculong>& neighbors,
    std::vector<float>& values) const
{
    const int typeOfNeighborHood = getNeighborhood();

    McVec3i gridPos;
    getGridPosFromIdx(vertexIdx, gridPos);

    float value = -1.0;
    m_lattice->eval(vertexIdx, &value);

    McVec2d vertex((double)value, (double)vertexIdx);

    if (neighborsWithSmallerValue)
        vertex *= -1.0;

    // clang-format off

    // Add direct neighbors of hexahedral mesh
    addNeighbor(neighborsWithSmallerValue, vertex, gridPos, McVec3i( 1,  0,  0), neighbors, values);
    addNeighbor(neighborsWithSmallerValue, vertex, gridPos, McVec3i(-1,  0,  0), neighbors, values);
    addNeighbor(neighborsWithSmallerValue, vertex, gridPos, McVec3i( 0,  1,  0), neighbors, values);
    addNeighbor(neighborsWithSmallerValue, vertex, gridPos, McVec3i( 0, -1,  0), neighbors, values);
    addNeighbor(neighborsWithSmallerValue, vertex, gridPos, McVec3i( 0,  0,  1), neighbors, values);
    addNeighbor(neighborsWithSmallerValue, vertex, gridPos, McVec3i( 0,  0, -1), neighbors, values);

    if (    typeOfNeighborHood == NEIGHBORHOOD_SIMPLICIALMESH
         || typeOfNeighborHood == NEIGHBORHOOD_26 )
    {
        // Add diagonal neighbors on faces for simplicial mesh
        addNeighbor(neighborsWithSmallerValue, vertex, gridPos, McVec3i( 1,  1,  0), neighbors, values);
        addNeighbor(neighborsWithSmallerValue, vertex, gridPos, McVec3i(-1, -1,  0), neighbors, values);
        addNeighbor(neighborsWithSmallerValue, vertex, gridPos, McVec3i( 0,  1,  1), neighbors, values);
        addNeighbor(neighborsWithSmallerValue, vertex, gridPos, McVec3i( 0, -1, -1), neighbors, values);
        addNeighbor(neighborsWithSmallerValue, vertex, gridPos, McVec3i( 1,  0,  1), neighbors, values);
        addNeighbor(neighborsWithSmallerValue, vertex, gridPos, McVec3i(-1,  0, -1), neighbors, values);

        // Add main diagonal neighbors for simplicial mesh
        addNeighbor(neighborsWithSmallerValue, vertex, gridPos, McVec3i( 1,  1,  1), neighbors, values);
        addNeighbor(neighborsWithSmallerValue, vertex, gridPos, McVec3i(-1, -1, -1), neighbors, values);
    }

    if ( typeOfNeighborHood == NEIGHBORHOOD_26 )
    {
        // Add remaining edge connected neighbors
        addNeighbor(neighborsWithSmallerValue, vertex, gridPos, McVec3i( 1, -1,  0), neighbors, values);
        addNeighbor(neighborsWithSmallerValue, vertex, gridPos, McVec3i(-1,  1,  0), neighbors, values);
        addNeighbor(neighborsWithSmallerValue, vertex, gridPos, McVec3i( 0,  1, -1), neighbors, values);
        addNeighbor(neighborsWithSmallerValue, vertex, gridPos, McVec3i( 0, -1,  1), neighbors, values);
        addNeighbor(neighborsWithSmallerValue, vertex, gridPos, McVec3i( 1,  0, -1), neighbors, values);
        addNeighbor(neighborsWithSmallerValue, vertex, gridPos, McVec3i(-1,  0,  1), neighbors, values);

        // Add remaining vertex connected neighbors
        addNeighbor(neighborsWithSmallerValue, vertex, gridPos, McVec3i( 1,  1, -1), neighbors, values);
        addNeighbor(neighborsWithSmallerValue, vertex, gridPos, McVec3i( 1, -1,  1), neighbors, values);
        addNeighbor(neighborsWithSmallerValue, vertex, gridPos, McVec3i(-1,  1,  1), neighbors, values);
        addNeighbor(neighborsWithSmallerValue, vertex, gridPos, McVec3i( 1, -1, -1), neighbors, values);
        addNeighbor(neighborsWithSmallerValue, vertex, gridPos, McVec3i(-1,  1, -1), neighbors, values);
        addNeighbor(neighborsWithSmallerValue, vertex, gridPos, McVec3i(-1, -1,  1), neighbors, values);
    }

    // clang-format on
}

void
SimplicialMesh3DForHexahedralMesh::addNeighbor(
    const bool neighborsWithSmallerValue,
    const McVec2d& vertex,
    const McVec3i& gridPos,
    const McVec3i& offSet,
    std::vector<mculong>& neighbors,
    std::vector<float>& values) const
{
    McVec3i neighborGridPos = gridPos + offSet;
    if (!isValidGridPos(neighborGridPos))
        return;

    const mculong neighborIdx = getIdxFromGridPos(neighborGridPos[0],
                                                  neighborGridPos[1],
                                                  neighborGridPos[2]);

    float neighborValue = -1.0;
    m_lattice->eval(neighborIdx, &neighborValue);

    if ((m_findMax && !(((double)neighborValue) >= m_threshold)) ||
        (!m_findMax && !(((double)neighborValue) <= m_threshold)))
        return;

    McVec2d neighbor((double)neighborValue, (double)neighborIdx);

    if (neighborsWithSmallerValue)
        neighbor *= -1.0;

    if (neighbor > vertex)
    {
        neighbors.push_back(neighborIdx);
        values.push_back(neighborValue);
    }
}

bool
SimplicialMesh3DForHexahedralMesh::isValidGridPos(
    const McVec3i& gridPos) const
{
    if (gridPos[0] < 0 || gridPos[1] < 0 || gridPos[2] < 0 || gridPos[0] >= int(m_dims[0]) || gridPos[1] >= int(m_dims[1]) || gridPos[2] >= int(m_dims[2]))
        return false;

    return true;
}

void
SimplicialMesh3DForHexahedralMesh::setNeighborhood(const int neighborhood)
{
    m_neighborhood = neighborhood;
}

int
SimplicialMesh3DForHexahedralMesh::getNeighborhood() const
{
    return m_neighborhood;
}
