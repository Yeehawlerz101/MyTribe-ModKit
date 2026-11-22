#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <GL/gl.h>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>

#ifndef GL_BGRA_EXT
#define GL_BGRA_EXT 0x80E1
#endif
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_UNPACK_ROW_LENGTH
#define GL_UNPACK_ROW_LENGTH 0x0CF2
#endif

// ---------------------------------------------------------------------------
// Exact SDL 1.2.11 x86 ABI definitions
// ---------------------------------------------------------------------------

struct SDL_Rect {
    int16_t x, y;
    uint16_t w, h;
};
static_assert(sizeof(SDL_Rect) == 8, "SDL_Rect size mismatch");

struct SDL_Palette {
    int ncolors;
    void* colors;
};
static_assert(sizeof(SDL_Palette) == 8, "SDL_Palette size mismatch");

struct SDL_PixelFormat {
    SDL_Palette* palette;
    uint8_t BitsPerPixel;
    uint8_t BytesPerPixel;
    uint8_t Rloss;
    uint8_t Gloss;
    uint8_t Bloss;
    uint8_t Aloss;
    uint8_t Rshift;
    uint8_t Gshift;
    uint8_t Bshift;
    uint8_t Ashift;
    uint32_t Rmask;
    uint32_t Gmask;
    uint32_t Bmask;
    uint32_t Amask;
    uint32_t colorkey;
    uint8_t alpha;
};
static_assert(sizeof(SDL_PixelFormat) == 40, "SDL_PixelFormat size mismatch");

struct SDL_Surface {
    uint32_t flags;
    SDL_PixelFormat* format;
    int w, h;
    uint16_t pitch;
    void* pixels;
    int offset;
    void* hwdata;
    SDL_Rect clip_rect;
    uint32_t unused1;
    uint32_t locked;
    void* map;
    unsigned int format_version;
    int refcount;
};
static_assert(sizeof(SDL_Surface) == 60, "SDL_Surface size mismatch");

struct SDL_keysym {
    uint8_t scancode;
    int sym;
    int mod;
    uint16_t unicode;
};
static_assert(sizeof(SDL_keysym) == 16, "SDL_keysym size mismatch");

struct SDL_KeyboardEvent {
    uint8_t type;
    uint8_t which;
    uint8_t state;
    SDL_keysym keysym;
};

struct SDL_MouseMotionEvent {
    uint8_t type;
    uint8_t which;
    uint8_t state;
    uint16_t x, y;
    int16_t xrel;
    int16_t yrel;
};

struct SDL_MouseButtonEvent {
    uint8_t type;
    uint8_t which;
    uint8_t button;
    uint8_t state;
    uint16_t x, y;
};

struct SDL_ResizeEvent {
    uint8_t type;
    int w;
    int h;
};

union SDL_Event {
    uint8_t type;
    SDL_KeyboardEvent key;
    SDL_MouseMotionEvent motion;
    SDL_MouseButtonEvent button;
    SDL_ResizeEvent resize;
    uint8_t pad[64];
};
static_assert(sizeof(SDL_Event) == 64, "SDL_Event size mismatch");

struct SDL_version {
    uint8_t major;
    uint8_t minor;
    uint8_t patch;
};

struct SDL_SysWMinfo {
    SDL_version version;
    HWND window;
    HGLRC hglrc;
};
static_assert(sizeof(SDL_SysWMinfo) == 12, "SDL_SysWMinfo size mismatch");

enum : uint32_t {
    SDL_SWSURFACE_FLAG  = 0x00000000,
    SDL_HWSURFACE_FLAG  = 0x00000001,
    SDL_OPENGL_FLAG     = 0x00000002,
    SDL_RESIZABLE_FLAG  = 0x00000010,
    SDL_NOFRAME_FLAG    = 0x00000020,
    SDL_FULLSCREEN_FLAG = 0x80000000
};

enum : uint8_t {
    SDL_EV_KEYDOWN         = 2,
    SDL_EV_KEYUP           = 3,
    SDL_EV_MOUSEMOTION     = 4,
    SDL_EV_MOUSEBUTTONDOWN = 5,
    SDL_EV_MOUSEBUTTONUP   = 6,
    SDL_EV_VIDEORESIZE     = 16,
    SDL_EV_VIDEOEXPOSE     = 17
};

enum : int {
    SDL_KEY_RETURN = 13,
    SDL_KEY_F9     = 290,
    SDL_KEY_F10    = 291,
    SDL_KEY_F11    = 292,
    SDL_MOD_ALT    = 0x0300
};

enum : int {
    SDL_ATTR_DOUBLEBUFFER = 5,
    SDL_ATTR_SWAP_CONTROL = 16
};

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

enum DisplayMode {
    MODE_AUTO = 0,
    MODE_WINDOWED = 1,
    MODE_BORDERLESS = 2
};

enum AspectMode {
    ASPECT_FIT = 0,
    ASPECT_INTEGER = 1,
    ASPECT_STRETCH = 2
};

enum FilterMode {
    FILTER_LINEAR = 0,
    FILTER_NEAREST = 1
};

struct ProxyConfig {
    DisplayMode mode = MODE_AUTO;
    int window_width = 0;
    int window_height = 0;
    AspectMode aspect = ASPECT_STRETCH;
    FilterMode filter = FILTER_LINEAR;
    int vsync = 1;
    int unlock_mouse_in_windowed = 1;
    int log = 1;
};

// ---------------------------------------------------------------------------
// Global state & original SDL function pointers
// ---------------------------------------------------------------------------

static HMODULE g_self_module = nullptr;
static HMODULE g_orig_module = nullptr;
static char g_dll_dir[MAX_PATH] = {};
static char g_ini_path[MAX_PATH] = {};
static char g_log_path[MAX_PATH] = {};
static ProxyConfig g_cfg;
static bool g_cfg_loaded = false;
static bool g_dpi_initialized = false;
static bool g_log_reset = false;

static int g_game_w = 800;
static int g_game_h = 600;
static int g_win_w = 800;
static int g_win_h = 600;
static int g_saved_win_w = 0;
static int g_saved_win_h = 0;
static int g_vp_x = 0;
static int g_vp_y = 0;
static int g_vp_w = 800;
static int g_vp_h = 600;

static bool g_is_borderless = false;
static bool g_use_gl = true;
static bool g_gl_use_bgra = true;
static GLuint g_gl_tex = 0;
static int g_tex_w = 0;
static int g_tex_h = 0;
static uint32_t* g_staging_rgba = nullptr;
static int g_staging_pixels = 0;

static SDL_Surface* g_real_screen = nullptr;
static SDL_Surface* g_virtual_screen = nullptr;

static double g_accum_xrel = 0.0;
static double g_accum_yrel = 0.0;

