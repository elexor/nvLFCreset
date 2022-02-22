// nvLFCreset.cpp : Defines the entry point for the application.
//
#define NOMINMAX

#include "framework.h"
#include <algorithm>
#include "NVAPI/nvapi.h"
#include "nvLFCreset.h"

/*variables*/
UINT WM_TASKBAR = 0;
HWND Hwnd;
HMENU Hmenu;
NOTIFYICONDATA notifyIconData;

NvU32 displayID;
NvU64 lastFlipTimeStamp;
int count;

TCHAR szTIP[64] = TEXT("Prevents low framerate events from triggering sticky LFC");
char szClassName[] = "nvLFCreset";



/*procedures  */
LRESULT CALLBACK WindowProcedure(HWND, UINT, WPARAM, LPARAM);
void minimize();
void restore();
void InitNotifyIconData();


//get DisplayID for primary monitor thanks Kaldaien
NvU32
NvAPI_GetDefaultDisplayId(void)
{
    static NvU32 default_display_id =
        std::numeric_limits <NvU32>::max();

    if (default_display_id != std::numeric_limits <NvU32>::max())
        return default_display_id;

    NvU32               gpuCount = 0;
    NvPhysicalGpuHandle ahGPU[NVAPI_MAX_PHYSICAL_GPUS] = { };

    if (NVAPI_OK == NvAPI_EnumPhysicalGPUs(ahGPU, &gpuCount))
    {
        for (NvU32 i = 0; i < gpuCount; ++i)
        {
            NvU32             displayIdCount = 16;
            NvU32             flags = 0;
            NV_GPU_DISPLAYIDS
                displayIdArray[16] = {                   };
            displayIdArray[0].version = NV_GPU_DISPLAYIDS_VER;

            // Query list of displays connected to this GPU
            if (NVAPI_OK == NvAPI_GPU_GetConnectedDisplayIds(
                ahGPU[i], displayIdArray,
                &displayIdCount, flags))
            {
                if (displayIdArray[0].isActive)
                {
                    default_display_id = displayIdArray[0].displayId;
                    break;
                }
            }

        }
    }

    return 0;
}

void disableFrameSplitting() {
    NV_SET_ADAPTIVE_SYNC_DATA
        setAdaptiveSync;
    ZeroMemory(&setAdaptiveSync, sizeof(NV_SET_ADAPTIVE_SYNC_DATA));
    setAdaptiveSync.version = NV_SET_ADAPTIVE_SYNC_DATA_VER;
    setAdaptiveSync.bDisableFrameSplitting = 1;
    NvAPI_DISP_SetAdaptiveSyncData(displayID, &setAdaptiveSync);
}

void disableAdaptiveSync() {
    NV_SET_ADAPTIVE_SYNC_DATA
        setAdaptiveSync;
    ZeroMemory(&setAdaptiveSync, sizeof(NV_SET_ADAPTIVE_SYNC_DATA));
    setAdaptiveSync.version = NV_SET_ADAPTIVE_SYNC_DATA_VER;
    setAdaptiveSync.bDisableAdaptiveSync = 1;
    NvAPI_DISP_SetAdaptiveSyncData(displayID, &setAdaptiveSync);
}

void getAdaptiveSyncData() {
    NV_GET_ADAPTIVE_SYNC_DATA
        getAdaptiveSync = {                               };
    ZeroMemory(&getAdaptiveSync, sizeof(NV_GET_ADAPTIVE_SYNC_DATA));
    getAdaptiveSync.version = NV_GET_ADAPTIVE_SYNC_DATA_VER;

    static DWORD lastChecked = 0;
    static NvU64 lastFlipTimeStamp = 0;
    static NvU64 lastFlipFrame = 0;
    static double dFlipPrint = 0.0;

    NvAPI_DISP_GetAdaptiveSyncData(
        displayID,
        &getAdaptiveSync);

    NvU64 deltaFlipTime = getAdaptiveSync.lastFlipTimeStamp - lastFlipTimeStamp;
    lastFlipTimeStamp = getAdaptiveSync.lastFlipTimeStamp;
    if (deltaFlipTime > 400000) {                           // detect a low framerate event that will allways trigger LFC
        disableAdaptiveSync();                              // for whatever reason adaptivesync won't reset until both bDisableAdaptiveSync and bDisableFrameSplitting
        disableFrameSplitting();                            // are called one after the other, these commands are only temporary.
    }                                                       // the monitor will change to fixed refreshrate for a 100ms period and then return to vrr operation sticky LFC won't be triggered.
}

void __stdcall
TimerCallback(PVOID lpParameter, BOOLEAN TimerOrWaitFired)  // Poll for adaptivesync data every 1ms
{
    getAdaptiveSyncData();
}



