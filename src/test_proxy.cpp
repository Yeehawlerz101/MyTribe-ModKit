#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>

struct SDL_Rect {
    int16_t x, y;
    uint16_t w, h;
};

struct SDL_PixelFormat {
    void* palette;
    uint8_t BitsPerPixel;
    uint8_t BytesPerPixel;
    uint8_t Rloss, Gloss, Bloss, Aloss;
    uint8_t Rshift, Gshift, Bshift, Ashift;
    uint32_t Rmask, Gmask, Bmask, Amask, colorkey;
    uint8_t alpha;
};

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

struct SDL_MouseMotionEvent {
    uint8_t type;
    uint8_t which;
    uint8_t state;
    uint16_t x, y;
    int16_t xrel, yrel;
};

struct SDL_MouseButtonEvent {
    uint8_t type;
    uint8_t which;
    uint8_t button;
    uint8_t state;
    uint16_t x, y;
};

union SDL_Event {
    uint8_t type;
    SDL_MouseMotionEvent motion;
    SDL_MouseButtonEvent button;
    uint8_t pad[64];
};

static int fail(const char* msg) {
    fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
}

int main() {
    HMODULE sdl = LoadLibraryA("SDL.dll");
    if (!sdl) return fail("LoadLibraryA(SDL.dll) failed");

    HMODULE img = LoadLibraryA("SDL_image.dll");
    if (!img) return fail("LoadLibraryA(SDL_image.dll) failed");
    if (!GetProcAddress(img, "IMG_LoadTyped_RW")) return fail("Missing IMG_LoadTyped_RW");

    HMODULE ttf = LoadLibraryA("SDL_ttf.dll");
    if (!ttf) return fail("LoadLibraryA(SDL_ttf.dll) failed");

    auto pSDL_Init = (int(__cdecl*)(uint32_t))GetProcAddress(sdl, "SDL_Init");
    auto pSDL_Quit = (void(__cdecl*)(void))GetProcAddress(sdl, "SDL_Quit");
    auto pSDL_VideoModeOK = (int(__cdecl*)(int, int, int, uint32_t))GetProcAddress(sdl, "SDL_VideoModeOK");
    auto pSDL_SetVideoMode = (SDL_Surface*(__cdecl*)(int, int, int, uint32_t))GetProcAddress(sdl, "SDL_SetVideoMode");
    auto pSDL_GetVideoSurface = (SDL_Surface*(__cdecl*)(void))GetProcAddress(sdl, "SDL_GetVideoSurface");
    auto pSDL_CreateRGBSurface = (SDL_Surface*(__cdecl*)(uint32_t, int, int, int, uint32_t, uint32_t, uint32_t, uint32_t))
        GetProcAddress(sdl, "SDL_CreateRGBSurface");
    auto pSDL_DisplayFormat = (SDL_Surface*(__cdecl*)(SDL_Surface*))GetProcAddress(sdl, "SDL_DisplayFormat");
    auto pSDL_FreeSurface = (void(__cdecl*)(SDL_Surface*))GetProcAddress(sdl, "SDL_FreeSurface");
    auto pSDL_FillRect = (int(__cdecl*)(SDL_Surface*, SDL_Rect*, uint32_t))GetProcAddress(sdl, "SDL_FillRect");
    auto pSDL_UpperBlit = (int(__cdecl*)(SDL_Surface*, SDL_Rect*, SDL_Surface*, SDL_Rect*))GetProcAddress(sdl, "SDL_UpperBlit");
    auto pSDL_Flip = (int(__cdecl*)(SDL_Surface*))GetProcAddress(sdl, "SDL_Flip");
    auto pSDL_UpdateRect = (void(__cdecl*)(SDL_Surface*, int32_t, int32_t, uint32_t, uint32_t))GetProcAddress(sdl, "SDL_UpdateRect");
    auto pSDL_PollEvent = (int(__cdecl*)(SDL_Event*))GetProcAddress(sdl, "SDL_PollEvent");
    auto pSDL_PushEvent = (int(__cdecl*)(SDL_Event*))GetProcAddress(sdl, "SDL_PushEvent");
    auto pSDL_WM_GrabInput = (int(__cdecl*)(int))GetProcAddress(sdl, "SDL_WM_GrabInput");

    auto pTTF_Init = (int(__cdecl*)(void))GetProcAddress(ttf, "TTF_Init");
    auto pTTF_Quit = (void(__cdecl*)(void))GetProcAddress(ttf, "TTF_Quit");

    if (!pSDL_Init || !pSDL_SetVideoMode || !pSDL_Flip || !pTTF_Init || !pTTF_Quit) {
        return fail("Missing required exports");
    }

    // 1. SDL_Init
    if (pSDL_Init(0x00000020u) != 0) return fail("SDL_Init(SDL_INIT_VIDEO) failed");
    if (GetFileAttributesA("mytribe_display.ini") == INVALID_FILE_ATTRIBUTES) {
        return fail("mytribe_display.ini was not created");
    }
    if (GetFileAttributesA("mytribe_display.log") == INVALID_FILE_ATTRIBUTES) {
        return fail("mytribe_display.log was not created");
    }
    printf("[PASS] SDL_Init created mytribe_display.ini and mytribe_display.log\n");

    // 2. SDL_VideoModeOK
    int bpp = pSDL_VideoModeOK(800, 600, 32, 0x80000000u);
    if (bpp != 32) return fail("SDL_VideoModeOK(800,600,32,SDL_FULLSCREEN) did not return 32");
    printf("[PASS] SDL_VideoModeOK returned %d\n", bpp);

    // 3. Windowed SetVideoMode
    SDL_Surface* screen = pSDL_SetVideoMode(800, 600, 32, 0);
    if (!screen || !screen->format) return fail("Windowed SDL_SetVideoMode returned NULL");
    if (screen->w != 800 || screen->h != 600 || screen->format->BitsPerPixel != 32) {
        return fail("Virtual screen dimensions/bpp mismatch");
    }
    if (pSDL_GetVideoSurface() != screen) return fail("SDL_GetVideoSurface mismatch");
    printf("[PASS] Windowed virtual_screen=%p (%dx%d, %dbpp)\n", (void*)screen, screen->w, screen->h, screen->format->BitsPerPixel);

    // 4. CreateRGBSurface + DisplayFormat + UpperBlit + Flip + UpdateRect
    SDL_Surface* tmp = pSDL_CreateRGBSurface(0, 64, 64, 32, 0x00FF0000u, 0x0000FF00u, 0x000000FFu, 0);
    if (!tmp) return fail("SDL_CreateRGBSurface failed");
    SDL_Surface* disp = pSDL_DisplayFormat(tmp);
    if (!disp || !disp->format) return fail("SDL_DisplayFormat failed");
    if (disp->format->Rmask != screen->format->Rmask ||
        disp->format->Gmask != screen->format->Gmask ||
        disp->format->Bmask != screen->format->Bmask) {
        return fail("SDL_DisplayFormat pixel masks do not match virtual_screen");
    }
    pSDL_FillRect(disp, nullptr, 0x003388CCu);
    pSDL_FillRect(screen, nullptr, 0x00112233u);
    SDL_Rect dst = {100, 100, 64, 64};
    if (pSDL_UpperBlit(disp, nullptr, screen, &dst) != 0) return fail("SDL_UpperBlit failed");
    if (pSDL_Flip(screen) != 0) return fail("SDL_Flip failed");
    pSDL_UpdateRect(screen, 0, 0, 800, 600);
    pSDL_FreeSurface(disp);
    pSDL_FreeSurface(tmp);
    printf("[PASS] CreateRGBSurface + DisplayFormat + UpperBlit + Flip + UpdateRect\n");

    // Verify grab unlock in windowed mode
    if (pSDL_WM_GrabInput(1) != 0) return fail("SDL_WM_GrabInput did not stay unlocked in windowed mode");
    printf("[PASS] SDL_WM_GrabInput unlocked in windowed mode\n");

    // 5. Switch to borderless fullscreen and verify same virtual_screen pointer
    SDL_Surface* fs_screen = pSDL_SetVideoMode(800, 600, 32, 0x80000000u);
    if (fs_screen != screen) return fail("Borderless SDL_SetVideoMode did not preserve virtual_screen pointer");
    pSDL_Flip(fs_screen);
    printf("[PASS] Borderless fullscreen preserved virtual_screen=%p\n", (void*)fs_screen);

    // Switch back to windowed
    SDL_Surface* win_screen = pSDL_SetVideoMode(800, 600, 32, 0);
    if (win_screen != screen) return fail("Return-to-windowed SDL_SetVideoMode did not preserve virtual_screen pointer");
    printf("[PASS] Switched back to windowed cleanly\n");

    // 6. Mouse coordinate round-trip via PushEvent / PollEvent
    SDL_Event ev = {};
    while (pSDL_PollEvent(&ev) > 0) {}

    SDL_Event push_btn = {};
    push_btn.type = 5; // SDL_MOUSEBUTTONDOWN
    push_btn.button.which = 0;
    push_btn.button.button = 1;
    push_btn.button.state = 1;
    push_btn.button.x = 400;
    push_btn.button.y = 300;
    if (pSDL_PushEvent(&push_btn) != 0) return fail("SDL_PushEvent failed");

    bool found = false;
    int rx = -1, ry = -1;
    for (int i = 0; i < 64 && pSDL_PollEvent(&ev) > 0; ++i) {
        if (ev.type == 5 && ev.button.button == 1) {
            rx = ev.button.x;
            ry = ev.button.y;
            found = true;
            break;
        }
    }
    if (!found || rx != 400 || ry != 300) {
        fprintf(stderr, "Mouse round-trip got found=%d (%d, %d)\n", found ? 1 : 0, rx, ry);
        return fail("Mouse PushEvent/PollEvent round-trip did not preserve (400, 300)");
    }
    printf("[PASS] Mouse PushEvent/PollEvent round-trip preserved (%d, %d)\n", rx, ry);

    // 7. SDL_ttf compatibility
    if (pTTF_Init() != 0) return fail("TTF_Init failed");
    pTTF_Quit();
    printf("[PASS] SDL_ttf TTF_Init / TTF_Quit succeeded\n");

    // 8. SDL_Quit
    pSDL_Quit();
    FreeLibrary(ttf);
    FreeLibrary(img);
    FreeLibrary(sdl);
    printf("[PASS] All smoke tests passed!\n");
    return 0;
}
