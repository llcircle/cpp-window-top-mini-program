#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <memory>

#pragma comment(lib, "comctl32.lib")
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// Global variables
HINSTANCE g_hInstance = NULL;
HWND g_hMainWindow = NULL;
HWND g_hTargetWindow = NULL;
HCURSOR g_hPinCursor = NULL;
bool g_bCapturing = false;
std::vector<HWND> g_pinnedWindows;

// Function declarations
LRESULT CALLBACK MainWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam);
void SetWindowAlwaysOnTop(HWND hwnd, bool bOnTop);
HWND GetWindowUnderCursor(POINT pt);
void UpdatePinnedWindowsList(HWND hListView);
void InitializeControls(HWND hwnd);

// Hook handle
HHOOK g_hMouseHook = NULL;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    g_hInstance = hInstance;

    // Initialize common controls
    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icc.dwICC = ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icc);

    // Load pin cursor
    g_hPinCursor = LoadCursor(NULL, IDC_CROSS); // Using standard cross cursor, replace with custom pin cursor if available

    // Register window class
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = MainWndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = L"WindowPinnerClass";
    wcex.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClassEx(&wcex))
    {
        MessageBox(NULL, L"Window Registration Failed!", L"Error", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    // Create main window
    g_hMainWindow = CreateWindow(
        L"WindowPinnerClass",
        L"窗口置顶工具",
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT,
        650, 550,
        NULL,
        NULL,
        hInstance,
        NULL);

    if (!g_hMainWindow)
    {
        MessageBox(NULL, L"Window Creation Failed!", L"Error", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    // Initialize controls
    InitializeControls(g_hMainWindow);

    // Show window
    ShowWindow(g_hMainWindow, nCmdShow);
    UpdateWindow(g_hMainWindow);

    // Message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Cleanup
    if (g_hMouseHook)
    {
        UnhookWindowsHookEx(g_hMouseHook);
    }

    return (int)msg.wParam;
}

void InitializeControls(HWND hwnd)
{
    // Create "Pin Window" button
    CreateWindow(
        L"BUTTON",
        L"置顶窗口",
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        20, 20, 120, 40,
        hwnd,
        (HMENU)1,
        g_hInstance,
        NULL);

    // Create "Unpin Selected" button
    CreateWindow(
        L"BUTTON",
        L"取消置顶选中",
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        160, 20, 120, 40,
        hwnd,
        (HMENU)2,
        g_hInstance,
        NULL);

    // Create "Unpin All" button
    CreateWindow(
        L"BUTTON",
        L"取消全部置顶",
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        300, 20, 120, 40,
        hwnd,
        (HMENU)3,
        g_hInstance,
        NULL);

    // Create ListView to show pinned windows
    HWND hListView = CreateWindow(
        WC_LISTVIEW,
        L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SINGLESEL,
        20, 80, 600, 400,
        hwnd,
        (HMENU)4,
        g_hInstance,
        NULL);

    // Add columns to ListView
    LVCOLUMN lvc;
    lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
    lvc.fmt = LVCFMT_LEFT;

    lvc.iSubItem = 0;
    lvc.cx = 270;
    lvc.pszText = const_cast<LPWSTR>(L"窗口标题");
    ListView_InsertColumn(hListView, 0, &lvc);

    lvc.iSubItem = 1;
    lvc.cx = 150;
    lvc.pszText = const_cast<LPWSTR>(L"状态");
    ListView_InsertColumn(hListView, 1, &lvc);
}

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    HWND hListView = GetDlgItem(hwnd, 4);

    switch (uMsg)
    {
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case 1: // Pin Window button
            if (!g_bCapturing)
            {
                g_bCapturing = true;
                // Set the mouse hook
                g_hMouseHook = SetWindowsHookEx(WH_MOUSE_LL, MouseHookProc, g_hInstance, 0);
                SetCursor(g_hPinCursor);
                SetCapture(hwnd);
                // Change button text
                SetWindowText(GetDlgItem(hwnd, 1), L"取消");
            }
            else
            {
                g_bCapturing = false;
                if (g_hMouseHook)
                {
                    UnhookWindowsHookEx(g_hMouseHook);
                    g_hMouseHook = NULL;
                }
                ReleaseCapture();
                SetCursor(LoadCursor(NULL, IDC_ARROW));
                // Change button text back
                SetWindowText(GetDlgItem(hwnd, 1), L"置顶窗口");
            }
            break;
        case 2: // Unpin Selected button
        {
            int selectedIndex = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
            if (selectedIndex != -1 && selectedIndex < static_cast<int>(g_pinnedWindows.size()))
            {
                HWND hSelected = g_pinnedWindows[selectedIndex];
                if (IsWindow(hSelected))
                {
                    SetWindowAlwaysOnTop(hSelected, false);
                    g_pinnedWindows.erase(g_pinnedWindows.begin() + selectedIndex);
                    UpdatePinnedWindowsList(hListView);
                }
                else
                {
                    // Remove invalid window from list
                    g_pinnedWindows.erase(g_pinnedWindows.begin() + selectedIndex);
                    UpdatePinnedWindowsList(hListView);
                }
            }
        }
        break;
        case 3: // Unpin All button
            for (HWND hPinned : g_pinnedWindows)
            {
                SetWindowAlwaysOnTop(hPinned, false);
            }
            g_pinnedWindows.clear();
            UpdatePinnedWindowsList(hListView);
            break;
        }
        break;

    case WM_LBUTTONDOWN:
        if (g_bCapturing)
        {
            POINT pt;
            GetCursorPos(&pt);
            g_hTargetWindow = GetWindowUnderCursor(pt);

            if (g_hTargetWindow && g_hTargetWindow != g_hMainWindow)
            {
                // Check if window is already pinned
                auto it = std::find(g_pinnedWindows.begin(), g_pinnedWindows.end(), g_hTargetWindow);
                if (it == g_pinnedWindows.end())
                {
                    // Pin the window
                    SetWindowAlwaysOnTop(g_hTargetWindow, true);
                    g_pinnedWindows.push_back(g_hTargetWindow);
                    UpdatePinnedWindowsList(hListView);
                }
            }

            // End capturing
            g_bCapturing = false;
            if (g_hMouseHook)
            {
                UnhookWindowsHookEx(g_hMouseHook);
                g_hMouseHook = NULL;
            }
            ReleaseCapture();
            SetCursor(LoadCursor(NULL, IDC_ARROW));
            SetWindowText(GetDlgItem(hwnd, 1), L"Pin Window");
        }
        break;

    case WM_DESTROY:
        // Unpin all windows before exiting
        for (HWND hPinned : g_pinnedWindows)
        {
            SetWindowAlwaysOnTop(hPinned, false);
        }

        if (g_hMouseHook)
        {
            UnhookWindowsHookEx(g_hMouseHook);
            g_hMouseHook = NULL;
        }

        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

LRESULT CALLBACK MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode >= 0 && wParam == WM_LBUTTONDOWN)
    {
        MOUSEHOOKSTRUCT* pMouseHook = (MOUSEHOOKSTRUCT*)lParam;
        g_hTargetWindow = GetWindowUnderCursor(pMouseHook->pt);

        if (g_hTargetWindow && g_hTargetWindow != g_hMainWindow)
        {
            // Check if window is already pinned
            auto it = std::find(g_pinnedWindows.begin(), g_pinnedWindows.end(), g_hTargetWindow);
            if (it == g_pinnedWindows.end())
            {
                // Pin the window
                SetWindowAlwaysOnTop(g_hTargetWindow, true);
                g_pinnedWindows.push_back(g_hTargetWindow);
                UpdatePinnedWindowsList(GetDlgItem(g_hMainWindow, 4));
            }
        }

        // End capturing
        g_bCapturing = false;
        if (g_hMouseHook)
        {
            UnhookWindowsHookEx(g_hMouseHook);
            g_hMouseHook = NULL;
        }
        ReleaseCapture();
        SetCursor(LoadCursor(NULL, IDC_ARROW));
        SetWindowText(GetDlgItem(g_hMainWindow, 1), L"Pin Window");

        return 1; // Prevent further processing
    }

    return CallNextHookEx(g_hMouseHook, nCode, wParam, lParam);
}

HWND GetWindowUnderCursor(POINT pt)
{
    HWND hWnd = WindowFromPoint(pt);

    // Check if window is valid and not a child window
    while (hWnd && (GetWindowLong(hWnd, GWL_STYLE) & WS_CHILD))
    {
        hWnd = GetParent(hWnd);
    }

    // Additional check for window visibility and enabled state
    if (hWnd && (!IsWindowVisible(hWnd) || !IsWindowEnabled(hWnd)))
    {
        return NULL;
    }

    return hWnd;
}

void SetWindowAlwaysOnTop(HWND hwnd, bool bOnTop)
{
    if (!IsWindow(hwnd))
        return;

    if (bOnTop)
    {
        if (!SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE))
        {
            DWORD err = GetLastError();
            // Removed message box for access denied error
        }
    }
    else
    {
        SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    }
}

