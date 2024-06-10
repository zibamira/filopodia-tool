#pragma once

#include "api.h"
#include <hxcore/HxCompModule.h>
#include <hxcore/HxPortFilename.h>
#include <hxcore/HxPortFloatTextN.h>
#include <hxcore/HxPortDoIt.h>

class HXFILOPODIA_API HxSetFilopodiaBaseNodes : public HxCompModule
{
    HX_HEADER(HxSetFilopodiaBaseNodes);

 public:

    virtual void compute();
    HxPortFilename portInputDir;
    HxPortFloatTextN portThreshold;
    HxPortDoIt   portAction;
};
