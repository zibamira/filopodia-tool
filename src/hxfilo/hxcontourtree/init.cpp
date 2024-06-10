// AUTOMATICALLY CMAKE-GENERATED FILE.  DO NOT MODIFY.  Place custom code in custominit.h.
void mcExitClass_HxTouchPointGraphBuilder();
void mcInitClass_HxTouchPointGraphBuilder();
void mcExitClass_HxPropagateContours();
void mcInitClass_HxPropagateContours();
void mcExitClass_HxMergeTreeSegmentation();
void mcInitClass_HxMergeTreeSegmentation();
void mcExitClass_HxCreateContourTree();
void mcInitClass_HxCreateContourTree();
void mcExitClass_HxContourTreeSegmentation();
void mcInitClass_HxContourTreeSegmentation();


extern "C"
#ifdef _WIN32
__declspec(dllexport)
#endif
void
amirapackage_hxcontourtree_init()
{
    static bool isInitialized = false;
    if (isInitialized)
      return;

    isInitialized = true;

    mcInitClass_HxTouchPointGraphBuilder();
    mcInitClass_HxPropagateContours();
    mcInitClass_HxMergeTreeSegmentation();
    mcInitClass_HxCreateContourTree();
    mcInitClass_HxContourTreeSegmentation();

}


extern "C"
#ifdef _WIN32
__declspec(dllexport)
#endif
void
amirapackage_hxcontourtree_finish()
{
    static bool isFinished = false;
    if (isFinished)
      return;

    isFinished = true;


    mcExitClass_HxContourTreeSegmentation();
    mcExitClass_HxCreateContourTree();
    mcExitClass_HxMergeTreeSegmentation();
    mcExitClass_HxPropagateContours();
    mcExitClass_HxTouchPointGraphBuilder();
}

#if defined(_WIN32)
#  include <windows.h>


BOOL WINAPI DllMain(
    __in  HINSTANCE hinstDLL,
    __in  DWORD fdwReason,
    __in  LPVOID lpvReserved
    )
{
    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
        amirapackage_hxcontourtree_init();
        break;
    case DLL_PROCESS_DETACH:
        amirapackage_hxcontourtree_finish();
        break;
    default:
        ;
    }
    return true;
}


#endif

#if defined(__GNUC__)
void
__attribute__((constructor)) soconstructor_hxcontourtree()
{
    amirapackage_hxcontourtree_init();
}

void
__attribute__((destructor)) sodestructor_hxcontourtree()
{
    amirapackage_hxcontourtree_finish();
}
#endif
