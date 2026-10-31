#pragma once
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "platform_posix.h"

typedef void *HWND;
typedef void *HDC;
typedef struct tagPOINT { LONG x; LONG y; } POINT;
typedef struct tagRECT { LONG left; LONG top; LONG right; LONG bottom; } RECT;
typedef struct tagPAINTSTRUCT { HDC hdc; BOOL fErase; RECT rcPaint; BOOL fRestore; BOOL fIncUpdate; uint8_t rgbReserved[32]; } PAINTSTRUCT;
typedef struct tagBITMAPINFOHEADER { DWORD biSize; LONG biWidth; LONG biHeight; uint16_t biPlanes; uint16_t biBitCount; DWORD biCompression; DWORD biSizeImage; LONG biXPelsPerMeter; LONG biYPelsPerMeter; DWORD biClrUsed; DWORD biClrImportant; } BITMAPINFOHEADER;
typedef struct tagBITMAPINFO { BITMAPINFOHEADER bmiHeader; uint32_t bmiColors[1]; } BITMAPINFO;
typedef struct tagBITMAPFILEHEADER { uint16_t bfType; DWORD bfSize; uint16_t bfReserved1; uint16_t bfReserved2; DWORD bfOffBits; } __attribute__((packed)) BITMAPFILEHEADER;
typedef struct tagWINDOWPLACEMENT { UINT length; UINT flags; UINT showCmd; POINT ptMinPosition; POINT ptMaxPosition; RECT rcNormalPosition; } WINDOWPLACEMENT;
typedef struct tagMONITORINFO { DWORD cbSize; RECT rcMonitor; RECT rcWork; DWORD dwFlags; } MONITORINFO;
typedef intptr_t LRESULT;
typedef uintptr_t WPARAM;
typedef intptr_t LPARAM;
typedef LRESULT (*WNDPROC)(HWND, UINT, WPARAM, LPARAM);
typedef struct tagWNDCLASSW { UINT style; WNDPROC lpfnWndProc; int cbClsExtra; int cbWndExtra; void *hInstance; void *hIcon; void *hCursor; void *hbrBackground; const wchar_t *lpszMenuName; const wchar_t *lpszClassName; } WNDCLASSW;

enum {
    QS_ALLINPUT = 0x04FF, MWMO_INPUTAVAILABLE = 0x0004, BI_RGB = 0, DIB_RGB_COLORS = 0,
    SRCCOPY = 0x00CC0020, GDI_ERROR = -1, COLOR_WINDOW = 5, WS_OVERLAPPEDWINDOW = 0x00CF0000,
    WS_POPUP = (int)0x80000000, CW_USEDEFAULT = (int)0x80000000, SW_HIDE = 0, SW_SHOWNOACTIVATE = 4,
    SWP_NOSIZE = 0x0001, SWP_NOMOVE = 0x0002, SWP_NOZORDER = 0x0004, SWP_NOACTIVATE = 0x0010,
    SWP_FRAMECHANGED = 0x020, SWP_NOOWNERZORDER = 0x0200, MONITOR_DEFAULTTONEAREST = 2,
    GWL_STYLE = -16, HTCAPTION = 2, HTCLIENT = 1, SIZE_MINIMIZED = 1, WHEEL_DELTA = 120,
    WM_MOVE = 0x0003, WM_SIZE = 0x0005, WM_CLOSE = 0x0010, WM_PAINT = 0x000F,
    WM_ACTIVATEAPP = 0x001C, WM_SETCURSOR = 0x0020, WM_NCLBUTTONDOWN = 0x00A1, WM_INPUT = 0x00FF,
    WM_KEYDOWN = 0x0100, WM_KEYUP = 0x0101, WM_SYSKEYDOWN = 0x0104, WM_SYSKEYUP = 0x0105,
    WM_MOUSEMOVE = 0x0200, WM_LBUTTONDOWN = 0x0201, WM_LBUTTONUP = 0x0202,
    WM_RBUTTONDOWN = 0x0204, WM_RBUTTONUP = 0x0205, WM_MBUTTONDOWN = 0x0207, WM_MBUTTONUP = 0x0208,
    WM_MOUSEWHEEL = 0x020A, WM_XBUTTONDOWN = 0x020B, WM_XBUTTONUP = 0x020C,
    WM_MOUSEHWHEEL = 0x020E, WM_CAPTURECHANGED = 0x0215, WM_EXITSIZEMOVE = 0x0232,
    MK_LBUTTON = 0x0001, MK_RBUTTON = 0x0002, MK_SHIFT = 0x0004, MK_CONTROL = 0x0008,
    MK_MBUTTON = 0x0010, MK_XBUTTON1 = 0x0020, MK_XBUTTON2 = 0x0040
};
#define HWND_TOP ((HWND)0)
#define LOWORD(l) ((uint16_t)(((uintptr_t)(l)) & 0xffff))
#define HIWORD(l) ((uint16_t)((((uintptr_t)(l)) >> 16) & 0xffff))
#define IsWindowVisible(w) (1)
#define GetCapture() ((HWND)NULL)
#define InvalidateRect(w, r, e) (1)
#define SetWindowTextW(w, t) (1)