int WINAPI WinMain(HINSTANCE hThisInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpszArgument,
    int nCmdShow)
{
    /* This is the handle for our window */
    MSG messages;            /* Here messages to the application are saved */
    WNDCLASSEX wincl;        /* Data structure for the windowclass */
    WM_TASKBAR = RegisterWindowMessageA("TaskbarCreated");
    /* The Window structure */
    wincl.hInstance = hThisInstance;
    wincl.lpszClassName = szClassName;
    wincl.lpfnWndProc = WindowProcedure;      /* This function is called by windows */
    wincl.style = CS_DBLCLKS;                 /* Catch double-clicks */
    wincl.cbSize = sizeof(WNDCLASSEX);

    /* Use default icon and mouse-pointer */
    wincl.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(ICO1));
    wincl.hIconSm = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(ICO1));
    wincl.hCursor = LoadCursor(NULL, IDC_ARROW);
    wincl.lpszMenuName = NULL;                 /* No menu */
    wincl.cbClsExtra = 0;                      /* No extra bytes after the window class */
    wincl.cbWndExtra = 0;                      /* structure or the window instance */
    wincl.hbrBackground = (HBRUSH)(CreateSolidBrush(RGB(45, 45, 48)));
    /* Register the window class, and if it fails quit the program */
    if (!RegisterClassEx(&wincl))
        return 0;

    /* The class is registered, let's create the program*/
    Hwnd = CreateWindowEx(
        0,                   /* Extended possibilites for variation */
        szClassName,         /* Classname */
        szClassName,         /* Title Text */
        WS_OVERLAPPEDWINDOW, /* default window */
        CW_USEDEFAULT,       /* Windows decides the position */
        CW_USEDEFAULT,       /* where the window ends up on the screen */
        544,                 /* The programs width */
        375,                 /* and height in pixels */
        HWND_DESKTOP,        /* The window is a child-window to desktop */
        NULL,                /* No menu */
        hThisInstance,       /* Program Instance handler */
        NULL                 /* No Window Creation data */
    );
    /*Initialize the NOTIFYICONDATA structure only once*/
    InitNotifyIconData();
    /* Make the window visible on the screen */
    ShowWindow(Hwnd, nCmdShow);
    ShowWindow(Hwnd, SW_HIDE);

    displayID = NvAPI_GetDefaultDisplayId();            //get the DisplayID of the primary monitor
    displayID = NvAPI_GetDefaultDisplayId();
    displayID = NvAPI_GetDefaultDisplayId();

    HANDLE ptrTimerHandle;                              //start 1ms timer callback for checking adaptivesync data                   
    CreateTimerQueueTimer(&ptrTimerHandle, NULL, TimerCallback, NULL, 0, 1, WT_EXECUTEDEFAULT);

    /* Run the message loop. It will run until GetMessage() returns 0 */
    while (GetMessage(&messages, NULL, 0, 0))
    {
        /* Translate virtual-key messages into character messages */
        TranslateMessage(&messages);
        /* Send message to WindowProcedure */
        DispatchMessage(&messages);
    }


    return messages.wParam;
}


/*  This function is called by the Windows function DispatchMessage()  */

LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{

    if (message == WM_TASKBAR && !IsWindowVisible(Hwnd))
    {
        minimize();
        return 0;
    }

    switch (message)                  /* handle the messages */
    {
    case WM_ACTIVATE:
        Shell_NotifyIcon(NIM_ADD, &notifyIconData);
        break;
    case WM_CREATE:

        ShowWindow(Hwnd, SW_HIDE);
        Hmenu = CreatePopupMenu();
        AppendMenu(Hmenu, MF_STRING, ID_TRAY_EXIT, TEXT("Quit"));

        break;

    case WM_SYSCOMMAND:
        /*In WM_SYSCOMMAND messages, the four low-order bits of the wParam parameter
        are used internally by the system. To obtain the correct result when testing the value of wParam,
        an application must combine the value 0xFFF0 with the wParam value by using the bitwise AND operator.*/

        switch (wParam & 0xFFF0)
        {
        case SC_MINIMIZE:
        case SC_CLOSE:
            minimize();
            return 0;
            break;
        }
        break;


        // Our user defined WM_SYSICON message.
    case WM_SYSICON:
    {

        switch (wParam)
        {
        case ID_TRAY_APP_ICON:
            SetForegroundWindow(Hwnd);

            break;
        }


        if (lParam == WM_LBUTTONUP)
        {

            restore();
        }
        else if (lParam == WM_RBUTTONDOWN)
        {
            // Get current mouse position.
            POINT curPoint;
            GetCursorPos(&curPoint);
            SetForegroundWindow(Hwnd);

            // TrackPopupMenu blocks the app until TrackPopupMenu returns

            UINT clicked = TrackPopupMenu(Hmenu, TPM_RETURNCMD | TPM_NONOTIFY, curPoint.x, curPoint.y, 0, hwnd, NULL);



            SendMessage(hwnd, WM_NULL, 0, 0); // send benign message to window to make sure the menu goes away.
            if (clicked == ID_TRAY_EXIT)
            {
                // quit the application.
                Shell_NotifyIcon(NIM_DELETE, &notifyIconData);
                PostQuitMessage(0);
            }
        }
    }
    break;

    // intercept the hittest message..
    case WM_NCHITTEST:
    {
        UINT uHitTest = DefWindowProc(hwnd, WM_NCHITTEST, wParam, lParam);
        if (uHitTest == HTCLIENT)
            return HTCAPTION;
        else
            return uHitTest;
    }

    case WM_CLOSE:

        minimize();
        return 0;
        break;

    case WM_DESTROY:

        PostQuitMessage(0);
        break;

    }

    return DefWindowProc(hwnd, message, wParam, lParam);
}


void minimize()
{
    // hide the main window
    ShowWindow(Hwnd, SW_HIDE);
}


void restore()
{
    ShowWindow(Hwnd, SW_SHOW);
}

void InitNotifyIconData()
{
    memset(&notifyIconData, 0, sizeof(NOTIFYICONDATA));

    notifyIconData.cbSize = sizeof(NOTIFYICONDATA);
    notifyIconData.hWnd = Hwnd;
    notifyIconData.uID = ID_TRAY_APP_ICON;
    notifyIconData.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    notifyIconData.uCallbackMessage = WM_SYSICON; //Set up our invented Windows Message
    notifyIconData.hIcon = (HICON)LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(ICO1));
    strncpy_s(notifyIconData.szTip, szTIP, sizeof(szTIP));
}
