#pragma once

#include "api.h"
#include <hxspatialgraph/internal/HxSpatialGraphIO.h>
#include <hxspreadsheet/internal/HxSpreadSheet.h>
#include <hxcore/HxCompModule.h>
#include <hxcore/HxPortDoIt.h>
#include <hxcore/HxPortToggleList.h>
#include <hxcore/HxPortFloatTextN.h>

class HXFILOPODIA_API HxFilopodiaStats : public HxCompModule
{
    HX_HEADER(HxFilopodiaStats);

    public:
    virtual void compute();

    enum Output {
        OUTPUT_SPREADSHEET_FILAMENT,
        OUTPUT_SPREADSHEET_FILOPODIA,
        OUTPUT_SPREADSHEET_LENGTH
    };

    HxPortToggleList portOutput;
    HxPortFloatTextN portFilter;
    HxPortDoIt       portAction;

    static void createFilamentTab(const HxSpatialGraph* graph,
                                  HxSpreadSheet* ss);
    static void createLengthTab(const HxSpatialGraph* graph,
                                HxSpreadSheet* ss,
                                const HxSpreadSheet* ssFilament);
    static void createFilopodiaTab(const HxSpatialGraph* graph,
                                   HxSpreadSheet* ss,
                                   const HxSpreadSheet* ssFilament,
                                   const float filter);

};