static inline void extra_window_get_size_hwnd(void *win_ptr, int *w, int *h);

static inline BOOL extra_get_client_rect(HWND w, RECT *r) {
    if (!r) return 0;
    int rw = 0, rh = 0;
    extra_window_get_size_hwnd(w, &rw, &rh);
    r->left = 0; r->top = 0; r->right = rw; r->bottom = rh;
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

static inline BOOL extra_show_window(HWND w, int s);
static inline BOOL extra_set_window_pos(HWND w, HWND after, int x, int y, int cx, int cy, UINT flags);
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
#define MAKEINTRESOURCEW(i) ((const wchar_t*)(uintptr_t)(i))

#include "vk_codes.h"
#define MapVirtualKeyW(vk, mode) (vk)

#define GetKeyState(k) (0)
#define SetCursorPos(x, y) (1)
#define RegisterRawInputDevices(d, n, s) (1)
#define GetRawInputData(h, c, d, s, hs) ((UINT)-1)

static inline DWORD MsgWaitForMultipleObjectsEx(DWORD count, const HANDLE *handles, DWORD timeout, DWORD mask, DWORD flags) {
    (void)count; (void)handles; (void)mask; (void)flags;
    if (timeout != INFINITE) {
        usleep((useconds_t)timeout * 1000);
    } else {
        usleep(16000);
    }
    return 0;
}

__attribute__((weak)) SDL_Window *g_sdl_window;
__attribute__((weak)) SDL_Gamepad *g_sdl_gamepad;
__attribute__((weak)) int g_sdl_input_focus, g_sdl_fullscreen, g_sdl_exit_requested, g_sdl_mouse_captured;
__attribute__((weak)) int g_sdl_active_device = 1;
__attribute__((weak)) uint32_t g_sdl_prompt_generation = 1;

static inline int extra_get_active_device(void) { return g_sdl_active_device; }
static inline uint32_t extra_get_prompt_generation(void) { return g_sdl_prompt_generation; }
static inline void extra_set_active_device(int dev) {
    if (g_sdl_active_device != dev) {
        g_sdl_active_device = dev;
        g_sdl_prompt_generation++;
    }
}


static inline void *extra_window_ptr(void);

static inline void extra_window_get_size_hwnd(void *win_ptr, int *w, int *h) {
    SDL_Window *win = (SDL_Window*)win_ptr;
    if (!win) win = g_sdl_window;
    if (!win) win = (SDL_Window*)extra_window_ptr();
    int rw = 0, rh = 0;
    if (win) SDL_GetWindowSizeInPixels(win, &rw, &rh);
    if (rw <= 0 || rh <= 0) {
        const char *ew = getenv("AC_WIDTH");
        const char *eh = getenv("AC_HEIGHT");
        rw = (ew && atoi(ew) > 0) ? atoi(ew) : 1600;
        rh = (eh && atoi(eh) > 0) ? atoi(eh) : 900;
    }
    if (w) *w = rw;
    if (h) *h = rh;
}

static inline void extra_window_get_size(int *w, int *h) {
    extra_window_get_size_hwnd(NULL, w, h);
}

static inline BOOL extra_set_window_pos(HWND w, HWND after, int x, int y, int cx, int cy, UINT flags) {
    (void)after; (void)x; (void)y;
    SDL_Window *win = (SDL_Window*)w;
    if (!win) win = g_sdl_window;
    if (win && cx > 0 && cy > 0 && !(flags & SWP_NOSIZE)) {
        SDL_SetWindowSize(win, cx, cy);
    }
    return 1;
}

static inline BOOL extra_show_window(HWND w, int s) {
    SDL_Window *win = (SDL_Window*)w;
    if (!win) win = g_sdl_window;
    if (win) {
        if (s == SW_HIDE) SDL_HideWindow(win);
        else {
            SDL_ShowWindow(win);
            SDL_RaiseWindow(win);
        }
    }
    return 1;
}

static inline uint8_t extra_sdl_to_vk(SDL_Keycode key) {
    if (key >= SDLK_A && key <= SDLK_Z) return (uint8_t)('A' + (key - SDLK_A));
    if (key >= SDLK_0 && key <= SDLK_9) return (uint8_t)('0' + (key - SDLK_0));
    switch (key) {
        case SDLK_RETURN: return 0x0D;
        case SDLK_ESCAPE: return 0x1B;
        case SDLK_BACKSPACE: return 0x08;
        case SDLK_TAB: return 0x09;
        case SDLK_SPACE: return 0x20;
        case SDLK_LSHIFT: case SDLK_RSHIFT: return 0x10;
        case SDLK_LCTRL: case SDLK_RCTRL: return 0x11;
        case SDLK_LALT: case SDLK_RALT: return 0x12;
        case SDLK_LEFT: return 0x25;
        case SDLK_UP: return 0x26;
        case SDLK_RIGHT: return 0x27;
        case SDLK_DOWN: return 0x28;
        case SDLK_F11: return 0x7A;
        default: return key < 256 ? (uint8_t)key : 0;
    }
}

static inline void *extra_window_init(int w, int h, const char *title) {
    if (g_sdl_window) return g_sdl_window;
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return NULL;
    }
    const char *ew = getenv("AC_WIDTH");
    const char *eh = getenv("AC_HEIGHT");
    if (ew && atoi(ew) > 0) w = atoi(ew);
    if (eh && atoi(eh) > 0) h = atoi(eh);
    if (w <= 0) w = 1600;
    if (h <= 0) h = 900;

    g_sdl_window = SDL_CreateWindow(title ? title : "Astral Chain", w, h,
                                   SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (!g_sdl_window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return NULL;
    }
    SDL_ShowWindow(g_sdl_window);
    SDL_RaiseWindow(g_sdl_window);

    const char *fs = getenv("AC_FULLSCREEN");
    if (fs && fs[0] == '1') {
        SDL_SetWindowFullscreen(g_sdl_window, true);
        g_sdl_fullscreen = 1;
    }
    printf("[video] SDL3 window initialized: %dx%d (fullscreen=%d)\n", w, h, g_sdl_fullscreen);
    return g_sdl_window;
}

static inline void *extra_window_ptr(void) {
    if (!g_sdl_window) extra_window_init(1600, 900, "Astral Chain");
    return g_sdl_window;
}

static inline void extra_window_set_fullscreen(int on) {
    if (!g_sdl_window) return;
    g_sdl_fullscreen = on;
    SDL_SetWindowFullscreen(g_sdl_window, on ? true : false);
}

static inline void extra_window_set_mouse_capture(int on) {
    if (!g_sdl_window) return;
    g_sdl_mouse_captured = on;
    SDL_SetWindowRelativeMouseMode(g_sdl_window, on ? true : false);
}

static void (*g_app_enqueue_fn)(unsigned) = NULL;
static void *extra_exit_watchdog_thread(void *unused) {
    (void)unused; usleep(400000); fflush(stdout); fflush(stderr); _exit(0); return NULL;
}
static inline void extra_trigger_exit(void) {
    if (g_sdl_exit_requested) return;
    g_sdl_exit_requested = 1;
    if (g_sdl_window) SDL_HideWindow(g_sdl_window);
    if (g_app_enqueue_fn) g_app_enqueue_fn(4);
    pthread_t th; pthread_create(&th, NULL, extra_exit_watchdog_thread, NULL); pthread_detach(th);
}

static inline int extra_window_poll_events(void (*key_cb)(uint8_t vk, int down),
                                          void (*mouse_motion_cb)(float dx, float dy),
                                          void (*mouse_button_cb)(uint8_t btn, int down),
                                          void (*mouse_wheel_cb)(int y)) {
    if (!g_sdl_window) return 0;
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
            case SDL_EVENT_QUIT:
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                extra_trigger_exit();
                break;
            case SDL_EVENT_WINDOW_FOCUS_GAINED:
                g_sdl_input_focus = 1;
                break;
            case SDL_EVENT_WINDOW_FOCUS_LOST:
                g_sdl_input_focus = 0;
                break;
            case SDL_EVENT_KEY_DOWN:
                extra_set_active_device(1);
                if (e.key.key == SDLK_F11) {
                    extra_window_set_fullscreen(!g_sdl_fullscreen);
                } else if ((e.key.key == SDLK_F4 && (e.key.mod & SDL_KMOD_ALT)) ||
                           (e.key.key == SDLK_Q && (e.key.mod & SDL_KMOD_CTRL))) {
                    extra_trigger_exit();
                } else if (key_cb) {
                    key_cb(extra_sdl_to_vk(e.key.key), 1);
                }
                break;
            case SDL_EVENT_KEY_UP:
                if (key_cb && e.key.key != SDLK_F11) key_cb(extra_sdl_to_vk(e.key.key), 0);
                break;
            case SDL_EVENT_MOUSE_MOTION:
                if (e.motion.xrel * e.motion.xrel + e.motion.yrel * e.motion.yrel > 2.0f) extra_set_active_device(1);
                if (mouse_motion_cb) mouse_motion_cb(e.motion.xrel, e.motion.yrel);
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                extra_set_active_device(1);
                if (mouse_button_cb) mouse_button_cb((uint8_t)e.button.button, 1);
                break;
            case SDL_EVENT_MOUSE_BUTTON_UP:
                if (mouse_button_cb) mouse_button_cb((uint8_t)e.button.button, 0);
                break;
            case SDL_EVENT_MOUSE_WHEEL:
                extra_set_active_device(1);
                if (mouse_wheel_cb) mouse_wheel_cb((int)e.wheel.y);
                break;
            case SDL_EVENT_GAMEPAD_ADDED:
                if (!g_sdl_gamepad) {
                    g_sdl_gamepad = SDL_OpenGamepad(e.gdevice.which);
                    if (g_sdl_gamepad) {
                        extra_set_active_device(0);
                        printf("[input] SDL3 gamepad connected: %s\n", SDL_GetGamepadName(g_sdl_gamepad));
                    }
                }
                break;
            case SDL_EVENT_GAMEPAD_REMOVED:
                if (g_sdl_gamepad && e.gdevice.which == SDL_GetGamepadID(g_sdl_gamepad)) {
                    printf("[input] SDL3 gamepad disconnected\n");
                    SDL_CloseGamepad(g_sdl_gamepad); g_sdl_gamepad = NULL;
                    extra_set_active_device(1);
                }
                break;
            default:
                break;
        }
    }
    return !g_sdl_exit_requested;
}

