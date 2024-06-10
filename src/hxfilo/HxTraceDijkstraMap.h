#ifndef HxTraceDijkstraMap_H
#define HxTraceDijkstraMap_H

#include "api.h"
#include <hxcore/HxCompModule.h>
#include <hxcore/HxPortDoIt.h>

class HXFILOPODIA_API HxTraceDijkstraMap : public HxCompModule
{
    HX_HEADER(HxTraceDijkstraMap);

  public:

    HxConnection     portDijkstra;
    HxPortDoIt       portAction;

    void compute();


};

#endif
