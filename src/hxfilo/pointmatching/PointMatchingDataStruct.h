/////////////////////////////////////////////////////////////////
// 
// PointMatchingDataStruct.h
//
// Main Authors: Baum
// 
/////////////////////////////////////////////////////////////////

#ifndef POINT_MATCHING_DATA_STRUCT_H
#define POINT_MATCHING_DATA_STRUCT_H

#include <mclib/McHandable.h>

#include "api.h"

#include <vector>

class POINTMATCHING_API PointMatchingDataStruct : public McHandable {
    
public:
    PointMatchingDataStruct();

    void reset();
    PointMatchingDataStruct * duplicate() const;
    PointMatchingDataStruct & operator=(const PointMatchingDataStruct & pointMatching);

    void setRefPoints(const std::vector<int> & refPoints);
    void setQueryPoints(const std::vector<int> & queryPoints);

    const std::vector<int> & getRefPoints() const;
    const std::vector<int> & getQueryPoints() const;
    
    int getSize() const;

    void setScore(const double score);
    double getScore() const;
    
protected:
    double score;
    std::vector<int> refPoints;
    std::vector<int> queryPoints;
};

#endif
