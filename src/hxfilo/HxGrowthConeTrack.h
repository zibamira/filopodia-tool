#pragma once

#include "api.h"
#include <hxcore/HxCompModule.h>
#include <hxcore/HxPortDoIt.h>
#include <hxcore/HxPortFloatTextN.h>

class HXFILOPODIA_API HxGrowthConeTrack : public HxCompModule
{
    HX_HEADER(HxGrowthConeTrack);

 public:

    virtual void compute();

    HxPortFloatTextN portDistanceThreshold;
    HxPortDoIt       portAction;
};
