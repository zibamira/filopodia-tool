#pragma once

#include "api.h"
#include <hxcore/HxCompModule.h>
#include <hxcore/HxPortDoIt.h>
#include <hxcore/HxPortFilename.h>

class HXFILOPODIA_API HxMergeSpatialGraphs : public HxCompModule
{
    HX_HEADER(HxMergeSpatialGraphs);

 public:

    virtual void compute();

    HxPortFilename   portInputDir;
    HxPortDoIt       portAction;
};
