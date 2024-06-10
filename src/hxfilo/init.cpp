// AUTOMATICALLY CMAKE-GENERATED FILE.  DO NOT MODIFY.  Place custom code in custominit.h.
void mcExitClass_HxTraceDijkstraMap();
void mcInitClass_HxTraceDijkstraMap();
void mcExitClass_HxSplitLabelField();
void mcInitClass_HxSplitLabelField();
void mcExitClass_HxShortestPathToPointMap();
void mcInitClass_HxShortestPathToPointMap();
void mcExitClass_HxSetFilopodiaBaseNodes();
void mcInitClass_HxSetFilopodiaBaseNodes();
void mcExitClass_HxRetraceFilopodia();
void mcInitClass_HxRetraceFilopodia();
void mcExitClass_HxNodeAtRegionMaximum();
void mcInitClass_HxNodeAtRegionMaximum();
void mcExitClass_HxMergeSpatialGraphs();
void mcInitClass_HxMergeSpatialGraphs();
void mcExitClass_HxMergeGrowthConeCenters();
void mcInitClass_HxMergeGrowthConeCenters();
void mcExitClass_HxLandweberDeconvolution();
void mcInitClass_HxLandweberDeconvolution();
void mcExitClass_HxLabelPSFCorrection();
void mcInitClass_HxLabelPSFCorrection();
void mcExitClass_HxLabelInsideOutside();
void mcInitClass_HxLabelInsideOutside();
void mcExitClass_HxGrowthConeTrack();
void mcInitClass_HxGrowthConeTrack();
void mcExitClass_HxGrowthConeFieldTrack();
void mcInitClass_HxGrowthConeFieldTrack();
void mcExitClass_HxFilopodiaTrack();
void mcInitClass_HxFilopodiaTrack();
void mcExitClass_HxFilopodiaTEASAR();
void mcInitClass_HxFilopodiaTEASAR();
void mcExitClass_HxFilopodiaStats();
void mcInitClass_HxFilopodiaStats();
void mcExitClass_HxFieldCorrelation();
void mcInitClass_HxFieldCorrelation();
void mcExitClass_HxExtractSlicesFrom3DImage();
void mcInitClass_HxExtractSlicesFrom3DImage();
void mcExitClass_HxConvertTif2AmiraMesh();
void mcInitClass_HxConvertTif2AmiraMesh();
void mcExitClass_HxConvertHoc2AmiraMesh();
void mcInitClass_HxConvertHoc2AmiraMesh();
void mcExitClass_HxCompareFilopodiaSpatialGraphs();
void mcInitClass_HxCompareFilopodiaSpatialGraphs();
void mcExitClass_HxClipSpatialGraphByThickness();
void mcInitClass_HxClipSpatialGraphByThickness();
void mcExitClass_HxClipSpatialGraph();
void mcInitClass_HxClipSpatialGraph();
void mcExitClass_HxAdjustWindowLevelOfAmiraMesh();
void mcInitClass_HxAdjustWindowLevelOfAmiraMesh();


extern "C"
#ifdef _WIN32
__declspec(dllexport)
#endif
void
amirapackage_hxfilo_init()
{
    static bool isInitialized = false;
    if (isInitialized)
      return;

    isInitialized = true;

    mcInitClass_HxTraceDijkstraMap();
    mcInitClass_HxSplitLabelField();
    mcInitClass_HxShortestPathToPointMap();
    mcInitClass_HxSetFilopodiaBaseNodes();
    mcInitClass_HxRetraceFilopodia();
    mcInitClass_HxNodeAtRegionMaximum();
    mcInitClass_HxMergeSpatialGraphs();
    mcInitClass_HxMergeGrowthConeCenters();
    mcInitClass_HxLandweberDeconvolution();
    mcInitClass_HxLabelPSFCorrection();
    mcInitClass_HxLabelInsideOutside();
    mcInitClass_HxGrowthConeTrack();
    mcInitClass_HxGrowthConeFieldTrack();
    mcInitClass_HxFilopodiaTrack();
    mcInitClass_HxFilopodiaTEASAR();
    mcInitClass_HxFilopodiaStats();
    mcInitClass_HxFieldCorrelation();
    mcInitClass_HxExtractSlicesFrom3DImage();
    mcInitClass_HxConvertTif2AmiraMesh();
    mcInitClass_HxConvertHoc2AmiraMesh();
    mcInitClass_HxCompareFilopodiaSpatialGraphs();
    mcInitClass_HxClipSpatialGraphByThickness();
    mcInitClass_HxClipSpatialGraph();
    mcInitClass_HxAdjustWindowLevelOfAmiraMesh();

}


extern "C"
#ifdef _WIN32
__declspec(dllexport)
#endif
void
amirapackage_hxfilo_finish()
{
    static bool isFinished = false;
    if (isFinished)
      return;

    isFinished = true;


    mcExitClass_HxAdjustWindowLevelOfAmiraMesh();
    mcExitClass_HxClipSpatialGraph();
    mcExitClass_HxClipSpatialGraphByThickness();
    mcExitClass_HxCompareFilopodiaSpatialGraphs();
    mcExitClass_HxConvertHoc2AmiraMesh();
    mcExitClass_HxConvertTif2AmiraMesh();
    mcExitClass_HxExtractSlicesFrom3DImage();
    mcExitClass_HxFieldCorrelation();
    mcExitClass_HxFilopodiaStats();
    mcExitClass_HxFilopodiaTEASAR();
    mcExitClass_HxFilopodiaTrack();
    mcExitClass_HxGrowthConeFieldTrack();
    mcExitClass_HxGrowthConeTrack();
    mcExitClass_HxLabelInsideOutside();
    mcExitClass_HxLabelPSFCorrection();
    mcExitClass_HxLandweberDeconvolution();
    mcExitClass_HxMergeGrowthConeCenters();
    mcExitClass_HxMergeSpatialGraphs();
    mcExitClass_HxNodeAtRegionMaximum();
    mcExitClass_HxRetraceFilopodia();
    mcExitClass_HxSetFilopodiaBaseNodes();
    mcExitClass_HxShortestPathToPointMap();
    mcExitClass_HxSplitLabelField();
    mcExitClass_HxTraceDijkstraMap();
}

#if defined(_WIN32)
#  include <windows.h>
#ifdef _OPENMP
    #include <omp.h>
#endif

BOOL WINAPI DllMain(
    __in  HINSTANCE hinstDLL,
    __in  DWORD fdwReason,
    __in  LPVOID lpvReserved
    )
{
    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
        amirapackage_hxfilo_init();
        break;
    case DLL_PROCESS_DETACH:
        amirapackage_hxfilo_finish();
        break;
    default:
        ;
    }
    return true;
}


#endif

#if defined(__GNUC__)
void
__attribute__((constructor)) soconstructor_hxfilo()
{
    amirapackage_hxfilo_init();
}

void
__attribute__((destructor)) sodestructor_hxfilo()
{
    amirapackage_hxfilo_finish();
}
#endif
