// -----------------------------------------------------------------------
// (C) 2021, Stanislav Povolotsky (stas.dev[at]povolotsky.info)
// -----------------------------------------------------------------------
#ifdef MIN_CRT
#include "MinCRT.h"
#endif
#include <atlbase.h>
#include <windows.h>
#include <tlhelp32.h>
#include <tchar.h>
#include <io.h>
#include <fcntl.h>

#define UTIL_NAME	L"SignalAnyEvent"

void ShowHelp()
{
    fwprintf(stdout,
		UTIL_NAME L" utility can set/pulse/reset event object by name or by handle value (for unnamed events)\n"
		L"https://github.com/Stanislav-Povolotsky/SignalAnyEvent" "\n"
        L"\n"
        L"Format: " UTIL_NAME " /n <event-name> [/global|/local] [/pulse] [/reset]\n"
        L"Format: " UTIL_NAME " /p <pid> /h <handle-value> [/pulse] [/reset]\n"
        L"Format: " UTIL_NAME " /pn <process-name> /h <handle-value> [/pulse] [/reset]\n"
        L"\n"
        L"Example: " UTIL_NAME " /n MyEvent\n"
        L"Example: " UTIL_NAME " /p 234 /h 0x290 /pulse\n"
        L"Example: " UTIL_NAME " /pn svchost.exe /h 1344\n"
		L"\n");
	exit(1);
}

enum class EWorkMode
{
    UNDEFINED,
    EVENT_NAME,
    PROCESS_ID,
    PROCESS_NAME,
};

struct SUtilOptions
{
    wchar_t sEmpty[1] = {};
    EWorkMode eMode = EWorkMode::UNDEFINED;
    bool bOptLocal = false;
    bool bOptGlobal = false;
    bool bOptPulse = false;
    bool bOptReset = false;
    wchar_t* pEventName = sEmpty;
    wchar_t* pProcessName = sEmpty;
    wchar_t* pProcessID = sEmpty;
    wchar_t* pHandleValue = sEmpty;

    bool Parse(int argc, wchar_t** argv);
};

bool SUtilOptions::Parse(int argc, wchar_t** argv)
{
    for (int i = 0; i < argc; ++i)
    {
        auto pn = argv[i];
        auto pv = ((i + 1) < argc) ? argv[i + 1] : sEmpty;
        if (!_wcsicmp(pn, L"/n") || !_wcsicmp(pn, L"/name"))
        {
            eMode = EWorkMode::EVENT_NAME;
            pEventName = pv;
            ++i;
        }
        else if (!_wcsicmp(pn, L"/p") || !_wcsicmp(pn, L"/pid"))
        {
            eMode = EWorkMode::PROCESS_ID;
            pProcessID = pv;
            ++i;
        }
        else if (!_wcsicmp(pn, L"/pn") || !_wcsicmp(pn, L"/process_name"))
        {
            eMode = EWorkMode::PROCESS_NAME;
            pProcessName = pv;
            ++i;
        }
        else if (!_wcsicmp(pn, L"/h") || !_wcsicmp(pn, L"/h"))
        {
            pHandleValue = pv;
            ++i;
        }
        else if (!_wcsicmp(pn, L"/global"))
        {
            bOptGlobal = true;
            bOptLocal = false;
        }
        else if (!_wcsicmp(pn, L"/local"))
        {
            bOptLocal = true;
            bOptGlobal = false;
        }
        else if (!_wcsicmp(pn, L"/pulse"))
        {
            bOptPulse = true;
        }
        else if (!_wcsicmp(pn, L"/reset"))
        {
            bOptReset = true;
        }
        else if (!_wcsicmp(pn, L"/?") || !_wcsicmp(pn, L"--help"))
        {
            ShowHelp();
        }
        else
        {
			fwprintf(stderr, L"Unsupported option '%s'\n", pn);
            return false;
        }
    }
    return true;
}

LPCWSTR GetErrorCodeDescription(DWORD dwError, LPWSTR buff, DWORD buff_size)
{
    size_t size = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, dwError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buff, buff_size ? buff_size - 1 : 0, NULL);
    while (size && (buff[size - 1] == '\r' || buff[size - 1] == '\n')) {
        --size;
    }
    if (size) {
        buff[size] = 0;
    }
    return size ? buff : L"";
}

