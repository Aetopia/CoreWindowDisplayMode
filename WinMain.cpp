#include <windows.h>
#include <winrt/base.h>
using namespace winrt;

#include <initguid.h>
#include <shobjidl.h>
#include <propsys.h>
#include <propkey.h>
#include <dwmapi.h>

static HWND hWnd = NULL;
static DWORD dmPelsWidth = 0, dmPelsHeight = 0, dmDisplayFrequency = 0;

static VOID SetDisplayMode(BOOL fSet)
{
    MONITORINFOEXW mi = {};
    mi.cbSize = sizeof(MONITORINFOEXW);
    GetMonitorInfoW(MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST), (LPMONITORINFO)&mi);

    if (!fSet)
    {
        ChangeDisplaySettingsExW(NULL, NULL, NULL, 0, NULL);
        return;
    }

    DEVMODEW dm = {.dmSize = sizeof(DEVMODEW),
                   .dmFields = DM_DISPLAYORIENTATION | DM_DISPLAYFIXEDOUTPUT | DM_PELSWIDTH | DM_PELSHEIGHT |
                               DM_DISPLAYFREQUENCY,
                   .dmDisplayOrientation = DMDO_DEFAULT,
                   .dmDisplayFixedOutput = DMDFO_DEFAULT,
                   .dmPelsWidth = dmPelsWidth,
                   .dmPelsHeight = dmPelsHeight,
                   .dmDisplayFrequency = dmDisplayFrequency};

    if (ChangeDisplaySettingsExW(mi.szDevice, &dm, NULL, CDS_TEST, NULL))
    {
        EnumDisplaySettingsW(NULL, ENUM_REGISTRY_SETTINGS, &dm);
        if (dm.dmDisplayOrientation == DMDO_90 || dm.dmDisplayOrientation == DMDO_270)
            dm = {.dmSize = sizeof(DEVMODEW), .dmPelsWidth = dm.dmPelsHeight, .dmPelsHeight = dm.dmPelsWidth};
        dm.dmDisplayOrientation = DMDO_DEFAULT;
        dm.dmDisplayFixedOutput = DMDFO_DEFAULT;
        dm.dmFields =
            DM_DISPLAYORIENTATION | DM_DISPLAYFIXEDOUTPUT | DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY;
    }

    ChangeDisplaySettingsExW(mi.szDevice, &dm, NULL, CDS_FULLSCREEN, NULL);
}

static BOOL IsWindowCloaked(HWND hWnd)
{
    BOOL fCloaked = FALSE;
    DwmGetWindowAttribute(hWnd, DWMWA_CLOAKED, &fCloaked, sizeof(BOOL));
    return !!fCloaked;
}

static BOOL IsWindowBandDesktop(HWND hWnd)
{
    static BOOL (*fnGetWindowBand)(HWND, PDWORD) = NULL;
    if (!fnGetWindowBand)
    {
        HMODULE hModule = LoadLibraryExW(L"User32.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
        fnGetWindowBand = (BOOL(*)(HWND, PDWORD))GetProcAddress(hModule, "GetWindowBand");
        FreeLibrary(hModule);
    }

    DWORD dwBand = 0;
    fnGetWindowBand(hWnd, &dwBand);
    return dwBand == 1;
}

static PCWSTR GetWindowAppUserModelId(HWND hWnd)
{
    PROPVARIANT pvAppUserModelId = {};
    IPropertyStore *pProperties = NULL;

    if (SUCCEEDED(SHGetPropertyStoreForWindow(hWnd, IID_IPropertyStore, (void **)&pProperties)))
    {
        pProperties->GetValue(PKEY_AppUserModel_ID, &pvAppUserModelId);
        pProperties->Release();
    }
    return pvAppUserModelId.pwszVal;
}

static BOOL EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
    PROPVARIANT pvAppUserModelId = {};
    IPropertyStore *pProperties = NULL;

    if (SUCCEEDED(SHGetPropertyStoreForWindow(hwnd, IID_PPV_ARGS(&pProperties))))
    {
        pProperties->GetValue(PKEY_AppUserModel_ID, &pvAppUserModelId);
        pProperties->Release();
    }

    WCHAR szClassName[256] = {};
    GetClassNameW(hwnd, szClassName, 256);

    if (CompareStringOrdinal(pvAppUserModelId.pwszVal, -1, (PCWSTR)lParam, -1, FALSE) == CSTR_EQUAL &&
        CompareStringOrdinal(L"ApplicationFrameWindow", -1, szClassName, -1, FALSE) == CSTR_EQUAL)
    {
        hWnd = hwnd;
        return FALSE;
    }

    return TRUE;
}

