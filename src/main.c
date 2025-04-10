
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "raylib.h"

#include "arena.h"
#include "stringview.h"
#include "array.h"
#include "fileio.h"

Arena main_arena;

float clamp(float x, float a, float b) {
    if (x < a) return a;
    if (x > b) return b;
    return x;
}

float font_size;

#include "assets.h"
#include "win32.h"

#ifdef _WIN32
char* Utf8Conv(char *path) {
    static char shortPath[MAX_PATH];
    wchar_t wPath[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, path, -1, wPath, MAX_PATH);
    WideCharToMultiByte(CP_ACP, 0, wPath, -1, shortPath, MAX_PATH, NULL, NULL);
    return shortPath;
}
#define windows_path_convert(x) Utf8Conv(x)
#else
#define windows_path_convert(x) x
#endif

#include "tags.c"
#include "music.c"
#include "ui.c"
#include "savestate.c"

int main(void) {
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    SetConfigFlags(FLAG_WINDOW_ALWAYS_RUN);

    bool debug = false;
    
    InitWindow(600, 400, "mus2");

    SetWindowMinSize(400, 300);
    SetExitKey(0);
    
    ui_load();

    InitAudioDevice();
    music_init();
    
    savestate_load();

    SetTargetFPS(60);
    while (!WindowShouldClose()) {
        SetMouseCursor(cursor);
        cursor = MOUSE_CURSOR_ARROW;
        
        if (IsKeyPressed(KEY_F1)) debug = !debug;

        if (!search_focused) {
            if (IsKeyPressed(KEY_SPACE)) music_toggle_pause();
            if (IsKeyPressed(KEY_LEFT)) music_playlist_backward();
            if (IsKeyPressed(KEY_RIGHT)) music_playlist_forward();
        
            if (IsKeyPressed(KEY_ONE)) current_tab = TAB_PLAYLIST;
            if (IsKeyPressed(KEY_TWO)) current_tab = TAB_ALBUMS;
            if (IsKeyPressed(KEY_THREE)) current_tab = TAB_BROWSE;
            if (IsKeyPressed(KEY_FOUR)) current_tab = TAB_ABOUT;
            
            if (IsKeyPressed(KEY_THREE)) {
                search_focused = true; GetCharPressed();
            }
        }

        if (deferred_cover_array->size > 0) {
            size_t i = 3;
            while (deferred_cover_array->size > 0 && i > 0) {
                //for (size_t i = 0; i < deferred_cover_array->size; i++) printf("%u\n", array_get(deferred_cover_array, i));
                uint32_t idx = array_get(deferred_cover_array, 0);
                array_remove(deferred_cover_array, 0);
                music_load_coverart(idx);
                i--;
            }
        }
        
        music_update();
        
        BeginDrawing();
        
        ui_draw();

        if (debug) {
            size_t bytes = 0;
            for (Region* r = main_arena.begin; r; r = r->next) bytes += r->cursor;
            size_t width = ui_measure_text(sv((char*) TextFormat("memory usage: %db, %d FPS", bytes, GetFPS())));
            ui_draw_rect(font_size*0.5f, font_size*0.5f, width + font_size, font_size*3.f, theme->mg);
            ui_draw_text(font_size, font_size, sv((char*) TextFormat("%dx%d, p: %dx%d", GetScreenWidth(), GetScreenHeight(), GetMouseX(), GetMouseY())), theme->fg, 0, 0, 1e9);
            ui_draw_text(font_size, font_size*2.f, sv((char*) TextFormat("memory usage: %db, %d FPS", bytes, GetFPS())), theme->fg, 0, 0, 1e9);
        }
        
        EndDrawing();
    }

    savestate_save();
    
    music_deinit();
    CloseAudioDevice();
    
    ui_unload();
    CloseWindow();
    
    arena_free(&main_arena);
    
    return 0;
}
