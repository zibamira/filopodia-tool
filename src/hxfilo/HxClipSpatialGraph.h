#pragma once

#include "api.h"
#include <hxcore/HxCompModule.h>
#include <hxcore/HxPortDoIt.h>


class HXFILOPODIA_API HxClipSpatialGraph : public HxCompModule
{
    HX_HEADER(HxClipSpatialGraph);

 public:

    virtual void compute();

    HxConnection portSurface;
    HxPortDoIt   portAction;
};
