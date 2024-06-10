#ifndef HXFILOPODIATEASAR_H
#define HXFILOPODIATEASAR_H

#include "api.h"
#include <hxcore/HxCompModule.h>
#include <hxcore/HxPortIntTextN.h>
#include <hxcore/HxPortRadioBox.h>
#include <hxcore/HxPortToggleList.h>
#include <hxcore/HxPortDoIt.h>
#include <mclib/internal/McFHeap.h>
#include <mclib/McVec3i.h>
#include <mclib/McDim3l.h>

class HxData;
class HxUniformScalarField3;
class HxUniformLabelField3;


class HXFILOPODIA_API HxShortestPathToPointMap : public HxCompModule
{
    HX_HEADER(HxShortestPathToPointMap);

  public:

    enum Output {
        OUTPUT_DISTANCEMAP,
        OUTPUT_PRIORMAP
    };

    ///

    HxConnection     portGraph;
    HxPortRadioBox   portCoordsOrGraph;
    HxPortFloatTextN portCoords;
    HxPortFloatTextN portIntensityWeight;
    HxPortFloatTextN portIntensityPower;
    HxPortIntTextN   portTopBrightness;
    HxPortToggleList portOutput;

    /// Start the Algorithm.
    HxPortDoIt portDoIt;

    ///
    void compute();
    void update();

    static void computeDijkstraMap(const HxUniformScalarField3 *image,
                                   const McVec3f rootPoint,
                                   const McDim3l startVoxel,
                                   const McDim3l endVoxel,
                                   const float intensityWeight,
                                   const int topBrightness,
                                   HxUniformLabelField3* dijkstraMap,
                                   HxUniformScalarField3* distanceMap = 0,
                                   const float intensityPower = 1.0);

  protected:

  private:
};


class HXFILOPODIA_API McFHeapElementDijkstra : public McFHeapElement {
public:
    float distance;
    McVec3i coordinates;

    bool operator<(const McFHeapElementDijkstra& other) {
        return distance < other.distance;
    }
};

#endif
