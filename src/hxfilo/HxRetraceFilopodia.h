#ifndef HXRETRACEFILOPODIA_H
#define HXRETRACEFILOPODIA_H

#include "api.h"
#include <hxcore/HxCompModule.h>
#include <hxcore/HxPortDoIt.h>
#include <hxcore/HxPortFilename.h>
#include <hxcore/HxPortIntTextN.h>
#include <hxcore/HxPortToggleList.h>

class HXFILOPODIA_API HxRetraceFilopodia : public HxCompModule
{
    HX_HEADER(HxRetraceFilopodia);

  public:

    HxPortFilename portImageDir;
    HxPortFloatTextN portIntensityWeight;
    HxPortIntTextN portTopBrightness;
    HxPortToggleList portSmooth;
    HxPortIntTextN portSurroundWidthInVoxels;
    HxPortDoIt portAction;

    void compute();
};

#endif
