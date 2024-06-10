#include <mclib/McVec3.h>

#include "SimplicialMesh2D_Test.h"

SimplicialMesh2D_Test::SimplicialMesh2D_Test()
{
    m_threshold           = 0.0;
    m_useThreshold        = false;

    m_numNodes = 18;

    m_nodeValues.resize(m_numNodes);
    m_nodeCoords.resize(m_numNodes);
    m_nodeCoords[ 0] = McVec3f( 7.f, 34.f, 0.f); m_nodeValues[ 0] = 10.f;
    m_nodeCoords[ 1] = McVec3f( 0.f, 32.f, 0.f); m_nodeValues[ 1] =  9.f;
    m_nodeCoords[ 2] = McVec3f( 6.f, 30.f, 0.f); m_nodeValues[ 2] =  8.3;
    m_nodeCoords[ 3] = McVec3f(16.f, 28.f, 0.f); m_nodeValues[ 3] =  8.f;
    m_nodeCoords[ 4] = McVec3f( 5.f, 26.f, 0.f); m_nodeValues[ 4] =  7.2;
    m_nodeCoords[ 5] = McVec3f(22.f, 24.f, 0.f); m_nodeValues[ 5] =  7.f;
    m_nodeCoords[ 6] = McVec3f(18.f, 22.f, 0.f); m_nodeValues[ 6] =  6.9;
    m_nodeCoords[ 7] = McVec3f( 2.f, 20.f, 0.f); m_nodeValues[ 7] =  6.5;
    m_nodeCoords[ 8] = McVec3f( 4.f, 18.f, 0.f); m_nodeValues[ 8] =  6.1;
    m_nodeCoords[ 9] = McVec3f(20.f, 16.f, 0.f); m_nodeValues[ 9] =  6.f;
    m_nodeCoords[10] = McVec3f( 4.f, 14.f, 0.f); m_nodeValues[10] =  5.f;
    m_nodeCoords[11] = McVec3f(16.f, 12.f, 0.f); m_nodeValues[11] =  4.9;
    m_nodeCoords[12] = McVec3f( 8.f, 10.f, 0.f); m_nodeValues[12] =  4.6;
    m_nodeCoords[13] = McVec3f(12.f,  8.f, 0.f); m_nodeValues[13] =  4.f;
    m_nodeCoords[14] = McVec3f(12.f,  6.f, 0.f); m_nodeValues[14] =  3.f;
    m_nodeCoords[15] = McVec3f(10.f,  4.f, 0.f); m_nodeValues[15] =  2.1;
    m_nodeCoords[16] = McVec3f(14.f,  2.f, 0.f); m_nodeValues[16] =  2.f; 
    m_nodeCoords[17] = McVec3f( 8.f,  0.f, 0.f); m_nodeValues[17] =  1.f;

    m_nodeNeighbors.resize(m_numNodes);

    m_nodeNeighbors[ 0].push_back(10);
    m_nodeNeighbors[ 0].push_back( 2);
    m_nodeNeighbors[ 0].push_back(13);

    m_nodeNeighbors[ 1].push_back(10);
    m_nodeNeighbors[ 1].push_back( 7);
    m_nodeNeighbors[ 1].push_back(14);
    m_nodeNeighbors[ 1].push_back(15);
    m_nodeNeighbors[ 1].push_back(17);

    m_nodeNeighbors[ 2].push_back( 0);
    m_nodeNeighbors[ 2].push_back( 4);
    m_nodeNeighbors[ 2].push_back( 8);

    m_nodeNeighbors[ 3].push_back(17);
    m_nodeNeighbors[ 3].push_back(15);
    m_nodeNeighbors[ 3].push_back(14);
    m_nodeNeighbors[ 3].push_back( 6);
    m_nodeNeighbors[ 3].push_back( 9);

    m_nodeNeighbors[ 4].push_back( 2);
    m_nodeNeighbors[ 4].push_back( 8);
    m_nodeNeighbors[ 4].push_back(10);
    
    m_nodeNeighbors[ 5].push_back( 9);
    m_nodeNeighbors[ 5].push_back(13);

    m_nodeNeighbors[ 6].push_back(11);
    m_nodeNeighbors[ 6].push_back(14);
    m_nodeNeighbors[ 6].push_back( 3);
    m_nodeNeighbors[ 6].push_back( 9);

    m_nodeNeighbors[ 7].push_back( 1);
    m_nodeNeighbors[ 7].push_back(10);
    m_nodeNeighbors[ 7].push_back(14);

    m_nodeNeighbors[ 8].push_back( 4);
    m_nodeNeighbors[ 8].push_back( 2);
    m_nodeNeighbors[ 8].push_back(13);

    m_nodeNeighbors[ 9].push_back( 3);
    m_nodeNeighbors[ 9].push_back( 6);
    m_nodeNeighbors[ 9].push_back(11);
    m_nodeNeighbors[ 9].push_back(16);
    m_nodeNeighbors[ 9].push_back(13);
    m_nodeNeighbors[ 9].push_back( 5);

    m_nodeNeighbors[10].push_back( 0);
    m_nodeNeighbors[10].push_back( 4);
    m_nodeNeighbors[10].push_back(13);
    m_nodeNeighbors[10].push_back(12);
    m_nodeNeighbors[10].push_back(16);
    m_nodeNeighbors[10].push_back(14);
    m_nodeNeighbors[10].push_back( 7);
    m_nodeNeighbors[10].push_back( 1);

    m_nodeNeighbors[11].push_back(16);
    m_nodeNeighbors[11].push_back( 9);
    m_nodeNeighbors[11].push_back( 6);
    m_nodeNeighbors[11].push_back(14);

    m_nodeNeighbors[12].push_back(16);
    m_nodeNeighbors[12].push_back(10);
    m_nodeNeighbors[12].push_back(13);

    m_nodeNeighbors[13].push_back( 0);
    m_nodeNeighbors[13].push_back( 8);
    m_nodeNeighbors[13].push_back(10);
    m_nodeNeighbors[13].push_back(12);
    m_nodeNeighbors[13].push_back(16);
    m_nodeNeighbors[13].push_back( 9);
    m_nodeNeighbors[13].push_back( 5);

    m_nodeNeighbors[14].push_back(16);
    m_nodeNeighbors[14].push_back(11);
    m_nodeNeighbors[14].push_back( 6);
    m_nodeNeighbors[14].push_back( 3);
    m_nodeNeighbors[14].push_back(15);
    m_nodeNeighbors[14].push_back( 1);
    m_nodeNeighbors[14].push_back( 7);
    m_nodeNeighbors[14].push_back(10);

    m_nodeNeighbors[15].push_back( 1);
    m_nodeNeighbors[15].push_back(14);
    m_nodeNeighbors[15].push_back( 3);
    m_nodeNeighbors[15].push_back(17);

    m_nodeNeighbors[16].push_back(10);
    m_nodeNeighbors[16].push_back(12);
    m_nodeNeighbors[16].push_back(13);
    m_nodeNeighbors[16].push_back( 9);
    m_nodeNeighbors[16].push_back(11);
    m_nodeNeighbors[16].push_back(14);

    m_nodeNeighbors[17].push_back( 1);
    m_nodeNeighbors[17].push_back(15);
    m_nodeNeighbors[17].push_back( 3);

    test_neighbors();
}