typedef int(__cdecl* pfn_SDL_Init)(uint32_t flags);
typedef int(__cdecl* pfn_SDL_InitSubSystem)(uint32_t flags);
typedef void(__cdecl* pfn_SDL_Quit)(void);
typedef int(__cdecl* pfn_SDL_VideoModeOK)(int w, int h, int bpp, uint32_t flags);
typedef SDL_Surface*(__cdecl* pfn_SDL_SetVideoMode)(int w, int h, int bpp, uint32_t flags);
typedef SDL_Surface*(__cdecl* pfn_SDL_GetVideoSurface)(void);
typedef SDL_Surface*(__cdecl* pfn_SDL_CreateRGBSurface)(uint32_t flags, int w, int h, int depth,
                                                        uint32_t rmask, uint32_t gmask, uint32_t bmask, uint32_t amask);
typedef void(__cdecl* pfn_SDL_FreeSurface)(SDL_Surface* surface);
typedef int(__cdecl* pfn_SDL_Flip)(SDL_Surface* screen);
typedef void(__cdecl* pfn_SDL_UpdateRect)(SDL_Surface* screen, int32_t x, int32_t y, uint32_t w, uint32_t h);
typedef void(__cdecl* pfn_SDL_UpdateRects)(SDL_Surface* screen, int numrects, SDL_Rect* rects);
typedef int(__cdecl* pfn_SDL_PollEvent)(SDL_Event* event);
typedef int(__cdecl* pfn_SDL_WaitEvent)(SDL_Event* event);
typedef int(__cdecl* pfn_SDL_PushEvent)(SDL_Event* event);
typedef void(__cdecl* pfn_SDL_WarpMouse)(uint16_t x, uint16_t y);
typedef uint8_t(__cdecl* pfn_SDL_GetMouseState)(int* x, int* y);
typedef int(__cdecl* pfn_SDL_WM_GrabInput)(int mode);
typedef int(__cdecl* pfn_SDL_WM_ToggleFullScreen)(SDL_Surface* surface);
typedef int(__cdecl* pfn_SDL_GL_SetAttribute)(int attr, int value);
typedef void(__cdecl* pfn_SDL_GL_SwapBuffers)(void);
typedef int(__cdecl* pfn_SDL_GetWMInfo)(SDL_SysWMinfo* info);
typedef int(__cdecl* pfn_SDL_putenv)(const char* variable);
typedef int(__cdecl* pfn_SDL_LockSurface)(SDL_Surface* surface);
typedef void(__cdecl* pfn_SDL_UnlockSurface)(SDL_Surface* surface);
typedef char*(__cdecl* pfn_SDL_GetError)(void);

static pfn_SDL_Init orig_SDL_Init = nullptr;
static pfn_SDL_InitSubSystem orig_SDL_InitSubSystem = nullptr;
static pfn_SDL_Quit orig_SDL_Quit = nullptr;
static pfn_SDL_VideoModeOK orig_SDL_VideoModeOK = nullptr;
static pfn_SDL_SetVideoMode orig_SDL_SetVideoMode = nullptr;
static pfn_SDL_GetVideoSurface orig_SDL_GetVideoSurface = nullptr;
static pfn_SDL_CreateRGBSurface orig_SDL_CreateRGBSurface = nullptr;
static pfn_SDL_FreeSurface orig_SDL_FreeSurface = nullptr;
static pfn_SDL_Flip orig_SDL_Flip = nullptr;
static pfn_SDL_UpdateRect orig_SDL_UpdateRect = nullptr;
static pfn_SDL_UpdateRects orig_SDL_UpdateRects = nullptr;
static pfn_SDL_PollEvent orig_SDL_PollEvent = nullptr;
static pfn_SDL_WaitEvent orig_SDL_WaitEvent = nullptr;
static pfn_SDL_PushEvent orig_SDL_PushEvent = nullptr;
static pfn_SDL_WarpMouse orig_SDL_WarpMouse = nullptr;
static pfn_SDL_GetMouseState orig_SDL_GetMouseState = nullptr;
static pfn_SDL_WM_GrabInput orig_SDL_WM_GrabInput = nullptr;
static pfn_SDL_WM_ToggleFullScreen orig_SDL_WM_ToggleFullScreen = nullptr;
static pfn_SDL_GL_SetAttribute orig_SDL_GL_SetAttribute = nullptr;
static pfn_SDL_GL_SwapBuffers orig_SDL_GL_SwapBuffers = nullptr;
static pfn_SDL_GetWMInfo orig_SDL_GetWMInfo = nullptr;
static pfn_SDL_putenv orig_SDL_putenv = nullptr;
static pfn_SDL_LockSurface orig_SDL_LockSurface = nullptr;
static pfn_SDL_UnlockSurface orig_SDL_UnlockSurface = nullptr;
static pfn_SDL_GetError orig_SDL_GetError = nullptr;

// ---------------------------------------------------------------------------
// Utility & Logging
// ---------------------------------------------------------------------------

