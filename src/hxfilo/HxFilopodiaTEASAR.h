#ifndef HXFILOPODIATEASAR_H
#define HXFILOPODIATEASAR_H

#include "api.h"
#include <hxcore/HxCompModule.h>
#include <hxcore/HxPortToggleList.h>
#include <hxcore/HxPortIntTextN.h>
#include <hxcore/HxPortDoIt.h>

class HxData;
class SpDistanceMap;

class HXFILOPODIA_API HxFilopodiaTEASAR : public HxCompModule
{
    HX_HEADER(HxFilopodiaTEASAR);

  public:

    HxConnection portCenters;

    HxConnection portLoops;

    HxPortToggleList portMaps;

    HxPortFloatTextN portTubeParams;

    HxPortFloatTextN portFakeVoxelSizeZ;

    /// Start the Algorithm.
    HxPortDoIt portDoIt;

    void update();

    void compute();

  protected:

    /**
     * Called when adding a result object to the pool.
     */
    static void addResultToPool(HxData* result);
};

#endif
