// AUTOMATICALLY CMAKE-GENERATED FILE.  DO NOT MODIFY.  Place custom code in custominit.h.
void mcExitClass_HxTest_PointMatching();
void mcInitClass_HxTest_PointMatching();


extern "C"
#ifdef _WIN32
__declspec(dllexport)
#endif
void
amirapackage_pointmatching_init()
{
    static bool isInitialized = false;
    if (isInitialized)
      return;

    isInitialized = true;

    mcInitClass_HxTest_PointMatching();

}


extern "C"
#ifdef _WIN32
__declspec(dllexport)
#endif
void
amirapackage_pointmatching_finish()
{
    static bool isFinished = false;
    if (isFinished)
      return;

    isFinished = true;


    mcExitClass_HxTest_PointMatching();
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
        amirapackage_pointmatching_init();
        break;
    case DLL_PROCESS_DETACH:
        amirapackage_pointmatching_finish();
        break;
    default:
        ;
    }
    return true;
}


#endif

#if defined(__GNUC__)
void
__attribute__((constructor)) soconstructor_pointmatching()
{
    amirapackage_pointmatching_init();
}

void
__attribute__((destructor)) sodestructor_pointmatching()
{
    amirapackage_pointmatching_finish();
}
#endif
