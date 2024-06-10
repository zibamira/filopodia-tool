
#include <mclib/McVec3.h>
#include <mclib/McMat4.h>

#include "SimplePointRepresentation3d.h"
#include "GreedyPointMatchingAlgorithm.h"
#include "Transformation.h"
#include "PointMatchingDataStruct.h"
#include "Test_GreedyPointMatching.h"

#include <vector>

namespace
{

void getCoords1(std::vector<McVec3f> & coords)
{
    coords.push_back(McVec3f(1.0, 1.0, 0.0));
    coords.push_back(McVec3f(2.0, 1.0, 0.0));
    coords.push_back(McVec3f(3.0, 1.0, 0.0));
    coords.push_back(McVec3f(4.0, 1.0, 0.0));
}

void getCoords2(std::vector<McVec3f> & coords)
{
    coords.push_back(McVec3f(1.0, 2.0, 0.0));
    coords.push_back(McVec3f(2.0, 2.0, 0.0));
    coords.push_back(McVec3f(3.0, 2.0, 0.0));
    coords.push_back(McVec3f(4.0, 2.0, 0.0));
}

void getCoords3(std::vector<McVec3f> & coords)
{
    coords.push_back(McVec3f(2.0, 1.0, 0.0));
    coords.push_back(McVec3f(3.0, 1.0, 0.0));
    coords.push_back(McVec3f(4.0, 1.0, 0.0));
    coords.push_back(McVec3f(5.0, 1.0, 0.0));
}

void getCoords4(std::vector<McVec3f> & coords)
{
    coords.push_back(McVec3f(2.0, 2.0, 0.0));
    coords.push_back(McVec3f(3.0, 2.0, 0.0));
    coords.push_back(McVec3f(4.0, 2.0, 0.0));
    coords.push_back(McVec3f(5.0, 2.0, 0.0));
}

void getCoords5(std::vector<McVec3f> & coords)
{
    coords.push_back(McVec3f(3.0, 2.0, 0.0));
    coords.push_back(McVec3f(4.0, 2.0, 0.0));
    coords.push_back(McVec3f(5.0, 2.0, 0.0));
    coords.push_back(McVec3f(6.0, 2.0, 0.0));
}

void getCoords6(std::vector<McVec3f> & coords)
{
    coords.push_back(McVec3f(1.55, 2.0, 0.0));
    coords.push_back(McVec3f(2.55, 2.0, 0.0));
    coords.push_back(McVec3f(3.55, 2.0, 0.0));
    coords.push_back(McVec3f(4.55, 2.0, 0.0));
}

void getIdentityTransformation(Transformation & transform)
{
    McMat4f transf = McMat4f::IDENTITY;
    transform.setTransformation3d(transf);
}

}

bool Test_GreedyPointMatching::test1()
{
    printf("Test_GreedyPointMatching::test1\n");

    std::vector<McVec3f> coords1;
    getCoords1(coords1);

    std::vector<McVec3f> coords2;
    getCoords2(coords2);

    return test(coords1, coords2);
}

bool Test_GreedyPointMatching::test2()
{
    printf("Test_GreedyPointMatching::test2\n");

    std::vector<McVec3f> coords1;
    getCoords1(coords1);

    std::vector<McVec3f> coords2;
    getCoords3(coords2);

    return test(coords1, coords2);
}

bool Test_GreedyPointMatching::test3()
{
    printf("Test_GreedyPointMatching::test3\n");

    std::vector<McVec3f> coords1;
    getCoords1(coords1);

    std::vector<McVec3f> coords2;
    getCoords4(coords2);

    return test(coords1, coords2);
}

bool Test_GreedyPointMatching::test4()
{
    printf("Test_GreedyPointMatching::test4\n");

    std::vector<McVec3f> coords1;
    getCoords1(coords1);

    std::vector<McVec3f> coords2;
    getCoords5(coords2);

    return test(coords1, coords2);
}

bool Test_GreedyPointMatching::test5()
{
    printf("Test_GreedyPointMatching::test4\n");

    std::vector<McVec3f> coords1;
    getCoords1(coords1);

    std::vector<McVec3f> coords2;
    getCoords6(coords2);

    return test(coords1, coords2);
}

bool Test_GreedyPointMatching::test(const std::vector< McVec3f > & coords1,
				   const std::vector< McVec3f > & coords2)
{
    SimplePointRepresentation3d pointRep1(4.0, 0.0);
    pointRep1.setCoords(coords1);

    SimplePointRepresentation3d pointRep2(4.0, 0.0);
    pointRep2.setCoords(coords2);
    
    Transformation pointRep2Transform;
    getIdentityTransformation(pointRep2Transform);

    PointMatchingDataStruct pointMatching;
    GreedyPointMatchingAlgorithm greedyAlgorithm;
    greedyAlgorithm.computePointMatching(&pointRep1, 
					 &pointRep2,
					 &pointRep2Transform,
					 &pointMatching);

    const std::vector<int> & refPoints = pointMatching.getRefPoints();
    const std::vector<int> & queryPoints = pointMatching.getQueryPoints();

    for ( size_t i=0; i<refPoints.size(); ++i )
    {
        printf("(%d, %d)", refPoints[i], queryPoints[i]);
    }
    printf("\n");
    fflush(stdout);

    return true;
}