class CAppVisibilityEvents : public implements<CAppVisibilityEvents, IAppVisibilityEvents>
{
    HRESULT AppVisibilityOnMonitorChanged(HMONITOR hMonitor, MONITOR_APP_VISIBILITY previousMode,
                                          MONITOR_APP_VISIBILITY currentMode)
    {
        if (hWnd == GetForegroundWindow())
        {
            switch (currentMode)
            {
            case MAV_UNKNOWN:
                break;
            case MAV_NO_APP_VISIBLE:
                SetDisplayMode(FALSE);
                break;
            case MAV_APP_VISIBLE:
                SetDisplayMode(TRUE);
                break;
            }
        }
        return S_OK;
    }

    HRESULT LauncherVisibilityChange(WINBOOL currentVisibleState)
    {
        if (!IsWindowBandDesktop(hWnd) && currentVisibleState && !IsWindowCloaked(hWnd))
            SetDisplayMode(FALSE);
        return S_OK;
    }
};

static VOID WinEventProc(HWINEVENTHOOK hWinEventHook, DWORD event, HWND hwnd, LONG idObject, LONG idChild,
                         DWORD dwEventThread, DWORD dwmsEventTime)
{
    if (hwnd == hWnd)
        switch (event)
        {
        case EVENT_OBJECT_DESTROY:
            PostQuitMessage(0);
            break;

        case EVENT_OBJECT_CLOAKED:
        case EVENT_OBJECT_UNCLOAKED:
            SetDisplayMode(event == EVENT_OBJECT_UNCLOAKED);
            break;

        case EVENT_SYSTEM_FOREGROUND:
            if (!IsWindowBandDesktop(hWnd))
                SetDisplayMode(TRUE);
            break;
        }
}

int wmain(int argc, wchar_t *wargv[])
{
    if (argc != 5)
        return 1;

    if (!(dmPelsWidth = _wtol(wargv[2])) || !(dmPelsHeight = _wtol(wargv[3])) ||
        !(dmDisplayFrequency = _wtol(wargv[4])))
        dmDisplayFrequency = -1;

    CoInitialize(NULL);

    DWORD dwProcessId = 0;
    com_ptr<IApplicationActivationManager> pApplicationActivationManager =
        create_instance<IApplicationActivationManager>(CLSID_ApplicationActivationManager);

    if (!SUCCEEDED(pApplicationActivationManager->ActivateApplication(wargv[1], NULL, AO_NONE, &dwProcessId)) ||
        EnumWindows(EnumWindowsProc, (LPARAM)wargv[1]))
        return 1;

    HANDLE hMutex = CreateMutexW(NULL, TRUE, wargv[1]);
    if (hMutex && GetLastError() != ERROR_ALREADY_EXISTS)
    {
        DWORD dwThreadId = GetWindowThreadProcessId(hWnd, NULL);
        SetWinEventHook(EVENT_OBJECT_DESTROY, EVENT_OBJECT_DESTROY, NULL, WinEventProc, 0, dwThreadId,
                        WINEVENT_OUTOFCONTEXT);
        SetWinEventHook(EVENT_OBJECT_CLOAKED, EVENT_OBJECT_CLOAKED, NULL, WinEventProc, 0, dwThreadId,
                        WINEVENT_OUTOFCONTEXT);
        SetWinEventHook(EVENT_OBJECT_UNCLOAKED, EVENT_OBJECT_UNCLOAKED, NULL, WinEventProc, 0, dwThreadId,
                        WINEVENT_OUTOFCONTEXT);
        SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, NULL, WinEventProc, 0, dwThreadId,
                        WINEVENT_OUTOFCONTEXT);

        com_ptr<IAppVisibility> pAppVisibility = create_instance<IAppVisibility>(CLSID_AppVisibility);
        com_ptr<IAppVisibilityEvents> pCallback = make<CAppVisibilityEvents>();

        MONITOR_APP_VISIBILITY currentMode = MAV_UNKNOWN;
        pAppVisibility->GetAppVisibilityOnMonitor(MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST), &currentMode);
        if (currentMode == MAV_APP_VISIBLE)
            pCallback->AppVisibilityOnMonitorChanged(NULL, MAV_UNKNOWN, currentMode);

        BOOL fVisible = FALSE;
        pAppVisibility->IsLauncherVisible(&fVisible);
        pCallback->LauncherVisibilityChange(fVisible);

        DWORD dwCookie = 0;
        pAppVisibility->Advise(pCallback.get(), &dwCookie);

        MSG msg = {};
        while (GetMessageW(&msg, NULL, 0, 0))
            DispatchMessageW(&msg);
    }
    ReleaseMutex(hMutex);
    return 0;
}