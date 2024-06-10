#ifndef HX_CREATE_CONTOUR_TREE_H
#define HX_CREATE_CONTOUR_TREE_H

#include <mclib/McVec3.h>
#include <mclib/McVec3i.h>

#include <hxcore/HxCompModule.h>
#include <hxcore/HxPortDoIt.h>
#include <hxcore/HxPortFloatSlider.h>
#include <hxcore/HxPortToggleList.h>
#include <hxcore/HxPortRadioBox.h>

class ContourTree;
class AugmentedContourTree;

class HxCreateContourTree : public HxCompModule
{
    HX_HEADER(HxCreateContourTree);

public:
    HxPortFloatSlider portThreshold;

    HxPortToggleList portOptions;

    HxPortRadioBox portTrees;

    HxPortDoIt portDoIt;

protected:
    void update();
    void compute();

private:
    void computeTrees();
    void computeTrees_Test();
    void createSpatialGraphOfAugmentedTree(const int                    resultId,
                                           const AugmentedContourTree & contourTree,
                                           const std::vector< McVec3f >  & coords,
                                           const std::vector< float >    & values);
    void createSpatialGraphOfTree(const int                    resultId,
                                  const ContourTree          & contourTree,
                                  const std::vector< McVec3f >  & coords,
                                  const std::vector< float >    & values);
    void getCoordsFromGridPos(const std::vector< McVec3i > & gridPos,
                              std::vector< McVec3f >       & coords);
    void getValuesFromGridPos(const std::vector< McVec3i > & gridPos,
                              std::vector< float >         & values);
};

#endif