SimplicialMesh2D_Test::~SimplicialMesh2D_Test()
{
}

void
SimplicialMesh2D_Test::test_neighbors()
{
    for ( mculong i=0; i<m_numNodes; ++i )
    {
        for ( size_t j=0; j<m_nodeNeighbors[i].size(); ++j )
        {
            const int neighbor = m_nodeNeighbors[i][j];
            bool found = false;
            for ( size_t k=0; k<m_nodeNeighbors[neighbor].size(); ++k )
            {
                if ( m_nodeNeighbors[neighbor][k] == i )
                {
                    found = true;
                    break;
                }
            }
            if ( ! found )
            {
                printf("Did not find neighbor %d of %d\n", neighbor, (int)i);
                printf("ERROR: list of neigbors is wrong\n");
                fflush(stdout);
            }
        }
    }
}

void
SimplicialMesh2D_Test::setThreshold(const double threshold)
{
    m_threshold           = threshold;
    m_useThreshold        = true;
}

void
SimplicialMesh2D_Test::getSortedListOfVertices(
    const bool            computeSplitTree,
    std::vector< mculong > & sortedListOfVertices)
{
    sortedListOfVertices.resize(m_numNodes);
    if ( computeSplitTree )
    {
        for ( mculong i=0, j=m_numNodes-1; i<m_numNodes; ++i, --j )
        {
            sortedListOfVertices[i] = j;
        }
    }
    else
    {
        for ( mculong i=0; i<m_numNodes; ++i )
        {
            sortedListOfVertices[i] = i;
        }
    }
}

void
SimplicialMesh2D_Test::getNeighborsOfNode(
    const bool            neighborsWithSmallerValue,
    const mculong         nodeIdx,
    std::vector< mculong > & neighbors,
    std::vector< float >   & values) const
{
    neighbors.clear();
    if ( neighborsWithSmallerValue )
    {
        for ( size_t i=0; i<m_nodeNeighbors[nodeIdx].size(); ++i )
            if ( m_nodeValues[nodeIdx] > m_nodeValues[m_nodeNeighbors[nodeIdx][i]] )
                neighbors.push_back(m_nodeNeighbors[nodeIdx][i]);
    }
    else
    {
        for ( size_t i=0; i<m_nodeNeighbors[nodeIdx].size(); ++i )
            if ( m_nodeValues[nodeIdx] < m_nodeValues[m_nodeNeighbors[nodeIdx][i]] )
                neighbors.push_back(m_nodeNeighbors[nodeIdx][i]);
    }
}

const std::vector<float> & 
SimplicialMesh2D_Test::getValues() const
{
    return m_nodeValues;
}

const std::vector<McVec3f> &
SimplicialMesh2D_Test::getCoords() const
{
    return m_nodeCoords;
}

mclong
SimplicialMesh2D_Test::getMeshVertexIdx(const mclong nodeIdx) const
{
    return nodeIdx;
}

mclong
SimplicialMesh2D_Test::getNodeIdx(const mclong vertexIdx) const
{
    return vertexIdx;
}

void
SimplicialMesh2D_Test::setNeighborhood(const int neighborhood)
{
}

int
SimplicialMesh2D_Test::getNeighborhood() const
{
    return 1;
}
