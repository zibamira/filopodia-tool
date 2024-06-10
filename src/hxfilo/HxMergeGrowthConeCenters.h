#pragma once

#include "api.h"
#include <hxcore/HxCompModule.h>
#include <hxcore/HxPortDoIt.h>
#include <hxcolor/HxPortColormap.h>

class HXFILOPODIA_API HxMergeGrowthConeCenters : public HxCompModule
{
    HX_HEADER(HxMergeGrowthConeCenters);

 public:

    virtual void compute();

    HxPortColormap   portColormap;
    HxPortDoIt       portAction;

  private:
    int mTimeStep;
};