static int clamp_int(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void init_paths() {
    if (g_dll_dir[0] != '\0') return;

    char mod_path[MAX_PATH] = {};
    if (g_self_module && GetModuleFileNameA(g_self_module, mod_path, MAX_PATH) > 0) {
        char* slash = strrchr(mod_path, '\\');
        if (!slash) slash = strrchr(mod_path, '/');
        if (slash) {
            *slash = '\0';
            snprintf(g_dll_dir, MAX_PATH, "%s", mod_path);
        }
    }
    if (g_dll_dir[0] == '\0') {
        strcpy_s(g_dll_dir, ".");
    }
    snprintf(g_ini_path, MAX_PATH, "%s\\mytribe_display.ini", g_dll_dir);
    snprintf(g_log_path, MAX_PATH, "%s\\mytribe_display.log", g_dll_dir);
}

static void log_msg(const char* fmt, ...) {
    if (!g_cfg.log) return;
    init_paths();

    FILE* f = fopen(g_log_path, g_log_reset ? "a" : "w");
    if (!f) return;
    g_log_reset = true;

    va_list args;
    va_start(args, fmt);
    vfprintf(f, fmt, args);
    va_end(args);
    fputc('\n', f);
    fclose(f);
}

static void write_default_ini_if_missing() {
    init_paths();
    DWORD attr = GetFileAttributesA(g_ini_path);
    if (attr != INVALID_FILE_ATTRIBUTES) return;

    FILE* f = fopen(g_ini_path, "w");
    if (!f) return;
    fprintf(f,
        "; My Tribe Display Scaling Mod Configuration\n"
        "; Hotkeys in-game:\n"
        ";   F11 or Alt+Enter : Toggle Borderless Fullscreen / Resizable Window\n"
        ";   F9               : Toggle Filter (linear / nearest)\n"
        ";   F10              : Cycle Aspect Mode (fit / integer / stretch)\n"
        "\n"
        "[Display]\n"
        "; mode: auto (follows in-game Full Screen setting using borderless native / large window),\n"
        ";       windowed (always start windowed), borderless (always start borderless fullscreen)\n"
        "mode = auto\n"
        "\n"
        "; Initial window size in windowed mode (0 = auto-size to ~85%% of screen height at 4:3)\n"
        "window_width = 0\n"
        "window_height = 0\n"
        "\n"
        "; aspect: fit (exact 4:3 with black bars), integer (largest integer multiple), stretch\n"
        "aspect = stretch\n"
        "\n"
        "; filter: linear (smooth bilinear) or nearest (crisp pixels)\n"
        "filter = linear\n"
        "\n"
        "; vsync: 1 = enabled, 0 = disabled\n"
        "vsync = 1\n"
        "\n"
        "; unlock_mouse_in_windowed: 1 = allow cursor to leave window freely in windowed mode\n"
        "unlock_mouse_in_windowed = 1\n"
        "\n"
        "; log: 1 = write mytribe_display.log, 0 = disable logging\n"
        "log = 1\n"
    );
    fclose(f);
}

static void load_config() {
    if (g_cfg_loaded) return;
    g_cfg_loaded = true;

    write_default_ini_if_missing();

    char buf[64] = {};
    g_cfg.log = GetPrivateProfileIntA("Display", "log", 1, g_ini_path);

    GetPrivateProfileStringA("Display", "mode", "auto", buf, sizeof(buf), g_ini_path);
    if (_stricmp(buf, "windowed") == 0) g_cfg.mode = MODE_WINDOWED;
    else if (_stricmp(buf, "borderless") == 0) g_cfg.mode = MODE_BORDERLESS;
    else g_cfg.mode = MODE_AUTO;

    g_cfg.window_width = GetPrivateProfileIntA("Display", "window_width", 0, g_ini_path);
    g_cfg.window_height = GetPrivateProfileIntA("Display", "window_height", 0, g_ini_path);

    GetPrivateProfileStringA("Display", "aspect", "stretch", buf, sizeof(buf), g_ini_path);
    if (_stricmp(buf, "fit") == 0) g_cfg.aspect = ASPECT_FIT;
    else if (_stricmp(buf, "integer") == 0) g_cfg.aspect = ASPECT_INTEGER;
    else g_cfg.aspect = ASPECT_STRETCH;

    GetPrivateProfileStringA("Display", "filter", "linear", buf, sizeof(buf), g_ini_path);
    if (_stricmp(buf, "nearest") == 0) g_cfg.filter = FILTER_NEAREST;
    else g_cfg.filter = FILTER_LINEAR;

    g_cfg.vsync = GetPrivateProfileIntA("Display", "vsync", 1, g_ini_path);
    g_cfg.unlock_mouse_in_windowed = GetPrivateProfileIntA("Display", "unlock_mouse_in_windowed", 1, g_ini_path);

    log_msg("Config loaded: mode=%d win=%dx%d aspect=%d filter=%d vsync=%d unlock_mouse=%d",
            (int)g_cfg.mode, g_cfg.window_width, g_cfg.window_height,
            (int)g_cfg.aspect, (int)g_cfg.filter, g_cfg.vsync, g_cfg.unlock_mouse_in_windowed);
}

static void enable_dpi_awareness() {
    if (g_dpi_initialized) return;
    g_dpi_initialized = true;

    HMODULE user32 = GetModuleHandleA("user32.dll");
    if (user32) {
        typedef BOOL(WINAPI* pfn_SetProcessDpiAwarenessContext)(HANDLE);
        auto fn_v2 = (pfn_SetProcessDpiAwarenessContext)GetProcAddress(user32, "SetProcessDpiAwarenessContext");
        if (fn_v2) {
            if (fn_v2((HANDLE)(intptr_t)-4)) {
                log_msg("DPI awareness: Per-Monitor V2 enabled");
                return;
            }
        }
        typedef BOOL(WINAPI* pfn_SetProcessDPIAware)(void);
        auto fn_v1 = (pfn_SetProcessDPIAware)GetProcAddress(user32, "SetProcessDPIAware");
        if (fn_v1) {
            fn_v1();
            log_msg("DPI awareness: System DPI aware enabled");
        }
    }
}

static bool ensure_orig_loaded() {
    if (g_orig_module) return true;
    init_paths();

    char orig_path[MAX_PATH] = {};
    snprintf(orig_path, MAX_PATH, "%s\\SDL_orig.dll", g_dll_dir);
    g_orig_module = LoadLibraryA(orig_path);
    if (!g_orig_module) {
        return false;
    }

    #define RESOLVE_SYM(name) orig_##name = (pfn_##name)GetProcAddress(g_orig_module, #name)
    RESOLVE_SYM(SDL_Init);
    RESOLVE_SYM(SDL_InitSubSystem);
    RESOLVE_SYM(SDL_Quit);
    RESOLVE_SYM(SDL_VideoModeOK);
    RESOLVE_SYM(SDL_SetVideoMode);
    RESOLVE_SYM(SDL_GetVideoSurface);
    RESOLVE_SYM(SDL_CreateRGBSurface);
    RESOLVE_SYM(SDL_FreeSurface);
    RESOLVE_SYM(SDL_Flip);
    RESOLVE_SYM(SDL_UpdateRect);
    RESOLVE_SYM(SDL_UpdateRects);
    RESOLVE_SYM(SDL_PollEvent);
    RESOLVE_SYM(SDL_WaitEvent);
    RESOLVE_SYM(SDL_PushEvent);
    RESOLVE_SYM(SDL_WarpMouse);
    RESOLVE_SYM(SDL_GetMouseState);
    RESOLVE_SYM(SDL_WM_GrabInput);
    RESOLVE_SYM(SDL_WM_ToggleFullScreen);
    RESOLVE_SYM(SDL_GL_SetAttribute);
    RESOLVE_SYM(SDL_GL_SwapBuffers);
    RESOLVE_SYM(SDL_GetWMInfo);
    RESOLVE_SYM(SDL_putenv);
    RESOLVE_SYM(SDL_LockSurface);
    RESOLVE_SYM(SDL_UnlockSurface);
    RESOLVE_SYM(SDL_GetError);
    #undef RESOLVE_SYM

    return orig_SDL_Init && orig_SDL_SetVideoMode && orig_SDL_CreateRGBSurface;
}

// ---------------------------------------------------------------------------
// Viewport, Coordinate Mapping & Monitors
// ---------------------------------------------------------------------------

static HWND get_sdl_hwnd() {
    if (!orig_SDL_GetWMInfo) return nullptr;
    SDL_SysWMinfo info = {};
    info.version.major = 1;
    info.version.minor = 2;
    info.version.patch = 11;
    if (orig_SDL_GetWMInfo(&info) > 0) {
        return info.window;
    }
    return nullptr;
}

static void get_current_monitor_info(MONITORINFO& mi) {
    memset(&mi, 0, sizeof(mi));
    mi.cbSize = sizeof(mi);

    HWND hwnd = get_sdl_hwnd();
    HMONITOR mon = nullptr;
    if (hwnd && IsWindow(hwnd)) {
        mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    } else {
        POINT pt = {0, 0};
        mon = MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY);
    }
    GetMonitorInfoA(mon, &mi);
}

