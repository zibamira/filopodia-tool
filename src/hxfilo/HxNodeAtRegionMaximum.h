#pragma once

#include "api.h"
#include <hxcore/HxCompModule.h>
#include <hxcore/HxPortDoIt.h>
#include <hxcore/HxPortRadioBox.h>
#include <hxcore/HxConnection.h>
#include <hxcore/HxPortToggleList.h>
#include <hxcore/HxPortFloatTextN.h>

class HXFILOPODIA_API HxNodeAtRegionMaximum : public HxCompModule
{
    HX_HEADER(HxNodeAtRegionMaximum);

 public:

    virtual void compute();
    void update();

    HxPortRadioBox   portGlobalOrRegion;
    HxConnection     portRegions;
    HxPortToggleList portRemoveBoundaryNodes;
    HxPortDoIt       portAction;
};
