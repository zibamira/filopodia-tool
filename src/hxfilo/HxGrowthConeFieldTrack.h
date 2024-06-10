#pragma once

#include "api.h"
#include <hxcore/HxCompModule.h>
#include <hxcore/HxPortDoIt.h>
#include <hxcore/HxPortFilename.h>
#include <hxcore/HxPortFloatTextN.h>

class HXFILOPODIA_API HxGrowthConeFieldTrack : public HxCompModule
{
    HX_HEADER(HxGrowthConeFieldTrack);

 public:

    virtual void compute();

    HxPortFilename   portInputDir;
    HxPortFilename   portOutputDir;
    HxPortFilename   portOutputGraph;
    HxPortFloatTextN portMinOverlapFraction;
    HxPortDoIt       portAction;
};
