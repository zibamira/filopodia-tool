#ifndef HXHOC2AMIRAMESHBINARY_H
#define HXHOC2AMIRAMESHBINARY_H

#include "api.h"
#include <hxcore/HxCompModule.h>
#include <hxcore/HxPortFilename.h>
#include <hxcore/HxPortDoIt.h>
#include <hxcore/HxPortToggleList.h>


class HXFILOPODIA_API HxConvertHoc2AmiraMesh : public HxCompModule {

    HX_HEADER(HxConvertHoc2AmiraMesh);

public:
    void compute();
    void update();

private:
    HxPortFilename portInputDir;
    HxPortFilename portOutputDir;
    HxPortDoIt portDoIt;

};

#endif
