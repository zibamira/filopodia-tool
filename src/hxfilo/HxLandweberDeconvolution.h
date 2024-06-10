#ifndef HXLANDWEBERDECONVOLUTION_H
#define HXLANDWEBERDECONVOLUTION_H


#include "api.h"
#include <hxcore/HxCompModule.h>
#include <hxcore/HxPortDoIt.h>
#include <hxcore/HxPortIntTextN.h>


class HXFILOPODIA_API HxLandweberDeconvolution : public HxCompModule
{
    HX_HEADER(HxLandweberDeconvolution);

    public:

        virtual void compute();

        HxConnection portTemplate;
        HxPortIntTextN portIterations;
        HxPortDoIt   portAction;
};

#endif
