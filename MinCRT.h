// Stubs to use msvcrt.dll instead of vcruntimeXXX.dll and msvcrtXX.dll

#define _NO_CRT_STDIO_INLINE
//extern "C" { _declspec(selectany) int _fltused; };

#define WIN32_LEAN_AND_MEAN        // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>

// -----------------------------------------------------------------------
// CRT stubs

#include <io.h>
#include <stdio.h>
#include <atlbase.h>

extern "C" void __std_terminate() { exit(E_UNEXPECTED); };

#if _MSC_VER >= 1400
#undef stdin
#undef stdout
#undef stderr

typedef struct {
    char* _ptr;
    int   _cnt;
    char* _base;
    int   _flag;
    int   _file;
    int   _ungotchar;
    int   _bufsiz;
    char* _name_to_remove;
} FILE_MSVCRT;

extern "C" FILE_MSVCRT* __cdecl __iob_func();
#define stdin   (FILE*)(__iob_func() + 0)
#define stdout  (FILE*)(__iob_func() + 1)
#define stderr  (FILE*)(__iob_func() + 2)
#endif

// -----------------------------------------------------------------------
// ATL stubs

namespace ATL {
    CAtlBaseModule _AtlBaseModule;
};

inline CAtlBaseModule::CAtlBaseModule() 
{
    m_hInst = 0;
}
inline CAtlBaseModule::~CAtlBaseModule()
{
}

// -----------------------------------------------------------------------
// Entry point wrappers

#include <ShellAPI.h>

int wmain(int argc, wchar_t** argv);

extern "C" int wmainCRTStartup()
{
    LPWSTR *szArglist;
    int nArgs;

    szArglist = ::CommandLineToArgvW(::GetCommandLineW(), &nArgs);
    if (NULL == szArglist) {
        nArgs = 0;
    }

    return wmain(nArgs, szArglist);
}