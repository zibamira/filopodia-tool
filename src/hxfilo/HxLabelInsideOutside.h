#pragma once

#include "api.h"
#include <hxcore/HxCompModule.h>
#include <hxcore/HxPortDoIt.h>

class HXFILOPODIA_API HxLabelInsideOutside : public HxCompModule
{
    HX_HEADER(HxLabelInsideOutside);

 public:

    virtual void compute();
    HxPortDoIt   portAction;
};
