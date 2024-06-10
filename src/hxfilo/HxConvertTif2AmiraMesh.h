#ifndef HXTIF2AMIRAMESHBINARY_H
#define HXTIF2AMIRAMESHBINARY_H

#include "api.h"
#include <hxcore/HxCompModule.h>
#include <hxcore/HxPortFilename.h>
#include <hxcore/HxPortFloatTextN.h>
#include <hxcore/HxPortDoIt.h>
#include <hxcore/HxPortToggleList.h>
#include <hxcore/HxPortOnOff.h>
#include <hxfield/HxUniformScalarField3.h>

/**
    @brief The HxConvertTif2AmiraMesh class creates an object in the object pool that is used
           to convert .tif files to .am files that can be processed in Amira. Module also
           includes functionality of adjusting window lavel of the images you want to convert.
*/

class HXFILOPODIA_API HxConvertTif2AmiraMesh : public HxCompModule
{
    HX_HEADER(HxConvertTif2AmiraMesh);

public:
    void compute();
    void update();

private:
    float windowMin, windowMax;

    HxPortFilename portInputDir;
    HxPortFilename portOutputDir;
    HxPortFloatTextN portSize;
    HxPortOnOff adjustWindowSize;
    HxPortFloatTextN windowSize;
    HxPortDoIt portDoIt;

    // Function extracts window level from the input (.tif) images
    void getWindowMinMax(HxUniformScalarField3* image, float& dataWindowMin, float& dataWindowMax);

    // Function sets window level of the output images (.am) images
    void setWindowMinMax(HxUniformScalarField3* image, float& dataWindowMin, float& dataWindowMax);

    bool isInputDirectoryValid();

    McHandle<HxUniformScalarField3> getSingleFile();
};

#endif
