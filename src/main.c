
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include <math.h>

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
String Utf8Conv(String str) {
    char* path = sv_to_cstr(str);
    static char shortPath[MAX_PATH];
    wchar_t wPath[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, path, -1, wPath, MAX_PATH);
    WideCharToMultiByte(CP_ACP, 0, wPath, -1, shortPath, MAX_PATH, NULL, NULL);
    free(path);
    return sv(shortPath);
}
String Utf8Unconv(String str) {
    char* path = sv_to_cstr(str);
    static char shortPath[MAX_PATH];
    wchar_t wPath[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, path, -1, wPath, MAX_PATH);
    WideCharToMultiByte(CP_UTF8, 0, wPath, -1, shortPath, MAX_PATH, NULL, NULL);
    free(path);
    return sv(shortPath);
}
#define windows_path_convert(x) Utf8Conv(x)
#define windows_unpath_convert(x) Utf8Unconv(x)
#else
#define windows_path_convert(x) x
#define windows_unpath_convert(x) x
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
    StringBuilder* debug_sb = array_new(&main_arena);

    SetRandomSeed(time(NULL));
    
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
        if (IsKeyPressed(KEY_F2)) TakeScreenshot(arena_printf(&main_arena, "mus2-screenshot-%.2lf.png", GetTime()));

        if (!search_focused && !album_add_focused) {
            if (IsKeyPressed(KEY_SPACE)) music_toggle_pause();
            if (IsKeyPressed(KEY_LEFT)) music_previous();
            if (IsKeyPressed(KEY_RIGHT)) music_next();
            if (IsKeyPressed(KEY_R)) music_set_repeat((music_repeat()+1)%COUNT_REPEAT);
        
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
            int l = 0;
            debug_sb->size = 0;
            array_sb_printf(debug_sb, "debug info:"); l++;
            array_sb_printf(debug_sb, "\nW: %dx%d", GetScreenWidth(), GetScreenHeight()); l++;
            array_sb_printf(debug_sb, "\nP: %dx%d", GetMouseX(), GetMouseY()); l++;
            array_sb_printf(debug_sb, "\nM: %.2f%s", bytes < 1024 ? (float) bytes : bytes < 1024*1024 ? (float) bytes/1024 : bytes < 1024*1024*1024 ? (float) bytes/1024/1024 : (float) bytes/1024/1024/1024, bytes < 1024 ? " B" : bytes < 1024*1024 ? " KiB" : bytes < 1024*1024*1024 ? " MiB" : " GiB"); l++;
            array_sb_printf(debug_sb, "\nFS: %.1fpx", font_size); l++;
            array_sb_printf(debug_sb, "\n%d FPS (%.2f ms)", GetFPS(), GetFrameTime()*100); l++;
            float w = ui_measure_text_multiline(sv_from_sb(debug_sb));
            ui_draw_rect(font_size*0.5f, font_size*0.5f, w + font_size, font_size*(1+l), theme->mg);
            ui_draw_text_multiline(font_size, font_size, sv_from_sb(debug_sb), theme->fg);
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
