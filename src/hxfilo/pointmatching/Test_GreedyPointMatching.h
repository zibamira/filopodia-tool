#ifndef TEST_GREEDY_POINT_MATCHING_H
#define TEST_GREEDY_POINT_MATCHING_H

#include <mclib/McVec3.h>

#include <vector>

class Test_GreedyPointMatching
{
public:
    static bool test1();
    static bool test2();
    static bool test3();
    static bool test4();
    static bool test5();

    static bool test(const std::vector< McVec3f > & coords1,
		     const std::vector< McVec3f > & coords2);
};

#endif
