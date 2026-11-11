#pragma once
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include "hooks/platform/platform_posix.h"
#include "hooks/input/vk_codes.h"

#define EXTRA_PAINT_RESERVED_BYTES 32
#define EXTRA_MSG_WAIT_DEFAULT_US 16000
#define EXTRA_US_PER_MS 1000
#define EXTRA_LOWORD_MASK 0xffff
#define EXTRA_HIWORD_SHIFT 16

typedef void *HWND;
typedef void *HDC;

typedef struct tagPOINT {
    LONG x;
    LONG y;
} POINT;

typedef struct tagRECT {
    LONG left;
    LONG top;
    LONG right;
    LONG bottom;
} RECT;

typedef struct tagPAINTSTRUCT {
    HDC hdc;
    BOOL fErase;
    RECT rcPaint;
    BOOL fRestore;
    BOOL fIncUpdate;
    uint8_t rgbReserved[EXTRA_PAINT_RESERVED_BYTES];
} PAINTSTRUCT;

typedef struct tagBITMAPINFOHEADER {
    DWORD biSize;
    LONG biWidth;
    LONG biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    DWORD biCompression;
    DWORD biSizeImage;
    LONG biXPelsPerMeter;
    LONG biYPelsPerMeter;
    DWORD biClrUsed;
    DWORD biClrImportant;
} BITMAPINFOHEADER;

typedef struct tagBITMAPINFO {
    BITMAPINFOHEADER bmiHeader;
    uint32_t bmiColors[1];
} BITMAPINFO;

typedef struct tagBITMAPFILEHEADER {
    uint16_t bfType;
    DWORD bfSize;
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    DWORD bfOffBits;
} __attribute__((packed)) BITMAPFILEHEADER;

typedef struct tagWINDOWPLACEMENT {
    UINT length;
    UINT flags;
    UINT showCmd;
    POINT ptMinPosition;
    POINT ptMaxPosition;
    RECT rcNormalPosition;
} WINDOWPLACEMENT;

typedef struct tagMONITORINFO {
    DWORD cbSize;
    RECT rcMonitor;
    RECT rcWork;
    DWORD dwFlags;
} MONITORINFO;

typedef intptr_t LRESULT;
typedef uintptr_t WPARAM;
typedef intptr_t LPARAM;
typedef LRESULT (*WNDPROC)(HWND, UINT, WPARAM, LPARAM);

typedef struct tagWNDCLASSW {
    UINT style;
    WNDPROC lpfnWndProc;
    int cbClsExtra;
    int cbWndExtra;
    void *hInstance;
    void *hIcon;
    void *hCursor;
    void *hbrBackground;
    const wchar_t *lpszMenuName;
    const wchar_t *lpszClassName;
} WNDCLASSW;

enum {
    QS_ALLINPUT = 0x04FF,
    MWMO_INPUTAVAILABLE = 0x0004,
    BI_RGB = 0,
    DIB_RGB_COLORS = 0,
    SRCCOPY = 0x00CC0020,
    GDI_ERROR = -1,
    COLOR_WINDOW = 5,
    WS_OVERLAPPEDWINDOW = 0x00CF0000,
    WS_POPUP = (int)0x80000000,
    CW_USEDEFAULT = (int)0x80000000,
    SW_HIDE = 0,
    SW_SHOWNOACTIVATE = 4,
    SWP_NOSIZE = 0x0001,
    SWP_NOMOVE = 0x0002,
    SWP_NOZORDER = 0x0004,
    SWP_NOACTIVATE = 0x0010,
    SWP_FRAMECHANGED = 0x0020,
    SWP_NOOWNERZORDER = 0x0200,
    MONITOR_DEFAULTTONEAREST = 2,
    GWL_STYLE = -16,
    HTCAPTION = 2,
    HTCLIENT = 1,
    SIZE_MINIMIZED = 1,
    WHEEL_DELTA = 120,
    WM_MOVE = 0x0003,
    WM_SIZE = 0x0005,
    WM_CLOSE = 0x0010,
    WM_PAINT = 0x000F,
    WM_ACTIVATEAPP = 0x001C,
    WM_SETCURSOR = 0x0020,
    WM_NCLBUTTONDOWN = 0x00A1,
    WM_INPUT = 0x00FF,
    WM_KEYDOWN = 0x0100,
    WM_KEYUP = 0x0101,
    WM_SYSKEYDOWN = 0x0104,
    WM_SYSKEYUP = 0x0105,
    WM_MOUSEMOVE = 0x0200,
    WM_LBUTTONDOWN = 0x0201,
    WM_LBUTTONUP = 0x0202,
    WM_RBUTTONDOWN = 0x0204,
    WM_RBUTTONUP = 0x0205,
    WM_MBUTTONDOWN = 0x0207,
    WM_MBUTTONUP = 0x0208,
    WM_MOUSEWHEEL = 0x020A,
    WM_XBUTTONDOWN = 0x020B,
    WM_XBUTTONUP = 0x020C,
    WM_MOUSEHWHEEL = 0x020E,
    WM_CAPTURECHANGED = 0x0215,
    WM_EXITSIZEMOVE = 0x0232,
    MK_LBUTTON = 0x0001,
    MK_RBUTTON = 0x0002,
    MK_SHIFT = 0x0004,
    MK_CONTROL = 0x0008,
    MK_MBUTTON = 0x0010,
    MK_XBUTTON1 = 0x0020,
    MK_XBUTTON2 = 0x0040
};

