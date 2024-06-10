#ifndef HX_FILOPODIA
#define HX_FILOPODIA

#include "api.h"
#include <hxcore/HxCompModule.h>
#include <hxfield/HxUniformLabelField3.h>
#include <hxcore/HxPortIntTextN.h>
#include <hxcore/HxPortDoIt.h>


class HXFILOPODIA_API HxLabelPSFCorrection : public HxCompModule {

  HX_HEADER(HxLabelPSFCorrection);

  public:

    virtual void compute();

    HxConnection portImage;
    HxPortIntTextN portNumIterations;
    HxPortDoIt portDoIt;

};

#endif