static void recalc_viewport() {
    if (g_win_w <= 0 || g_win_h <= 0 || g_game_w <= 0 || g_game_h <= 0) {
        g_vp_x = 0;
        g_vp_y = 0;
        g_vp_w = (g_win_w > 0) ? g_win_w : 800;
        g_vp_h = (g_win_h > 0) ? g_win_h : 600;
        return;
    }

    if (g_cfg.aspect == ASPECT_STRETCH) {
        g_vp_x = 0;
        g_vp_y = 0;
        g_vp_w = g_win_w;
        g_vp_h = g_win_h;
        return;
    }

    if (g_cfg.aspect == ASPECT_INTEGER) {
        int scale_x = g_win_w / g_game_w;
        int scale_y = g_win_h / g_game_h;
        int scale = (scale_x < scale_y) ? scale_x : scale_y;
        if (scale >= 1) {
            g_vp_w = g_game_w * scale;
            g_vp_h = g_game_h * scale;
            g_vp_x = (g_win_w - g_vp_w) / 2;
            g_vp_y = (g_win_h - g_vp_h) / 2;
            return;
        }
        // If window is smaller than 1x game size, fall through to ASPECT_FIT
    }

    // ASPECT_FIT
    double scale_x = (double)g_win_w / (double)g_game_w;
    double scale_y = (double)g_win_h / (double)g_game_h;
    double scale = (scale_x < scale_y) ? scale_x : scale_y;

    g_vp_w = clamp_int((int)floor(g_game_w * scale + 0.5), 1, g_win_w);
    g_vp_h = clamp_int((int)floor(g_game_h * scale + 0.5), 1, g_win_h);
    g_vp_x = (g_win_w - g_vp_w) / 2;
    g_vp_y = (g_win_h - g_vp_h) / 2;
}

static void map_win_to_game(int wx, int wy, int& gx, int& gy) {
    if (g_vp_w <= 0 || g_vp_h <= 0) {
        gx = clamp_int(wx, 0, g_game_w - 1);
        gy = clamp_int(wy, 0, g_game_h - 1);
        return;
    }
    gx = clamp_int((int)floor((double)(wx - g_vp_x) * (double)g_game_w / (double)g_vp_w), 0, g_game_w - 1);
    gy = clamp_int((int)floor((double)(wy - g_vp_y) * (double)g_game_h / (double)g_vp_h), 0, g_game_h - 1);
}

static void map_game_to_win(int gx, int gy, int& wx, int& wy) {
    if (g_game_w <= 0 || g_game_h <= 0) {
        wx = gx;
        wy = gy;
        return;
    }
    wx = g_vp_x + (int)floor(((double)gx + 0.5) * (double)g_vp_w / (double)g_game_w);
    wy = g_vp_y + (int)floor(((double)gy + 0.5) * (double)g_vp_h / (double)g_game_h);
}

// ---------------------------------------------------------------------------
// OpenGL & GDI Presentation
// ---------------------------------------------------------------------------

static int next_pow2(int v) {
    int p = 1;
    while (p < v) p <<= 1;
    return p;
}

static void init_gl_texture() {
    if (g_gl_tex != 0) {
        glDeleteTextures(1, &g_gl_tex);
        g_gl_tex = 0;
    }

    g_tex_w = next_pow2(g_game_w + 1);
    g_tex_h = next_pow2(g_game_h + 1);

    glGenTextures(1, &g_gl_tex);
    glBindTexture(GL_TEXTURE_2D, g_gl_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    GLint filt = (g_cfg.filter == FILTER_NEAREST) ? GL_NEAREST : GL_LINEAR;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filt);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filt);

    // Clear error state before checking GL_BGRA_EXT support
    while (glGetError() != GL_NO_ERROR) {}

    size_t zero_bytes = (size_t)g_tex_w * (size_t)g_tex_h * 4;
    uint8_t* zeros = (uint8_t*)calloc(1, zero_bytes);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, g_tex_w, g_tex_h, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, zeros);
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        g_gl_use_bgra = false;
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, g_tex_w, g_tex_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, zeros);
        log_msg("OpenGL: GL_BGRA_EXT not accepted (err=0x%X), using RGBA staging swap", (unsigned)err);
    } else {
        g_gl_use_bgra = true;
    }
    free(zeros);

    log_msg("OpenGL texture initialized: %dx%d (bgra=%d)", g_tex_w, g_tex_h, g_gl_use_bgra ? 1 : 0);
}

