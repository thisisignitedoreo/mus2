
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "raylib.h"

#include "arena.h"
#include "stringview.h"
#include "array.h"
#include "fileio.h"
#include "json_write.h"
#include "json_read.h"

Arena main_arena;

float clamp(float x, float a, float b) {
    if (x < a) return a;
    if (x > b) return b;
    return x;
}

float font_size;

#include "assets.h"

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

    SetWindowMinSize(250, 200);
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
            if (IsKeyPressed(KEY_THREE)) { current_tab = TAB_BROWSE; search_focused = true; GetCharPressed(); }
            if (IsKeyPressed(KEY_FOUR)) current_tab = TAB_ABOUT;
        }
        
        music_update();
        
        BeginDrawing();
        
        ui_draw();

        if (debug) {
            size_t bytes = 0;
            for (Region* r = main_arena.begin; r; r = r->next) bytes += r->cursor;
            size_t width = ui_measure_text(sv((char*) TextFormat("memory usage: %db", bytes)));
            ui_draw_rect(font_size*0.5f, font_size*0.5f, width + font_size, font_size*3.f, theme.mg);
            ui_draw_text(font_size, font_size, sv((char*) TextFormat("%dx%d, p: %dx%d", GetScreenWidth(), GetScreenHeight(), GetMouseX(), GetMouseY())), theme.fg, 0, 0, 1e9);
            ui_draw_text(font_size, font_size*2.f, sv((char*) TextFormat("memory usage: %db", bytes)), theme.fg, 0, 0, 1e9);
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
