#ifndef HXADJUSTWINDOWLEVELOFAMIRAMESH_H
#define HXADJUSTWINDOWLEVELOFAMIRAMESH_H

#include "api.h"
#include <hxcore/HxCompModule.h>
#include <hxcore/HxPortFilename.h>
#include <hxcore/HxPortFloatTextN.h>
#include <hxcore/HxPortDoIt.h>
#include <hxfield/HxUniformScalarField3.h>


/**
 * @brief The HxAdjustWindowLevelOfAmiraMesh class adjusts window level of input (.am) files.
*/
class HXFILOPODIA_API HxAdjustWindowLevelOfAmiraMesh : public HxCompModule {

    HX_HEADER(HxAdjustWindowLevelOfAmiraMesh);

public:
    void compute();
    void update();

private:
    float windowMin, windowMax;

    HxPortFilename portInputDir;
    HxPortFilename portOutputDir;
    HxPortFloatTextN windowSize;
    HxPortDoIt portDoIt;

    bool isInputDirectoryValid();

    // Function extracts window level from the input (.am) images
    void getWindowMinMax(HxUniformScalarField3* image, float& dataWindowMin, float& dataWindowMax);

    // Function sets window level of the output images (.am) images
    void setWindowMinMax(HxUniformScalarField3* image, float& dataWindowMin, float& dataWindowMax);

    McHandle<HxUniformScalarField3> getSingleFile();
};

#endif // HXADJUSTWINDOWLEVELOFAMIRAMESH_H