static inline int extra_is_gamepad_connected(void) { return g_sdl_gamepad != NULL; }
static inline void extra_poll_gamepad(uint64_t *buttons, int32_t *lx, int32_t *ly, int32_t *rx, int32_t *ry) {
    if (!g_sdl_gamepad) return;
    uint64_t b = 0;
    if (SDL_GetGamepadButton(g_sdl_gamepad, SDL_GAMEPAD_BUTTON_SOUTH)) b |= (1ull << 1); /* B */
    if (SDL_GetGamepadButton(g_sdl_gamepad, SDL_GAMEPAD_BUTTON_EAST))  b |= (1ull << 0); /* A */
    if (SDL_GetGamepadButton(g_sdl_gamepad, SDL_GAMEPAD_BUTTON_WEST))  b |= (1ull << 3); /* Y */
    if (SDL_GetGamepadButton(g_sdl_gamepad, SDL_GAMEPAD_BUTTON_NORTH)) b |= (1ull << 2); /* X */
    if (SDL_GetGamepadButton(g_sdl_gamepad, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER))  b |= (1ull << 6); /* L */
    if (SDL_GetGamepadButton(g_sdl_gamepad, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)) b |= (1ull << 7); /* R */
    if (SDL_GetGamepadButton(g_sdl_gamepad, SDL_GAMEPAD_BUTTON_LEFT_STICK))     b |= (1ull << 4);
    if (SDL_GetGamepadButton(g_sdl_gamepad, SDL_GAMEPAD_BUTTON_RIGHT_STICK))    b |= (1ull << 5);
    if (SDL_GetGamepadButton(g_sdl_gamepad, SDL_GAMEPAD_BUTTON_START))          b |= (1ull << 10);
    if (SDL_GetGamepadButton(g_sdl_gamepad, SDL_GAMEPAD_BUTTON_BACK))           b |= (1ull << 11);
    if (SDL_GetGamepadButton(g_sdl_gamepad, SDL_GAMEPAD_BUTTON_DPAD_LEFT))      b |= (1ull << 12);
    if (SDL_GetGamepadButton(g_sdl_gamepad, SDL_GAMEPAD_BUTTON_DPAD_UP))        b |= (1ull << 13);
    if (SDL_GetGamepadButton(g_sdl_gamepad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT))     b |= (1ull << 14);
    if (SDL_GetGamepadButton(g_sdl_gamepad, SDL_GAMEPAD_BUTTON_DPAD_DOWN))      b |= (1ull << 15);
    if (SDL_GetGamepadAxis(g_sdl_gamepad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > 8000)  b |= (1ull << 8);  /* ZL */
    if (SDL_GetGamepadAxis(g_sdl_gamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > 8000) b |= (1ull << 9);  /* ZR */
    int16_t glx = SDL_GetGamepadAxis(g_sdl_gamepad, SDL_GAMEPAD_AXIS_LEFTX);
    int16_t gly = SDL_GetGamepadAxis(g_sdl_gamepad, SDL_GAMEPAD_AXIS_LEFTY);
    int16_t grx = SDL_GetGamepadAxis(g_sdl_gamepad, SDL_GAMEPAD_AXIS_RIGHTX);
    int16_t gry = SDL_GetGamepadAxis(g_sdl_gamepad, SDL_GAMEPAD_AXIS_RIGHTY);
    if (abs(glx) > 8000 || abs(gly) > 8000) { *lx = glx; *ly = -gly; }
    if (abs(grx) > 8000 || abs(gry) > 8000) { *rx = grx; *ry = -gry; }
    if (b || abs(glx) > 8000 || abs(gly) > 8000 || abs(grx) > 8000 || abs(gry) > 8000) extra_set_active_device(0);
    *buttons |= b;
}


static inline void extra_app_set_fullscreen(int *app_fs, int on) {
    if (app_fs) *app_fs = on;
    extra_window_set_fullscreen(on);
}

static inline void extra_app_pump_inline(void) {
    extra_window_poll_events(NULL, NULL, NULL, NULL);
}

static inline void *extra_app_open_window(HANDLE app_msg_ev, unsigned *app_foc, unsigned *msg_cnt, void (*enqueue_fn)(unsigned)) {
    (void)app_msg_ev;
    g_app_enqueue_fn = enqueue_fn;
    void *win = extra_window_ptr();
    if (win) {
        if (app_foc) *app_foc = 1;
        if (msg_cnt && enqueue_fn && !(*msg_cnt)) enqueue_fn(15);
    }
    return win;
}
