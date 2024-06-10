#ifndef HXFIELDCORRELATION_H
#define HXFIELDCORRELATION_H

#include <hxcore/HxCompModule.h>
#include <hxcore/HxPortDoIt.h>
#include <hxcore/HxPortIntTextN.h>
#include "api.h"


class HXFILOPODIA_API HxFieldCorrelation : public HxCompModule
{
    HX_HEADER(HxFieldCorrelation);

    public:

        virtual void compute();

        HxConnection portTemplate;
        HxPortIntTextN portStart;
        HxPortIntTextN portSize;
        HxPortDoIt   portAction;
};

#endif