bool EnableDebugPrivileges()
{
    CHandle hToken;
    TOKEN_PRIVILEGES tokenPrivileges;
    LUID luidDebug;
    wchar_t buff[1024];

    if (::OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken.m_h) != FALSE) {
        if (::LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luidDebug) != FALSE) {
            tokenPrivileges.PrivilegeCount = 1;
            tokenPrivileges.Privileges[0].Luid = luidDebug;
            tokenPrivileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
            if (::AdjustTokenPrivileges(hToken, FALSE, &tokenPrivileges, 0, NULL, NULL)) {
                return true;
            }
        }
    }
    DWORD dwErrorCode = ::GetLastError();
    fwprintf(stderr, L"Warning: Unable to enable debug privilege: %u %s\n", dwErrorCode,
        GetErrorCodeDescription(dwErrorCode, buff, _countof(buff)));
    return false;
}

DWORD FindProcess(const wchar_t* sProcessNameOrFullPath)
{
    HANDLE hTmp;
    CHandle hProcessSnap;
    PROCESSENTRY32 pe32;
    const wchar_t* pProcessNameOnly = wcsrchr(const_cast<wchar_t*>(sProcessNameOrFullPath), L'\\');
    bool bHaveSlashesInSProcessName = pProcessNameOnly != NULL;
    if (pProcessNameOnly) ++pProcessNameOnly;
    else pProcessNameOnly = sProcessNameOrFullPath;

    hTmp = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hTmp == INVALID_HANDLE_VALUE) {
        return(FALSE);
    }
    hProcessSnap.Attach(hTmp);

    pe32.dwSize = sizeof(PROCESSENTRY32);
    if (!Process32First(hProcessSnap, &pe32)) {
        return(FALSE);
    }
    do
    {
        auto sCurProcessName = wcsrchr(pe32.szExeFile, L'\\');
        if (!sCurProcessName) sCurProcessName = pe32.szExeFile;
        else ++sCurProcessName;
        if ((bHaveSlashesInSProcessName && !_wcsicmp(sProcessNameOrFullPath, pe32.szExeFile)) ||
            (!bHaveSlashesInSProcessName && sCurProcessName && !_wcsicmp(sProcessNameOrFullPath, sCurProcessName)))
        {
            return pe32.th32ProcessID;
        }

        // Process32First, Process32Next gives us only name of the process. 
        // To get full path, we can use Module32First, Module32Next
        if(bHaveSlashesInSProcessName && pe32.th32ProcessID && !_wcsicmp(pProcessNameOnly, sCurProcessName))
        {
            MODULEENTRY32 entry;
            CHandle hThreadSnap;
            hTmp = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pe32.th32ProcessID);
            if (hTmp != INVALID_HANDLE_VALUE) 
            {
                hThreadSnap.Attach(hTmp);
                entry.dwSize = sizeof(entry);
                if (Module32First(hThreadSnap, &entry))
                {
                    do
                    {
                        if (!_wcsicmp(sCurProcessName, entry.szModule) &&
                            !_wcsicmp(sProcessNameOrFullPath, entry.szExePath))
                        {
                            return pe32.th32ProcessID;
                        }
                        entry.dwSize = sizeof(entry);
                    } while (Module32Next(hThreadSnap, &entry));
                }
            }
        }
        pe32.dwSize = sizeof(PROCESSENTRY32);
    } while (Process32Next(hProcessSnap, &pe32));

    return FALSE;
}