void UpdatePinnedWindowsList(HWND hListView)
{
    ListView_DeleteAllItems(hListView);

    for (size_t i = 0; i < g_pinnedWindows.size(); i++)
    {
        HWND hPinned = g_pinnedWindows[i];

        // Get window title
        wchar_t windowTitle[256] = { 0 };
        GetWindowText(hPinned, windowTitle, 256);

        // Add item to list view
        LVITEM lvi = { 0 };
        lvi.mask = LVIF_TEXT;
        lvi.iItem = static_cast<int>(i);
        lvi.iSubItem = 0;
        lvi.pszText = windowTitle;

        int index = ListView_InsertItem(hListView, &lvi);

        // Check if window is actually topmost
        LONG_PTR style = GetWindowLongPtr(hPinned, GWL_EXSTYLE);
        bool isTopmost = (style & WS_EX_TOPMOST) == WS_EX_TOPMOST;

        // Add status with color
        if (isTopmost)
        {
            ListView_SetItemText(hListView, index, 1, const_cast<LPWSTR>(L"已置顶"));
        }
        else
        {
            ListView_SetItemText(hListView, index, 1, const_cast<LPWSTR>(L"置顶失败"));
            // Set red color for failed items
            ListView_SetItemState(hListView, index, LVIS_CUT, LVIS_CUT);

            // Set text color to red
            LVITEM lvi = { 0 };
            lvi.mask = LVIF_STATE;
            lvi.iItem = index;
            lvi.state = LVIS_CUT;
            lvi.stateMask = LVIS_CUT;
            ListView_SetItem(hListView, &lvi);
            ListView_SetTextColor(hListView, RGB(255, 0, 0));
        }
    }
}



