#pragma once

#include "api.h"
#include <hxcore/HxCompModule.h>
#include <hxcore/HxPortDoIt.h>

class HXFILOPODIA_API HxClipSpatialGraphByThickness : public HxCompModule
{
    HX_HEADER(HxClipSpatialGraphByThickness);

 public:

    virtual void compute();
    HxPortDoIt   portAction;
};
