#pragma once

#include "api.h"
#include "FilopodiaFunctions.h"
#include "NodeTracks.h"
#include <hxcore/HxCompModule.h>
#include <hxcore/HxPortDoIt.h>
#include <hxcore/HxPortFloatTextN.h>
#include <hxneuroneditor/internal/HxNeuronEditorSubApp.h>


class HxSpatialGraph;


class HXFILOPODIA_API HxFilopodiaTrack : public HxCompModule
{
    HX_HEADER(HxFilopodiaTrack);

 public:

    virtual void compute();

    HxPortFloatTextN portDistanceThreshold;
    HxPortToggleList portMergeManualNodeMatch;
    HxPortDoIt       portAction;

    static HxNeuronEditorSubApp::SpatialGraphChanges trackFilopodia(HxSpatialGraph* graph,
                               const float distThreshold,
                               const bool mergeManualNodeTracks);

    static TrackingModel createTrackingModel(const HxSpatialGraph* graph);

    static void matchAcrossSingleTimeStep(TrackingModel& model,
                                   const HxSpatialGraph* graph,
                                   const float distThreshold,
                                   const bool mergeManualNodeTracks);

    static void processManualNodeMatch(const HxSpatialGraph* graph,
                                TrackingModel& model);

};
