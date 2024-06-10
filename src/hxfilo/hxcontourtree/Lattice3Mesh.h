#ifndef LATTICE3MESH_H
#define LATTICE3MESH_H


#include <mclib/McVec3.h>
#include <mclib/McDim3l.h>
#include <array>
#include <vector>




class HxLattice3;

enum class Interpretation
{
    SPATIAL = 0,
    XY_SLICES,
    SPATIOTEMPORAL,
    NUM_INTERPRETATIONS
};


enum class Connectivity
{
    CORNER = 0,
    EDGE,
    FACE,
    EXP_8P2,
    NUM_CONNECTIVITIES
};




class Lattice3MeshBase
{

public:
    Lattice3MeshBase(const HxLattice3* lattice);
    virtual ~Lattice3MeshBase() {}

    size_t getNumMeshVertices() { return static_cast<size_t>(m_dims.nbVoxel()); }
    void setInterpretation(Interpretation interpretation) { m_interpretation = interpretation; }
    void setConnectivity(Connectivity connectivity) { m_connectivity = connectivity; }

    void setDeformation(const HxLattice3* backwardDeformation, const HxLattice3* forwardDeformation);
    void unsetDeformation();

    inline float getDataMin() { return m_dataMin; }
    inline float getDataMax() { return m_dataMax; }

    virtual void resetStoreOffsetNeighbours() = 0;
    virtual void unsetStoreOffsetNeighbours() = 0;
    virtual void setGlobalRange(const float min, const float max) = 0;
    virtual void unsetGlobalRange() = 0;
    virtual void sortNodeIndices() = 0;

    size_t getMeshVertexIdx(const size_t nodeIdx) const;
    long long getNodeIdx(const size_t meshVertexIdx) const;

    virtual void getAllZNeighboursOfMeshVertex(const size_t meshVertexIdx, std::vector<size_t>& meshVertexNeighbours) = 0;
    virtual size_t getMeshVertexIdxOfMinValue(const std::vector<size_t>& meshVertexIndices) const = 0;
    virtual size_t getMeshVertexIdxOfMaxValue(const std::vector<size_t>& meshVertexIndices) const = 0;

protected:
    std::array<long long, 3> linear2components(const size_t meshVertexIdx) const;
    bool validComponents(const std::array<long long, 3>& components) const;
    size_t components2linear(const std::array<long long, 3>& components) const;
    size_t components2linear(const size_t x, const size_t y, const size_t z) const;


    Interpretation m_interpretation;
    Connectivity m_connectivity;
    bool m_storeOffsetNeighbours;
    bool m_useGlobalRange;

    McDim3l m_dims;
    McVec3f m_voxelSize;
    float m_dataMin;
    float m_dataMax;

    std::vector<long long> m_meshVertex2node;
    std::vector<size_t> m_node2meshVertex;

    int m_nDataVarDeformation;
    const float* m_meshVertexBackwardData;
    const float* m_meshVertexFordwardData;
};




template <typename T>
struct IndexValueAbstractBaseComparator;

template <typename T>
class OutOfRangeChecker;


template <typename T>
class Lattice3Mesh : public Lattice3MeshBase
{

public:
    Lattice3Mesh(const HxLattice3* lattice);
    virtual ~Lattice3Mesh() {}

    virtual void resetStoreOffsetNeighbours();
    virtual void unsetStoreOffsetNeighbours();
    virtual void setGlobalRange(const float min, const float max);
    virtual void unsetGlobalRange();
    virtual void sortNodeIndices();

    const std::vector<std::pair<size_t, T> >& getSortedNodeIndices() const;

    inline T getMeshVertexValue(const size_t meshVertexIdx) const { return m_meshVertexData[meshVertexIdx]; }
    inline T getTreeNodeValue(const size_t nodeIdx) const { return m_meshVertexData[getMeshVertexIdx(nodeIdx)]; }

    virtual void getAllZNeighboursOfMeshVertex(const size_t meshVertexIdx, std::vector<size_t>& meshVertexNeighbours);
    virtual size_t getMeshVertexIdxOfMinValue(const std::vector<size_t>& meshVertexIndices) const;
    virtual size_t getMeshVertexIdxOfMaxValue(const std::vector<size_t>& meshVertexIndices) const;

    void getSmallerNeighboursOfMeshVertex(const size_t meshVertexIdx, std::vector<std::pair<size_t, T> >& meshVertexNeighbours, float rangeMin, float rangeMax);
    void getGreaterNeighboursOfMeshVertex(const size_t meshVertexIdx, std::vector<std::pair<size_t, T> >& meshVertexNeighbours, float rangeMin, float rangeMax);
    void getNeighboursOfMeshVertex(const size_t meshVertexIdx, std::vector<std::pair<size_t, T> >& meshVertexNeighbours, float rangeMin, float rangeMax, const IndexValueAbstractBaseComparator<T>* comparator);

private:
    void getNeighboursOfMeshVertex(const size_t meshVertexIdx, std::vector<std::pair<size_t, T> >& meshVertexNeighbours, const OutOfRangeChecker<T>* outOfRangeChecker, const IndexValueAbstractBaseComparator<T>* comparator);
    void getXYNeighboursOfMeshVertex(const size_t meshVertexIdx, std::vector<std::pair<size_t, T> >& meshVertexNeighbours, const OutOfRangeChecker<T>* outOfRangeChecker, const IndexValueAbstractBaseComparator<T>* comparator);
    void getZNeighboursOfMeshVertex(const size_t meshVertexIdx, std::vector<std::pair<size_t, T> >& meshVertexNeighbours, const OutOfRangeChecker<T>* outOfRangeChecker, const IndexValueAbstractBaseComparator<T>* comparator);
    void addNeighbour(const size_t meshVertexIdx, const std::array<long long, 3>& offsetComponents, std::vector<std::pair<size_t, T> >& meshVertexNeighbours, const OutOfRangeChecker<T>* outOfRangeChecker, const IndexValueAbstractBaseComparator<T>* comparator);


    float m_globalMin;
    float m_globalMax;

    const T* m_meshVertexData;

    std::vector<std::pair<size_t, T> > m_sortedNodeIndices;
    std::vector<std::vector<std::pair<size_t, T> > > m_node2inverseMeshVertexNeighbours;
};


#endif // LATTICE3MESH_H