static void present_frame_gl(const uint8_t* src_pixels, int src_pitch) {
    if (g_gl_tex == 0) {
        init_gl_texture();
    }

    glViewport(0, 0, g_win_w, g_win_h);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glBindTexture(GL_TEXTURE_2D, g_gl_tex);
    GLint filt = (g_cfg.filter == FILTER_NEAREST) ? GL_NEAREST : GL_LINEAR;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filt);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filt);

    uint32_t rmask = g_virtual_screen->format ? g_virtual_screen->format->Rmask : 0x00FF0000;
    uint32_t bmask = g_virtual_screen->format ? g_virtual_screen->format->Bmask : 0x000000FF;

    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    if (rmask == 0x000000FF && bmask == 0x00FF0000) {
        // Memory bytes are R, G, B, X -> direct GL_RGBA upload
        int row_pixels = src_pitch / 4;
        glPixelStorei(GL_UNPACK_ROW_LENGTH, row_pixels);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, g_game_w, g_game_h, GL_RGBA, GL_UNSIGNED_BYTE, src_pixels);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, g_game_h, g_game_w, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                        src_pixels + (g_game_h - 1) * src_pitch);
        glTexSubImage2D(GL_TEXTURE_2D, 0, g_game_w, 0, 1, g_game_h, GL_RGBA, GL_UNSIGNED_BYTE,
                        src_pixels + (g_game_w - 1) * 4);
        glTexSubImage2D(GL_TEXTURE_2D, 0, g_game_w, g_game_h, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                        src_pixels + (g_game_h - 1) * src_pitch + (g_game_w - 1) * 4);
    } else if (rmask == 0x00FF0000 && bmask == 0x000000FF && g_gl_use_bgra) {
        // Memory bytes are B, G, R, X -> direct GL_BGRA_EXT upload
        int row_pixels = src_pitch / 4;
        glPixelStorei(GL_UNPACK_ROW_LENGTH, row_pixels);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, g_game_w, g_game_h, GL_BGRA_EXT, GL_UNSIGNED_BYTE, src_pixels);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, g_game_h, g_game_w, 1, GL_BGRA_EXT, GL_UNSIGNED_BYTE,
                        src_pixels + (g_game_h - 1) * src_pitch);
        glTexSubImage2D(GL_TEXTURE_2D, 0, g_game_w, 0, 1, g_game_h, GL_BGRA_EXT, GL_UNSIGNED_BYTE,
                        src_pixels + (g_game_w - 1) * 4);
        glTexSubImage2D(GL_TEXTURE_2D, 0, g_game_w, g_game_h, 1, 1, GL_BGRA_EXT, GL_UNSIGNED_BYTE,
                        src_pixels + (g_game_h - 1) * src_pitch + (g_game_w - 1) * 4);
    } else {
        // Software R<->B swap staging fallback
        int needed = g_game_w * g_game_h;
        if (needed > g_staging_pixels) {
            free(g_staging_rgba);
            g_staging_rgba = (uint32_t*)malloc((size_t)needed * 4);
            g_staging_pixels = needed;
        }
        for (int y = 0; y < g_game_h; ++y) {
            const uint32_t* row = (const uint32_t*)(src_pixels + y * src_pitch);
            uint32_t* dst = g_staging_rgba + y * g_game_w;
            for (int x = 0; x < g_game_w; ++x) {
                uint32_t px = row[x];
                dst[x] = ((px & 0x00FF0000u) >> 16) | (px & 0xFF00FF00u) | ((px & 0x000000FFu) << 16);
            }
        }
        glPixelStorei(GL_UNPACK_ROW_LENGTH, g_game_w);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, g_game_w, g_game_h, GL_RGBA, GL_UNSIGNED_BYTE, g_staging_rgba);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, g_game_h, g_game_w, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                        g_staging_rgba + (g_game_h - 1) * g_game_w);
        glTexSubImage2D(GL_TEXTURE_2D, 0, g_game_w, 0, 1, g_game_h, GL_RGBA, GL_UNSIGNED_BYTE,
                        g_staging_rgba + (g_game_w - 1));
        glTexSubImage2D(GL_TEXTURE_2D, 0, g_game_w, g_game_h, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                        g_staging_rgba + (g_game_h - 1) * g_game_w + (g_game_w - 1));
    }
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, (double)g_win_w, (double)g_win_h, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_ALPHA_TEST);
    glEnable(GL_TEXTURE_2D);

    float u1 = (float)g_game_w / (float)g_tex_w;
    float v1 = (float)g_game_h / (float)g_tex_h;
    float x0 = (float)g_vp_x;
    float y0 = (float)g_vp_y;
    float x1 = (float)(g_vp_x + g_vp_w);
    float y1 = (float)(g_vp_y + g_vp_h);

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex2f(x0, y0);
    glTexCoord2f(u1,   0.0f); glVertex2f(x1, y0);
    glTexCoord2f(u1,   v1);   glVertex2f(x1, y1);
    glTexCoord2f(0.0f, v1);   glVertex2f(x0, y1);
    glEnd();

    orig_SDL_GL_SwapBuffers();
}

static void present_frame_gdi(const uint8_t* src_pixels, int src_pitch) {
    HWND hwnd = get_sdl_hwnd();
    if (!hwnd) return;

    HDC hdc = GetDC(hwnd);
    if (!hdc) return;

    HBRUSH black = (HBRUSH)GetStockObject(BLACK_BRUSH);
    if (g_vp_y > 0) {
        RECT top_bar = {0, 0, g_win_w, g_vp_y};
        RECT bot_bar = {0, g_vp_y + g_vp_h, g_win_w, g_win_h};
        FillRect(hdc, &top_bar, black);
        FillRect(hdc, &bot_bar, black);
    }
    if (g_vp_x > 0) {
        RECT left_bar = {0, g_vp_y, g_vp_x, g_vp_y + g_vp_h};
        RECT right_bar = {g_vp_x + g_vp_w, g_vp_y, g_win_w, g_vp_y + g_vp_h};
        FillRect(hdc, &left_bar, black);
        FillRect(hdc, &right_bar, black);
    }

    struct {
        BITMAPINFOHEADER bmiHeader;
        DWORD bmiColors[3];
    } bmi = {};

    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = src_pitch / 4;
    bmi.bmiHeader.biHeight = -g_game_h;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_BITFIELDS;

    if (g_virtual_screen->format && g_virtual_screen->format->Rmask) {
        bmi.bmiColors[0] = g_virtual_screen->format->Rmask;
        bmi.bmiColors[1] = g_virtual_screen->format->Gmask;
        bmi.bmiColors[2] = g_virtual_screen->format->Bmask;
    } else {
        bmi.bmiColors[0] = 0x00FF0000;
        bmi.bmiColors[1] = 0x0000FF00;
        bmi.bmiColors[2] = 0x000000FF;
    }

    if (g_cfg.filter == FILTER_LINEAR) {
        SetStretchBltMode(hdc, HALFTONE);
        SetBrushOrgEx(hdc, 0, 0, nullptr);
    } else {
        SetStretchBltMode(hdc, COLORONCOLOR);
    }

    StretchDIBits(hdc,
                  g_vp_x, g_vp_y, g_vp_w, g_vp_h,
                  0, 0, g_game_w, g_game_h,
                  src_pixels, (const BITMAPINFO*)&bmi, DIB_RGB_COLORS, SRCCOPY);

    ReleaseDC(hwnd, hdc);
}

static void present_frame() {
    if (!g_virtual_screen || !g_real_screen) return;

    bool need_lock = ((g_virtual_screen->flags & 0x00004000u) != 0); // SDL_RLEACCEL or hw lock
    if (need_lock && orig_SDL_LockSurface) {
        if (orig_SDL_LockSurface(g_virtual_screen) < 0) return;
    }

    const uint8_t* pixels = (const uint8_t*)g_virtual_screen->pixels;
    int pitch = g_virtual_screen->pitch;
    if (pixels && pitch > 0) {
        if (g_use_gl) {
            present_frame_gl(pixels, pitch);
        } else {
            present_frame_gdi(pixels, pitch);
        }
    }

    if (need_lock && orig_SDL_UnlockSurface) {
        orig_SDL_UnlockSurface(g_virtual_screen);
    }
}

// ---------------------------------------------------------------------------
// Video Mode Switching
// ---------------------------------------------------------------------------