#define HWND_TOP ((HWND)0)
#define LOWORD(l) ((uint16_t)(((uintptr_t)(l)) & EXTRA_LOWORD_MASK))
#define HIWORD(l) ((uint16_t)((((uintptr_t)(l)) >> EXTRA_HIWORD_SHIFT) & EXTRA_LOWORD_MASK))
#define IsWindowVisible(w) (1)
#define GetCapture() ((HWND)NULL)
#define InvalidateRect(w, r, e) (1)
#define SetWindowTextW(w, t) (1)

static inline void extra_window_get_size_hwnd(void *win_ptr, int *w, int *h);
static inline BOOL extra_show_window(HWND w, int s);
static inline BOOL extra_set_window_pos(HWND w, HWND after, int x, int y, int cx, int cy, UINT flags);

static inline BOOL extra_get_client_rect(HWND w, RECT *r) {
    if (!r) {
        return 0;
    }
    int rw = 0;
    int rh = 0;
    extra_window_get_size_hwnd(w, &rw, &rh);
    r->left = 0;
    r->top = 0;
    r->right = rw;
    r->bottom = rh;
    return 1;
}

#define GetClientRect(w, r) extra_get_client_rect(w, r)
#define GetWindowRect(w, r) extra_get_client_rect(w, r)
#define MapWindowPoints(w1, w2, p, c) (0)
#define ClipCursor(r) (0)
#define IsZoomed(w) (0)
#define IsIconic(w) (0)
#define GetCursorPos(p) memset((p), 0, sizeof(*(p)))
#define SetCapture(w) ((HWND)(w))
#define ReleaseCapture() (1)
#define SetCursor(c) (NULL)
#define BeginPaint(w, p) ((HDC)NULL)
#define EndPaint(w, p) (1)
#define DefWindowProcW(w, m, wp, lp) (0)

#define ShowWindow(w, s) extra_show_window(w, s)
#define UpdateWindow(w) (1)
#define SetWindowPos(w, a, x, y, cx, cy, f) extra_set_window_pos(w, a, x, y, cx, cy, f)
#define SetWindowPlacement(w, p) (1)
#define GetWindowPlacement(w, p) (0)
#define GetMonitorInfoW(m, i) (0)
#define MonitorFromWindow(w, f) (NULL)
#define SetWindowLongPtrW(w, i, v) (0)
#define GetWindowLongPtrW(w, i) (0)
#define AdjustWindowRect(r, s, m) (1)
#define RegisterClassW(c) (1)
#define CreateWindowExW(ex, cn, wn, s, x, y, w, h, p, m, i, lp) (NULL)
#define LoadCursorW(i, n) (NULL)
#define MAKEINTRESOURCEW(i) ((const wchar_t *)(uintptr_t)(i))
#define MapVirtualKeyW(vk, mode) (vk)
#define GetKeyState(k) (0)
#define SetCursorPos(x, y) (1)
#define RegisterRawInputDevices(d, n, s) (1)
#define GetRawInputData(h, c, d, s, hs) ((UINT)-1)

static inline DWORD MsgWaitForMultipleObjectsEx(DWORD count, const HANDLE *handles, DWORD timeout, DWORD mask, DWORD flags) {
    (void)count;
    (void)handles;
    (void)mask;
    (void)flags;
    if (timeout != INFINITE) {
        usleep((useconds_t)timeout * EXTRA_US_PER_MS);
    } else {
        usleep(EXTRA_MSG_WAIT_DEFAULT_US);
    }
    return 0;
}
