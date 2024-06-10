#ifndef HXEXTRACTSLICESFROM3DIMAGE_H
#define HXEXTRACTSLICESFROM3DIMAGE_H

#include "api.h"
#include <hxcore/HxCompModule.h>
#include <hxcore/HxPortIntTextN.h>
#include <hxcore/HxPortDoIt.h>
#include <mclib/McDim3l.h>

class HXFILOPODIA_API HxExtractSlicesFrom3DImage : public HxCompModule {

    HX_HEADER(HxExtractSlicesFrom3DImage);

public:
    void compute();

private:
    HxPortIntTextN z_step;
    HxPortDoIt portAction;
};

#endif // HXEXTRACTSLICESFROM3DIMAGE_H