static bool apply_video_mode(bool borderless, int req_win_w, int req_win_h, bool is_resize) {
    MONITORINFO mi;
    get_current_monitor_info(mi);

    int mon_x = mi.rcMonitor.left;
    int mon_y = mi.rcMonitor.top;
    int mon_w = mi.rcMonitor.right - mi.rcMonitor.left;
    int mon_h = mi.rcMonitor.bottom - mi.rcMonitor.top;

    int target_w = g_game_w;
    int target_h = g_game_h;

    if (borderless) {
        target_w = mon_w;
        target_h = mon_h;
        char pos_env[64];
        snprintf(pos_env, sizeof(pos_env), "SDL_VIDEO_WINDOW_POS=%d,%d", mon_x, mon_y);
        if (orig_SDL_putenv) {
            orig_SDL_putenv(pos_env);
            orig_SDL_putenv("SDL_VIDEO_CENTERED=");
        }
    } else {
        if (req_win_w > 0 && req_win_h > 0) {
            target_w = req_win_w;
            target_h = req_win_h;
        } else if (g_saved_win_w > 0 && g_saved_win_h > 0) {
            target_w = g_saved_win_w;
            target_h = g_saved_win_h;
        } else if (g_cfg.window_width > 0 && g_cfg.window_height > 0) {
            target_w = g_cfg.window_width;
            target_h = g_cfg.window_height;
        } else {
            int work_w = mi.rcWork.right - mi.rcWork.left;
            int work_h = mi.rcWork.bottom - mi.rcWork.top;
            double aspect = (double)g_game_w / (double)g_game_h;
            target_h = (int)floor(work_h * 0.85 + 0.5);
            target_w = (int)floor(target_h * aspect + 0.5);
            if (target_w > (int)(work_w * 0.90)) {
                target_w = (int)floor(work_w * 0.90 + 0.5);
                target_h = (int)floor(target_w / aspect + 0.5);
            }
            if (target_w < g_game_w) target_w = g_game_w;
            if (target_h < g_game_h) target_h = g_game_h;
        }

        if (target_w < 320) target_w = 320;
        if (target_h < 240) target_h = 240;

        if (!is_resize && orig_SDL_putenv) {
            orig_SDL_putenv("SDL_VIDEO_WINDOW_POS=");
            orig_SDL_putenv("SDL_VIDEO_CENTERED=1");
        }
    }

    if (g_use_gl && g_gl_tex != 0) {
        glDeleteTextures(1, &g_gl_tex);
        g_gl_tex = 0;
    }

    if (orig_SDL_GL_SetAttribute) {
        orig_SDL_GL_SetAttribute(SDL_ATTR_DOUBLEBUFFER, 1);
        orig_SDL_GL_SetAttribute(SDL_ATTR_SWAP_CONTROL, g_cfg.vsync ? 1 : 0);
    }

    uint32_t sdl_flags = SDL_OPENGL_FLAG | (borderless ? SDL_NOFRAME_FLAG : SDL_RESIZABLE_FLAG);
    SDL_Surface* real_surf = orig_SDL_SetVideoMode(target_w, target_h, 32, sdl_flags);
    if (real_surf) {
        g_use_gl = true;
    } else {
        const char* err = orig_SDL_GetError ? orig_SDL_GetError() : "unknown";
        log_msg("SDL_OPENGL SetVideoMode(%dx%d) failed (%s); falling back to GDI SWSURFACE", target_w, target_h, err);
        g_use_gl = false;
        sdl_flags = SDL_SWSURFACE_FLAG | (borderless ? SDL_NOFRAME_FLAG : SDL_RESIZABLE_FLAG);
        real_surf = orig_SDL_SetVideoMode(target_w, target_h, 32, sdl_flags);
    }

    if (!real_surf) {
        log_msg("CRITICAL: orig_SDL_SetVideoMode(%dx%d) failed completely", target_w, target_h);
        return false;
    }

    g_real_screen = real_surf;
    g_is_borderless = borderless;

    HWND hwnd = get_sdl_hwnd();
    if (hwnd && IsWindow(hwnd)) {
        LONG style = GetWindowLongA(hwnd, GWL_STYLE);
        if (borderless) {
            style &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU | WS_BORDER);
            style |= (WS_POPUP | WS_VISIBLE);
            SetWindowLongA(hwnd, GWL_STYLE, style);
            SetWindowPos(hwnd, HWND_TOP, mon_x, mon_y, target_w, target_h, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        } else if (!is_resize) {
            // Ensure standard resizable frame styles when leaving borderless or creating window
            style &= ~WS_POPUP;
            style |= (WS_OVERLAPPEDWINDOW | WS_VISIBLE);
            SetWindowLongA(hwnd, GWL_STYLE, style);
            RECT wr = {0, 0, target_w, target_h};
            AdjustWindowRect(&wr, (DWORD)style, FALSE);
            int win_outer_w = wr.right - wr.left;
            int win_outer_h = wr.bottom - wr.top;
            int work_x = mi.rcWork.left;
            int work_y = mi.rcWork.top;
            int work_w = mi.rcWork.right - mi.rcWork.left;
            int work_h = mi.rcWork.bottom - mi.rcWork.top;
            int cx = work_x + (work_w - win_outer_w) / 2;
            int cy = work_y + (work_h - win_outer_h) / 2;
            if (cx < work_x) cx = work_x;
            if (cy < work_y) cy = work_y;
            SetWindowPos(hwnd, HWND_NOTOPMOST, cx, cy, win_outer_w, win_outer_h, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        }

        RECT client_rc = {};
        if (GetClientRect(hwnd, &client_rc)) {
            g_win_w = client_rc.right - client_rc.left;
            g_win_h = client_rc.bottom - client_rc.top;
        } else {
            g_win_w = target_w;
            g_win_h = target_h;
        }
    } else {
        g_win_w = target_w;
        g_win_h = target_h;
    }

    if (!borderless) {
        g_saved_win_w = g_win_w;
        g_saved_win_h = g_win_h;
    }

    if (g_use_gl) {
        init_gl_texture();
    }

    recalc_viewport();

    log_msg("apply_video_mode: borderless=%d req=%dx%d client=%dx%d viewport=(%d,%d %dx%d) gl=%d",
            borderless ? 1 : 0, target_w, target_h, g_win_w, g_win_h,
            g_vp_x, g_vp_y, g_vp_w, g_vp_h, g_use_gl ? 1 : 0);
    return true;
}

// ---------------------------------------------------------------------------
// Exported Hook Functions
// ---------------------------------------------------------------------------

extern "C" {

int __cdecl SDL_Init(uint32_t flags) {
    if (!ensure_orig_loaded()) return -1;
    load_config();
    enable_dpi_awareness();
    if (orig_SDL_putenv) {
        orig_SDL_putenv("SDL_VIDEODRIVER=windib");
    }
    int rc = orig_SDL_Init(flags);
    log_msg("SDL_Init(0x%X) -> %d", flags, rc);
    return rc;
}

int __cdecl SDL_InitSubSystem(uint32_t flags) {
    if (!ensure_orig_loaded()) return -1;
    load_config();
    enable_dpi_awareness();
    if (orig_SDL_putenv) {
        orig_SDL_putenv("SDL_VIDEODRIVER=windib");
    }
    int rc = orig_SDL_InitSubSystem(flags);
    log_msg("SDL_InitSubSystem(0x%X) -> %d", flags, rc);
    return rc;
}

void __cdecl SDL_Quit(void) {
    if (!ensure_orig_loaded()) return;
    log_msg("SDL_Quit()");
    if (g_use_gl && g_gl_tex != 0) {
        glDeleteTextures(1, &g_gl_tex);
        g_gl_tex = 0;
    }
    free(g_staging_rgba);
    g_staging_rgba = nullptr;
    g_staging_pixels = 0;

    if (g_virtual_screen && orig_SDL_FreeSurface) {
        orig_SDL_FreeSurface(g_virtual_screen);
        g_virtual_screen = nullptr;
    }
    g_real_screen = nullptr;

    if (orig_SDL_Quit) {
        orig_SDL_Quit();
    }
}

int __cdecl SDL_VideoModeOK(int width, int height, int bpp, uint32_t flags) {
    if (!ensure_orig_loaded()) return 0;
    if (orig_SDL_VideoModeOK) {
        int res = orig_SDL_VideoModeOK(width, height, bpp > 0 ? bpp : 32, flags & ~SDL_FULLSCREEN_FLAG);
        if (res > 0) return res;
    }
    return (bpp > 0) ? bpp : 32;
}

SDL_Surface* __cdecl SDL_SetVideoMode(int width, int height, int bpp, uint32_t flags) {
    if (!ensure_orig_loaded()) return nullptr;
    load_config();

    if (width <= 0) width = 800;
    if (height <= 0) height = 600;
    g_game_w = width;
    g_game_h = height;

    bool borderless = false;
    if (g_cfg.mode == MODE_BORDERLESS) borderless = true;
    else if (g_cfg.mode == MODE_WINDOWED) borderless = false;
    else borderless = ((flags & SDL_FULLSCREEN_FLAG) != 0);

    log_msg("SDL_SetVideoMode(%dx%d, bpp=%d, flags=0x%X) -> borderless=%d",
            width, height, bpp, flags, borderless ? 1 : 0);

    if (!apply_video_mode(borderless, 0, 0, false)) {
        return nullptr;
    }

    uint32_t rmask = 0x00FF0000, gmask = 0x0000FF00, bmask = 0x000000FF;
    if (g_real_screen && g_real_screen->format &&
        (g_real_screen->format->Rmask || g_real_screen->format->Gmask || g_real_screen->format->Bmask)) {
        rmask = g_real_screen->format->Rmask;
        gmask = g_real_screen->format->Gmask;
        bmask = g_real_screen->format->Bmask;
    }

    bool need_new_virtual = true;
    if (g_virtual_screen && g_virtual_screen->w == width && g_virtual_screen->h == height &&
        g_virtual_screen->format &&
        g_virtual_screen->format->BitsPerPixel == 32 &&
        g_virtual_screen->format->Rmask == rmask &&
        g_virtual_screen->format->Gmask == gmask &&
        g_virtual_screen->format->Bmask == bmask) {
        need_new_virtual = false;
    }

    if (need_new_virtual) {
        if (g_virtual_screen && orig_SDL_FreeSurface) {
            orig_SDL_FreeSurface(g_virtual_screen);
            g_virtual_screen = nullptr;
        }
        g_virtual_screen = orig_SDL_CreateRGBSurface(SDL_SWSURFACE_FLAG, width, height, 32, rmask, gmask, bmask, 0);
        if (g_virtual_screen && g_virtual_screen->format) {
            log_msg("Virtual screen created: ptr=%p %dx%d pitch=%d masks=(%08X,%08X,%08X,%08X)",
                    (void*)g_virtual_screen, g_virtual_screen->w, g_virtual_screen->h, g_virtual_screen->pitch,
                    g_virtual_screen->format->Rmask, g_virtual_screen->format->Gmask,
                    g_virtual_screen->format->Bmask, g_virtual_screen->format->Amask);
        }
    } else {
        log_msg("Virtual screen preserved: ptr=%p", (void*)g_virtual_screen);
    }

    present_frame();
    return g_virtual_screen;
}

SDL_Surface* __cdecl SDL_GetVideoSurface(void) {
    if (g_virtual_screen) return g_virtual_screen;
    if (!ensure_orig_loaded() || !orig_SDL_GetVideoSurface) return nullptr;
    return orig_SDL_GetVideoSurface();
}

void __cdecl SDL_FreeSurface(SDL_Surface* surface) {
    if (!surface) return;
    if (surface == g_virtual_screen || surface == g_real_screen) {
        return;
    }
    if (ensure_orig_loaded() && orig_SDL_FreeSurface) {
        orig_SDL_FreeSurface(surface);
    }
}

int __cdecl SDL_Flip(SDL_Surface* screen) {
    if (screen == g_virtual_screen || screen == nullptr) {
        present_frame();
        return 0;
    }
    if (!ensure_orig_loaded() || !orig_SDL_Flip) return -1;
    return orig_SDL_Flip(screen);
}

void __cdecl SDL_UpdateRect(SDL_Surface* screen, int32_t x, int32_t y, uint32_t w, uint32_t h) {
    if (screen == g_virtual_screen) {
        present_frame();
        return;
    }
    if (ensure_orig_loaded() && orig_SDL_UpdateRect) {
        orig_SDL_UpdateRect(screen, x, y, w, h);
    }
}

void __cdecl SDL_UpdateRects(SDL_Surface* screen, int numrects, SDL_Rect* rects) {
    if (screen == g_virtual_screen) {
        if (numrects > 0) present_frame();
        return;
    }
    if (ensure_orig_loaded() && orig_SDL_UpdateRects) {
        orig_SDL_UpdateRects(screen, numrects, rects);
    }
}

static bool handle_polled_event(SDL_Event* ev) {
    switch (ev->type) {
        case SDL_EV_MOUSEMOTION: {
            int gx = 0, gy = 0;
            map_win_to_game(ev->motion.x, ev->motion.y, gx, gy);
            ev->motion.x = (uint16_t)gx;
            ev->motion.y = (uint16_t)gy;

            if (g_vp_w > 0 && g_vp_h > 0) {
                g_accum_xrel += (double)ev->motion.xrel * ((double)g_game_w / (double)g_vp_w);
                g_accum_yrel += (double)ev->motion.yrel * ((double)g_game_h / (double)g_vp_h);
                int rx = (int)trunc(g_accum_xrel);
                int ry = (int)trunc(g_accum_yrel);
                g_accum_xrel -= rx;
                g_accum_yrel -= ry;
                ev->motion.xrel = (int16_t)clamp_int(rx, -32768, 32767);
                ev->motion.yrel = (int16_t)clamp_int(ry, -32768, 32767);
            }
            return true;
        }
        case SDL_EV_MOUSEBUTTONDOWN:
        case SDL_EV_MOUSEBUTTONUP: {
            int gx = 0, gy = 0;
            map_win_to_game(ev->button.x, ev->button.y, gx, gy);
            ev->button.x = (uint16_t)gx;
            ev->button.y = (uint16_t)gy;
            return true;
        }
        case SDL_EV_VIDEORESIZE: {
            if (!g_is_borderless) {
                int rw = clamp_int(ev->resize.w, 320, 16384);
                int rh = clamp_int(ev->resize.h, 240, 16384);
                if (rw != g_win_w || rh != g_win_h) {
                    g_saved_win_w = rw;
                    g_saved_win_h = rh;
                    apply_video_mode(false, rw, rh, true);
                    present_frame();
                }
            }
            return false; // consume so game does not reset video mode
        }
        case SDL_EV_VIDEOEXPOSE: {
            present_frame();
            return true;
        }
        case SDL_EV_KEYDOWN: {
            int sym = ev->key.keysym.sym;
            int mod = ev->key.keysym.mod;
            if (sym == SDL_KEY_F11 || (sym == SDL_KEY_RETURN && (mod & SDL_MOD_ALT) != 0)) {
                apply_video_mode(!g_is_borderless, 0, 0, false);
                present_frame();
                return false;
            }
            if (sym == SDL_KEY_F9) {
                g_cfg.filter = (g_cfg.filter == FILTER_LINEAR) ? FILTER_NEAREST : FILTER_LINEAR;
                log_msg("Hotkey F9: filter=%d", (int)g_cfg.filter);
                present_frame();
                return false;
            }
            if (sym == SDL_KEY_F10) {
                g_cfg.aspect = (AspectMode)(((int)g_cfg.aspect + 1) % 3);
                log_msg("Hotkey F10: aspect=%d", (int)g_cfg.aspect);
                recalc_viewport();
                present_frame();
                return false;
            }
            return true;
        }
        default:
            return true;
    }
}

int __cdecl SDL_PollEvent(SDL_Event* event) {
    if (!ensure_orig_loaded() || !orig_SDL_PollEvent) return 0;
    if (!event) return orig_SDL_PollEvent(nullptr);

    int rc = 0;
    while ((rc = orig_SDL_PollEvent(event)) > 0) {
        if (handle_polled_event(event)) {
            return rc;
        }
    }
    return rc;
}

int __cdecl SDL_WaitEvent(SDL_Event* event) {
    if (!ensure_orig_loaded() || !orig_SDL_WaitEvent) return 0;
    if (!event) return orig_SDL_WaitEvent(nullptr);

    int rc = 0;
    while ((rc = orig_SDL_WaitEvent(event)) == 1) {
        if (handle_polled_event(event)) {
            return rc;
        }
    }
    return rc;
}

int __cdecl SDL_PushEvent(SDL_Event* event) {
    if (!ensure_orig_loaded() || !orig_SDL_PushEvent) return -1;
    if (!event) return orig_SDL_PushEvent(nullptr);

    if (event->type == SDL_EV_MOUSEMOTION) {
        SDL_Event copy = {};
        memcpy(&copy.motion, &event->motion, sizeof(SDL_MouseMotionEvent));
        int wx = 0, wy = 0;
        map_game_to_win(copy.motion.x, copy.motion.y, wx, wy);
        copy.motion.x = (uint16_t)clamp_int(wx, 0, 65535);
        copy.motion.y = (uint16_t)clamp_int(wy, 0, 65535);
        if (g_game_w > 0 && g_game_h > 0) {
            copy.motion.xrel = (int16_t)clamp_int((int)floor((double)copy.motion.xrel * g_vp_w / g_game_w + 0.5), -32768, 32767);
            copy.motion.yrel = (int16_t)clamp_int((int)floor((double)copy.motion.yrel * g_vp_h / g_game_h + 0.5), -32768, 32767);
        }
        return orig_SDL_PushEvent(&copy);
    }

    if (event->type == SDL_EV_MOUSEBUTTONDOWN || event->type == SDL_EV_MOUSEBUTTONUP) {
        SDL_Event copy = {};
        memcpy(&copy.button, &event->button, sizeof(SDL_MouseButtonEvent));
        int wx = 0, wy = 0;
        map_game_to_win(copy.button.x, copy.button.y, wx, wy);
        copy.button.x = (uint16_t)clamp_int(wx, 0, 65535);
        copy.button.y = (uint16_t)clamp_int(wy, 0, 65535);
        return orig_SDL_PushEvent(&copy);
    }

    return orig_SDL_PushEvent(event);
}

void __cdecl SDL_WarpMouse(uint16_t x, uint16_t y) {
    if (!ensure_orig_loaded() || !orig_SDL_WarpMouse) return;
    int wx = 0, wy = 0;
    map_game_to_win(x, y, wx, wy);
    orig_SDL_WarpMouse((uint16_t)clamp_int(wx, 0, 65535), (uint16_t)clamp_int(wy, 0, 65535));
}

uint8_t __cdecl SDL_GetMouseState(int* x, int* y) {
    if (!ensure_orig_loaded() || !orig_SDL_GetMouseState) return 0;
    int raw_x = 0, raw_y = 0;
    uint8_t buttons = orig_SDL_GetMouseState(&raw_x, &raw_y);
    int gx = 0, gy = 0;
    map_win_to_game(raw_x, raw_y, gx, gy);
    if (x) *x = gx;
    if (y) *y = gy;
    return buttons;
}

int __cdecl SDL_WM_GrabInput(int mode) {
    if (!g_is_borderless && g_cfg.unlock_mouse_in_windowed && mode == 1) {
        return 0; // SDL_GRAB_OFF
    }
    if (!ensure_orig_loaded() || !orig_SDL_WM_GrabInput) return 0;
    return orig_SDL_WM_GrabInput(mode);
}

int __cdecl SDL_WM_ToggleFullScreen(SDL_Surface* surface) {
    (void)surface;
    if (!ensure_orig_loaded()) return 0;
    if (!apply_video_mode(!g_is_borderless, 0, 0, false)) return 0;
    present_frame();
    return 1;
}

} // extern "C"

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    (void)lpvReserved;
    if (fdwReason == DLL_PROCESS_ATTACH) {
        g_self_module = hinstDLL;
        DisableThreadLibraryCalls(hinstDLL);
    }
    return TRUE;
}
