#ifndef HX_FILOPODIA
#define HX_FILOPODIA

#include "api.h"
#include <hxcore/HxCompModule.h>
//#include <hxcore/HxPortToggleList.h>
#include <hxcore/HxConnection.h>
#include <hxcore/HxPortIntTextN.h>
#include <hxcore/HxPortRadioBox.h>
#include <hxcore/HxPortDoIt.h>
#include <mclib/McDim3l.h>

class HxUniformLabelField3;
class HxUniformScalarField3;


class HXFILOPODIA_API HxSplitLabelField : public HxCompModule {

  HX_HEADER(HxSplitLabelField);

  public:

    enum IntensityPolicy {KEEP_INTENSITY=0, SET_TO_BACKGROUND};

    virtual void compute();
    virtual void update();

    HxConnection portImage;
    HxConnection portSeeds;
    HxPortIntTextN portBoundarySize;
    HxPortRadioBox portImageBackground;
    HxPortDoIt portDoIt;

  private:
    IntensityPolicy getIntensityPolicy();
};

#endif