DWORD SignalAnyEvent(SUtilOptions& options)
{
    HANDLE hTmp;
    wchar_t buff[1024];
    ULONG dwPID = 0, dwHandleValue = 0;
    CHandle hEvt;
    DWORD dwError = 0;
    wchar_t* p_end;
    const wchar_t* sFailType = L"Unable to open event";

    switch (options.eMode)
    {
    case EWorkMode::EVENT_NAME:
        buff[0] = 0;
        if (options.bOptGlobal) wcscat_s(buff, _countof(buff), L"Global\\");
        else if (options.bOptLocal) wcscat_s(buff, _countof(buff), L"Local\\");
        wcscat_s(buff, _countof(buff), options.pEventName);
        hTmp = ::OpenEventW(EVENT_MODIFY_STATE, FALSE, buff);
        if (hTmp) {
            hEvt.Attach(hTmp);
        }
        else {
            dwError = ::GetLastError();
            sFailType = L"Unable to open an event";
            break;
        }
        break;
    case EWorkMode::PROCESS_ID:
        if (!options.pHandleValue) {
            dwError = ERROR_INVALID_PARAMETER;
            sFailType = L"Handle was not specified";
            break;
        }
        p_end = options.pProcessID;
        dwPID = options.pProcessID ? wcstoul(options.pProcessID, &p_end, 0) : 0;
        if (p_end == options.pProcessID || *p_end) {
            dwError = ERROR_INVALID_PARAMETER;
            sFailType = L"Unable to parse PID value";
            break;
        }
        dwHandleValue = wcstoul(options.pHandleValue, &p_end, 0);
        if (p_end == options.pHandleValue || *p_end || !dwHandleValue) {
            dwError = ERROR_INVALID_PARAMETER;
            sFailType = L"Unable to parse handle value";
            break;
        }
        break;
    case EWorkMode::PROCESS_NAME:
        if (!options.pHandleValue) {
            dwError = ERROR_INVALID_PARAMETER;
            sFailType = L"Handle was not specified";
            break;
        }
        dwHandleValue = wcstoul(options.pHandleValue, &p_end, 0);
        if (p_end == options.pHandleValue || *p_end || !dwHandleValue) {
            dwError = ERROR_INVALID_PARAMETER;
            sFailType = L"Unable to parse handle value";
            break;
        }
        dwPID = FindProcess(options.pProcessName);
        if (!dwPID) {
            sFailType = L"Unable to find process";
            dwError = ERROR_NOT_FOUND;
            break;
        }
        break;
	default:
		sFailType = L"Unsupported mode";
		dwError = ERROR_CALL_NOT_IMPLEMENTED;
	}

	if (!dwError)
	{
		if (dwPID)
		{
			CHandle hProcess;
			EnableDebugPrivileges();
			hProcess.Attach(::OpenProcess(PROCESS_DUP_HANDLE, FALSE, dwPID));
			if (!hProcess) {
				dwError = ::GetLastError();
				sFailType = L"Unable to open process";
			}
			else
			{
				_Analysis_assume_(dwHandleValue != 0);
				if (!::DuplicateHandle(hProcess, (HANDLE)(ULONG_PTR)dwHandleValue, ::GetCurrentProcess(), &hEvt.m_h, EVENT_MODIFY_STATE, FALSE, 0)) {
					dwError = ::GetLastError();
					sFailType = L"Unable to open event (handle duplication fail)";
				}
			}
		}
	}

	if (dwError)
	{
		if (hEvt)
		{
			BOOL bRes = FALSE;
			if (!options.bOptReset)
			{
				bRes = options.bOptPulse ? ::PulseEvent(hEvt) : ::SetEvent(hEvt);;
			}
			else
			{
				if (!options.bOptPulse)
				{
					bRes = ::ResetEvent(hEvt);
				}
				else
				{
					// Reset-than-set pulsation
					bRes = ::ResetEvent(hEvt);
					if (bRes) {
						bRes = ::SetEvent(hEvt);
					}
				}
			}
			if (!bRes) {
				dwError = ::GetLastError();
				sFailType = L"Unable to modify event state";
			}
		}
	}

    if (dwError)
    {
		fwprintf(stderr, L"%s. Error: %u %s\n", sFailType, dwError, 
			GetErrorCodeDescription(dwError, buff, _countof(buff)));
        return dwError;
    }

    return NO_ERROR;
}

void PrepareConsole()
{
	int error_code = 0;
	if (_setmode(_fileno(stderr), _O_U16TEXT) == -1 ||
		_setmode(_fileno(stdout), _O_U16TEXT) == -1)
	{
		error_code = errno;
	}
	if (error_code) {
		fprintf(stderr, "Warning: console is not ready to be used with unicode\n");
	}
}

int wmain(int argc, wchar_t** argv)
{
    DWORD dwError = NO_ERROR;
    SUtilOptions options;
    
    PrepareConsole();
	if (argc <= 1) {
        ShowHelp();
    }
    if (!options.Parse(argc - 1, argv + 1)) {
        return 2;
    }

    dwError = SignalAnyEvent(options);
    if (dwError == NO_ERROR) {
		wprintf(L"Done.\n");
    }

    return static_cast<int>(dwError);
}
