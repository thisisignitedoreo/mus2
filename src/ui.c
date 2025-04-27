
typedef struct {
    Color bg, mg_off, mg, fg_off, fg, link;
} Theme;

Theme dark_theme = {
    {0x18, 0x18, 0x18, 0xFF},
    {0x50, 0x50, 0x50, 0xFF},
    {0x60, 0x60, 0x60, 0xFF},
    {0xD0, 0xD0, 0xD0, 0xFF},
    {0xFF, 0xFF, 0xFF, 0xFF},
    {0xA0, 0xA0, 0xFF, 0xFF},
};

Theme light_theme = {
    {0xFF, 0xFF, 0xFF, 0xFF},
    {0xD0, 0xD0, 0xD0, 0xFF},
    {0xC0, 0xC0, 0xC0, 0xFF},
    {0x20, 0x20, 0x20, 0xFF},
    {0x00, 0x00, 0x00, 0xFF},
    {0x50, 0x50, 0xFF, 0xFF},
};

Theme* theme = &dark_theme;

define_array_struct(DrawStack, Rectangle)
DrawStack* ds;

Font font;
int cursor = MOUSE_CURSOR_ARROW;
Texture play, pause, backward, forward, repeat, repeat_one, go_back, refresh, add;
bool seeking = false;
float scroll_factor = 0;

typedef enum {
    TAB_PLAYLIST,
    TAB_ALBUMS,
    TAB_BROWSE,
    TAB_ABOUT,
    COUNT_TABS
} MainTabs;

char* tab_names[COUNT_TABS] = {
    "playlist",
    "albums",
    "browse",
    "about"
};

MainTabs current_tab = TAB_PLAYLIST;

void ui_reload_font(float, String);

void ui_add_frame(float x, float y, float w, float h) {
    if (ds->size == 0) *array_push(ds) = (Rectangle) { x, y, w, h };
    else {
        Rectangle r = array_get(ds, ds->size-1);
        *array_push(ds) = (Rectangle) { r.x + x, r.y + y, w, h };
    }
}
void ui_add_frame_abs(float x, float y, float w, float h) {
    *array_push(ds) = (Rectangle) { x, y, w, h };
}

void ui_pop_frame(void) {
    (void)array_pop(ds);
}

Rectangle ui_get_frame(void) {
    if (ds->size == 0) return (Rectangle) { 0, 0, GetScreenWidth(), GetScreenHeight() };
    else return array_get(ds, ds->size-1);
}

float ui_measure_text(String string) {
    char* cstr = sv_to_cstr(string);
    float width = MeasureTextEx(font, cstr, font_size, 0).x;
    free(cstr);
    return width;
}

bool scissor_mode = false;
Rectangle scissor_mode_rect;

float ui_draw_text(float x, float y, String string, Color color, float ax, float ay, int max_width) {
    if (max_width <= 0) return 0;
    
    Rectangle frame = ui_get_frame();
    
    char* cstr = sv_to_cstr(string);

    float width = MeasureTextEx(font, cstr, font_size, 0).x;

    float rx = frame.x + x - ax*width, ry = frame.y + y - ay*font_size, w = width < max_width ? width : max_width, h = font_size;
    // A bit of a hack because raylib does not support nested
    // BeginScissorMode() calls :_(
    if (rx + w > frame.x + frame.width) w = frame.x + frame.width - rx;
    if (ry + h > frame.y + frame.height) h = frame.y + frame.height - ry;
    if (rx < frame.x) rx = frame.x;
    if (ry < frame.y) ry = frame.y;
    BeginScissorMode(rx, ry, w, h);
    DrawTextEx(font, cstr, (Vector2) {(int) (frame.x + x - ax*width), (int) (frame.y + y - ay*font_size)}, font_size, 0, color);
    EndScissorMode();
    if (scissor_mode) BeginScissorMode(scissor_mode_rect.x, scissor_mode_rect.y, scissor_mode_rect.width, scissor_mode_rect.height);

    free(cstr);

    return width;
}

float ui_measure_text_multiline(String string) {
    float max_width = 0;
    while (string.size) {
        String line = sv_split(&string, sv("\n"));
        float size = ui_measure_text(line);
        if (max_width < size) max_width = size;
    }
    return max_width;
}

float ui_draw_text_multiline(float x, float y, String string, Color color) {
    float max_width = 0;
    int l = 0;
    while (string.size) {
        String line = sv_split(&string, sv("\n"));
        float size = ui_measure_text(line);
        ui_draw_text(x, y + l*font_size, line, color, 0, 0, size);
        l++;
        if (max_width < size) max_width = size;
    }
    return max_width;
}

void ui_draw_rect(float x, float y, float width, float height, Color color) {
    Rectangle frame = ui_get_frame();
    float w = frame.x + x + width > frame.x + frame.width ? frame.width - x : width;
    float h = frame.y + y + height > frame.y + frame.height ? frame.height - y : height;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    DrawRectangle(frame.x + x, frame.y + y, w, h, color);
}

void ui_draw_circle(float x, float y, float radius, Color color) {
    Rectangle frame = ui_get_frame();
    DrawCircle(frame.x + x, frame.y + y, radius, color);
}

void ui_clear_frame(Color color) {
    Rectangle frame = ui_get_frame();
    DrawRectangleRec(frame, color);
}

void ui_draw_icon(float x, float y, Texture icon, Color color) {
    Rectangle frame = ui_get_frame();
    if (y < 0) return;
    DrawTexture(icon, frame.x + x, frame.y + y, color);
}

float spinny_degrees = 0;
void ui_draw_spinny(float x, float y, float r, Color bg, Color fg) {
    DrawCircleSector((Vector2) {(int)x, (int)y}, r, spinny_degrees, spinny_degrees + 360-60, 20, fg);
    DrawCircle((int)x, (int)y, r-font_size*0.125f, bg);
}

void ui_draw_texture(float x, float y, float w, float h, Texture texture) {
    Rectangle frame = ui_get_frame();
    if (texture.width) DrawTexture(texture, frame.x + x, frame.y + y, WHITE);
    else {
        DrawRectangle(frame.x + x, frame.y + y, w, h, theme->mg_off);
        ui_draw_spinny(frame.x + x + w/2, frame.y + y + h/2, font_size*0.375f, theme->mg_off, theme->fg);
    }
}

bool ui_mouse_in(float x, float y, float w, float h) {
    Rectangle frame = ui_get_frame();
    return (frame.x + x <= GetMouseX()) && (frame.x + x + w > GetMouseX()) && (frame.y + y <= GetMouseY()) && (frame.y + y + h > GetMouseY());
}

bool ui_mouse_in_frame(void) {
    Rectangle frame = ui_get_frame();
    return ui_mouse_in(0, 0, frame.width, frame.height);
}

bool ui_draw_button(float x, float y, Texture icon, Color bg, Color bg_on, Color fg, bool active) {
    bool hovered = ui_mouse_in(x, y, font_size, font_size) && ui_mouse_in_frame() && active;
    bool clicked = hovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    if (hovered) cursor = MOUSE_CURSOR_POINTING_HAND;
    if (clicked) {
        ui_draw_rect(x, y, font_size, font_size, fg);
        ui_draw_icon(x, y, icon, bg);
    } else if (hovered) {
        ui_draw_rect(x, y, font_size, font_size, bg_on);
        ui_draw_icon(x, y, icon, fg);
    } else {
        ui_draw_rect(x, y, font_size, font_size, bg);
        ui_draw_icon(x, y, icon, fg);
    }
    return hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

bool ui_update_input(StringBuilder* str, size_t* cursor, bool* focused) {
    bool updated = false;
    if (*focused) {
        if (IsKeyPressed(KEY_ESCAPE)) *focused = false;
        else if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && *cursor > 0) {
            int size = 0;
            (void)GetCodepointPrevious(array_set(str, *cursor), &size);
            for (int i = 0; i < size; i++) array_remove(str, *cursor-size);
            *cursor -= size;
            updated = true;
        } else if ((IsKeyPressed(KEY_LEFT) || IsKeyPressedRepeat(KEY_LEFT)) && *cursor > 0) {
            int size = 0;
            (void)GetCodepointPrevious(array_set(str, *cursor), &size);
            *cursor -= size;
        } else if ((IsKeyPressed(KEY_DELETE) || IsKeyPressedRepeat(KEY_DELETE)) && *cursor < str->size) {
            int size = 0;
            (void)GetCodepointNext(array_set(str, *cursor), &size);
            for (int i = 0; i < size; i++) array_remove(str, *cursor);
            updated = true;
        } else if ((IsKeyPressed(KEY_RIGHT) || IsKeyPressedRepeat(KEY_RIGHT)) && *cursor < str->size) {
            int size = 0;
            (void)GetCodepointNext(array_set(str, *cursor), &size);
            *cursor += size;
        } else {
            int ch = GetCharPressed(), size = 0;
            if (ch != 0) {
                const char* utf8 = CodepointToUTF8(ch, &size);
                for (int i = 0; i < size; i++) *(char*)array_add(str, *cursor + i) = utf8[i];
                *cursor += size;
                updated = true;
            }
        }
    }
    return updated;
}

void ui_draw_shadow_vd(int x, int y, int w, int h, Color col) {
    for (int i = y; i < y+h; i++) {
        int alpha = (float)col.a/255*((float)(i-y)/h)*255;
        DrawRectangle(x, i, w, 1, (Color) { col.r, col.g, col.b, alpha });
    }
}

void ui_draw_shadow_vu(int x, int y, int w, int h, Color col) {
    for (int i = y; i < y+h; i++) {
        int alpha = (float)col.a/255*(1-(float)(i-y)/h)*255;
        DrawRectangle(x, i, w, 1, (Color) { col.r, col.g, col.b, alpha });
    }
}

void ui_draw_shadow_hl(int x, int y, int w, int h, Color col) {
    for (int i = x; i < x+w; i++) {
        int alpha = (float)col.a/255*(1-(float)(i-x)/w)*255;
        DrawRectangle(i, y, 1, h, (Color) { col.r, col.g, col.b, alpha });
    }
}

void ui_draw_shadow_hr(int x, int y, int w, int h, Color col) {
    for (int i = x; i < x+w; i++) {
        int alpha = (float)col.a/255*((float)(i-x)/w)*255;
        DrawRectangle(i, y, 1, h, (Color) { col.r, col.g, col.b, alpha });
    }
}

// ACTUAL UI STUFF

void ui_draw_menubar(void) {
    ui_add_frame(0, 0, GetScreenWidth(), font_size*1.5f);
    ui_clear_frame(theme->mg_off);

    Rectangle frame = ui_get_frame();

    // mus
    
    bool loading = deferred_cover_array->size > 0;

    float mus_size = ui_measure_text(sv("mus"));
    if (loading) ui_draw_spinny(font_size*0.5f + mus_size/2, font_size*0.75f, font_size*0.375f, theme->mg_off, theme->fg);
    else ui_draw_text(font_size*0.5f, font_size*0.75f, sv("mus"), theme->fg, 0, 0.5f, 1e9);

    // tab buttons
    float c = font_size*0.75f + mus_size;
    for (size_t i = 0; i < COUNT_TABS; i++) {
        c += font_size*0.25f; // outer padding
        String name = sv(tab_names[i]);
        float size = ui_measure_text(name);
        bool hovered = ui_mouse_in(c, font_size*0.25f, size + font_size*0.5f, font_size);
        if (hovered) cursor = MOUSE_CURSOR_POINTING_HAND;
        if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) current_tab = i;
        ui_draw_rect(c, font_size*0.25f, size + font_size*0.5f, font_size, i == current_tab ? theme->fg : theme->mg);
        ui_draw_text(c + font_size*0.25f, font_size*0.25f, name, i == current_tab ? theme->mg_off : theme->fg, 0, 0, size);
        c += size + font_size*0.5f;
    }

    // volume spinbox
    String str = sv((char*) TextFormat("%.0f%%", music_get_volume()*100.f));
    float str_size = ui_measure_text(str);
    float vx = frame.width - str_size - font_size*0.5f, vy = 0, vw = str_size + font_size*0.5f, vh = font_size*1.5f;
    bool hovered = ui_mouse_in(vx, vy, vw, vh);
    if (hovered) {
        float scroll = GetMouseWheelMove();
        float factor = IsKeyDown(KEY_LEFT_SHIFT) ? 0.01f : 0.05f;
        if (scroll > 0 && music_get_volume() < 1.0f) music_set_volume(music_get_volume()+factor);
        else if (scroll < 0 && music_get_volume() > 0.0f) music_set_volume(music_get_volume()-factor);
        if (music_get_volume() < 0.0f) music_set_volume(0.0f);
        if (music_get_volume() > 1.0f) music_set_volume(1.0f);
        cursor = MOUSE_CURSOR_RESIZE_NS;
        ui_draw_rect(vx, vy, vw, vh, theme->mg);
    }
    ui_draw_text(frame.width - font_size*0.25f, font_size*0.25f, str, theme->fg, 1, 0, str_size);

    ui_pop_frame();
}

void ui_draw_statusbar(void) {
    ui_add_frame(0, GetScreenHeight() - font_size*3.5f, GetScreenWidth(), font_size*3.5f);
    ui_clear_frame(theme->mg_off);

    Rectangle frame = ui_get_frame();

    // Handy shortcuts :^)
    float f = font_size, w = frame.width;
    
    // METADATA

    bool loaded = music_is_playing();
    SongMetadata meta = music_get_playing_metadata();
    bool none = meta.album_name.size == 0 && meta.title.size == 0 && meta.artist.size == 0;
    if (!meta.album_name.size) meta.album_name = sv("<none>");
    if (!meta.title.size) meta.title = sv("<none>");
    if (!meta.artist.size) meta.artist = sv("<none>");
    
    float c = 0;
    
    if (loaded) {
        if (none) {
            ui_draw_text(f*0.5f, f*0.5f, meta.path, theme->fg, 0, 0, w-f);
        } else {
            c += ui_draw_text(f*0.5f + c, f*0.5f, meta.artist, theme->fg_off, 0, 0, w - f - c);
            c += ui_draw_text(f*0.5f + c, f*0.5f, sv(" - "), theme->fg_off, 0, 0, w - f - c);
            c += ui_draw_text(f*0.5f + c, f*0.5f, meta.title, theme->fg, 0, 0, w - f - c);
        
            size_t album_name = ui_measure_text(meta.album_name);
            if (w - album_name > c + f) ui_draw_text(w - f*0.5f, f*0.5f, meta.album_name, theme->fg_off, 1, 0, 1e9);
        }
    } else {
        ui_draw_text(f*0.5f, f*0.5f, sv("drag-n-drop some music!"), theme->fg_off, 0, 0, w - f);
    }

    // PLAYER CONTROLS

    // SEEK LABEL
    float seek, length;
    if (loaded) {
        seek = music_get_playing_seek();
        length = music_get_playing_length();
    } else { seek = 0.f; length = 0.f; }
    
    c = 0;
    c += ui_draw_text(f*0.5f + c, f*2.f, sv((char*) TextFormat("%d:%02d", ((int) seek)/60, ((int) seek)%60)), theme->fg, 0, 0, 1e9);
    c += f/4.f;
    c += ui_draw_text(f*0.5f + c, f*2.f, sv("/"), theme->fg_off, 0, 0, 1e9);
    c += f/4.f;
    c += ui_draw_text(f*0.5f + c, f*2.f, sv((char*) TextFormat("%d:%02d", ((int) length)/60, ((int) length)%60)), theme->fg, 0, 0, 1e9);
    
    // BUTTONS
    bool back = ui_draw_button(w - f*4.5f, f*2.f, backward, theme->mg_off, theme->mg, theme->fg, loaded);
    bool pause_ = ui_draw_button(w - f*3.5f, f*2.f, music_paused() ? play : pause, theme->mg_off, theme->mg, theme->fg, loaded);
    bool forw = ui_draw_button(w - f*2.5f, f*2.f, forward, theme->mg_off, theme->mg, theme->fg, loaded);
    bool repeat_ = ui_draw_button(w - f*1.5f, f*2.f, music_repeat() == REPEAT_ONE ? repeat_one : repeat, theme->mg_off, theme->mg, music_repeat() ? theme->fg : theme->fg_off, loaded);

    if (back) music_playlist_backward();
    if (pause_) music_toggle_pause();
    if (forw) music_playlist_forward();
    if (repeat_) music_set_repeat((music_repeat()+1)%COUNT_REPEAT);
    
    // SEEKBAR
    bool seekbar_hovered = ui_mouse_in(f*1.f + c, f*2.2f, w - f*6.f - c, f*0.6f);
    float seek_float = seek/length;

    float perc;
    if (seeking) {
        float x1 = f*1.f + c, x2 = w - f*5.f;
        perc = clamp((GetMouseX() - x1) / (x2 - x1), 0.001f, 0.999f);
        size_t second = perc*length;
        seek_float = perc;
        
        //ui_draw_circle(f*1.f + c + perc*(w - f*5.f - c), f*2.5f, f*0.3f, theme->fg);
        String text = sv((char*) TextFormat("%d:%02d", second/60, second%60));
        size_t width = ui_measure_text(text);
        ui_draw_rect(f*1.f + c + perc*(w - f*5.f - c - width*0.5f - f*0.25f) - width*0.5f - f*0.25f, f, width + f*0.5f, f, theme->bg);
        ui_draw_text(f*1.f + c + perc*(w - f*5.f - c - width*0.5f - f*0.25f), f*2.f, text, theme->fg, 0.5f, 1.f, 1e9);
    }
    
    ui_draw_rect(f*1.f + c, f*2.5f - f*0.0625f, w - f*6.f - c, f*0.125f, theme->mg);
    if (loaded) {
        ui_draw_rect(f*1.f + c, f*2.5f - f*0.0625f, seek_float*(w - f*6.f - c), f*0.125f, theme->fg);
        ui_draw_circle(f*1.f + c + seek_float*(w - f*6.f - c), f*2.5f, seekbar_hovered ? f*0.3f : f*0.2f, theme->fg);
    }

    if (loaded && seekbar_hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) seeking = true;
    if (seeking && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) { seeking = false; music_seek(perc*length); }
    if (loaded && (seekbar_hovered || seeking)) cursor = MOUSE_CURSOR_POINTING_HAND;
    
    ui_pop_frame();
}

float playlist_scroll = 0.0f;

void ui_begin_scissor_mode(void) {
    Rectangle frame = ui_get_frame();
    scissor_mode_rect = frame;
    scissor_mode = true;
    BeginScissorMode(frame.x, frame.y, frame.width, frame.height);
}

void ui_end_scissor_mode(void) {
    scissor_mode = false;
    EndScissorMode();
}

void ui_scrollable(float size, float* scroll) {
    if (!ui_mouse_in_frame()) return;
    float s = GetMouseWheelMove();
    float vp_size = ui_get_frame().height;
    if (s < 0) *scroll -= scroll_factor;
    else if (s > 0 && *scroll < 0) *scroll += scroll_factor;
    if (vp_size >= size) { *scroll = 0; return; }
    if (vp_size - size > *scroll) *scroll = vp_size - size;
    if (*scroll > 0) *scroll = 0;
}

void ui_scrollbar(float size, float scroll) {
    Rectangle frame = ui_get_frame();
    scroll = -scroll;
    float vp_size = frame.height;
    if (vp_size >= size) return;
    float scrolled = scroll / (size - vp_size);
    int knob_size = font_size*0.66f;
    ui_draw_rect(frame.width-font_size*0.25f, 0, font_size*0.25f, frame.height, theme->bg);
    ui_draw_rect(frame.width-font_size*0.25f, scrolled*(frame.height-knob_size), font_size*0.25f, knob_size, theme->mg_off);
}

void ui_draw_playlist(void) {
    if (IsFileDropped() && ui_mouse_in_frame()) {
        FilePathList files = LoadDroppedFiles();
        for (size_t i = 0; i < files.count; i++) {
            if (dir_exists(sv(files.paths[i]))) continue;
            music_insert_into_playlist(sv(files.paths[i]));
        }
        UnloadDroppedFiles(files);
    }
    
    Rectangle frame = ui_get_frame();
    ui_begin_scissor_mode();

    if (IsKeyPressed(KEY_DELETE)) {
        music_unload();
        while (playlist->size > 0) music_remove_from_playlist(0);
    }
    
    if (playlist->size == 0) {
        ui_draw_text(frame.width/2, frame.height/2, sv("whoa, quite empty here..."), theme->mg, 0.5f, 0.5f, 1e9);
    } else {
        float p = playlist_scroll, f = font_size, w = frame.width, h = frame.height;
        String old_album_name = {0};
        float h1 = playlist->size * f + f;
        if (playing >= 0) h1 += f*6.5f;
        
        array_foreach(playlist, i) {
            if (i*f + 0.5f*f + f + playlist_scroll < 0) continue;
            if (i*f + 0.5f*f + playlist_scroll > w) break;
            String path = array_get(playlist, i);
            SongMetadata meta = music_get_metadata(path);
            bool none = meta.album_name.size == 0 && meta.title.size == 0 && meta.artist.size == 0;
            if (!meta.album_name.size) meta.album_name = sv("<none>");
            if (!meta.title.size) meta.title = sv("<none>");
            if (!meta.artist.size) meta.artist = sv("<none>");
            
            bool hovered = ui_mouse_in(0, playlist_scroll + font_size*(i+0.5f), frame.width, font_size) && ui_mouse_in_frame() && (playing < 0 ? true : !ui_mouse_in(w - f*6.5f, h - f*6.5f, f*6.f, f*6.f));
            Color gray = i == (size_t) playing || hovered ? theme->fg_off : theme->mg;
            if (hovered) ui_draw_rect(0, playlist_scroll + font_size*(i+0.5f), frame.width, font_size, theme->mg);
            if (hovered) cursor = MOUSE_CURSOR_POINTING_HAND;
            if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { music_unload(); music_load(i); }
            if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) { music_remove_from_playlist(i); }
            float c = 0;

            if (none) {
                ui_draw_text(f*0.5f, p + f*(0.5f + i), meta.path, gray, 0, 0, w-f);
                old_album_name = sv_from_bytes(NULL, 0);
            } else {
                c += ui_draw_text(f*0.5f + c, p + f*(0.5f + i), sv((char*) TextFormat("%d. ", i+1)), gray, 0, 0, w - f - c);
                c += ui_draw_text(f*0.5f + c, p + f*(0.5f + i), meta.artist, theme->fg, 0, 0, w - f - c);
                c += ui_draw_text(f*0.5f + c, p + f*(0.5f + i), sv(" - "), gray, 0, 0, w - f - c);
                c += ui_draw_text(f*0.5f + c, p + f*(0.5f + i), meta.title, theme->fg, 0, 0, w - f - c);
                String al = sv((char*) TextFormat(SV_FMT" ("SV_FMT")", SvFmt(meta.album_name), SvFmt(meta.year)));
                size_t als = ui_measure_text(al);
                if (w - als > f + c && !sv_compare(al, old_album_name) && i*f + 0.5f*f + playlist_scroll >= 0) {
                    ui_draw_text(w - f*0.5f, playlist_scroll + f*0.5f + i*f, al, gray, 1, 0, als);
                    old_album_name = al;
                }
            }
        }

        if (playing >= 0) {
            SongMetadata meta = music_get_playing_metadata();
            ui_draw_texture(w - f*6.5f, h - f*6.5f, f*6.f, f*6.f, meta.cover_art_6x);
        }
        
        ui_scrollable(h1, &playlist_scroll);
        ui_scrollbar(h1, playlist_scroll);
    }
    
    ui_end_scissor_mode();
}

size_t album_index = 0;
bool album_selected = false;

float album_scroll = 0.0f;

float i_album_scroll = 0.0f;

StringBuilder* album_add;
size_t album_add_cursor = 0;
bool album_add_focused = false;

bool aadd_folder_exists = false;
StringArray* aadd_folder_content;
String aadd_real_path;
String aadd_leftover_path;

String cwd;

bool aadd_file = false;

float aadd_scroll = 0.f;

void ui_update_aadd(void) {
    String full_path = windows_path_convert(sv_from_sb(album_add));
    if (file_exists(full_path)) {
        aadd_real_path = full_path;
        aadd_file = true;
        aadd_folder_exists = false;
    } else {
        size_t path_size = album_add->size;
        if (!dir_exists(full_path)) {
            for (size_t i = album_add->size; i > 0; i--) {
                char ch = array_get(album_add, i-1);
                if (ch == '/' || ch == '\\') { path_size = i; break; }
            }
        }
        String p = windows_path_convert(sv_from_bytes(album_add->data, path_size));
        aadd_real_path = sv_copy(p, &main_arena); // windows_path_convert uses static memory
        p = windows_path_convert(sv_from_bytes(album_add->data + path_size, album_add->size - path_size));
        aadd_leftover_path = sv_copy(p, &main_arena);
        aadd_folder_exists = dir_exists(aadd_real_path);
        if (aadd_folder_exists) {
            aadd_folder_content = dir_list(aadd_real_path, &main_arena);
        }
    }
}

bool ui_startswith(String, String);

void ui_draw_albums(void) {
    if (IsFileDropped() && ui_mouse_in_frame()) {
        FilePathList files = LoadDroppedFiles();
        SetTraceLogLevel(LOG_NONE);
        for (size_t i = 0; i < files.count; i++) {
            music_scan_folder(windows_path_convert(sv(files.paths[i])));
        }
        SetTraceLogLevel(LOG_INFO);
        UnloadDroppedFiles(files);

        music_sort_albums();
    }
    
    Rectangle frame = ui_get_frame();
    ui_begin_scissor_mode();

    if (album_selected) {
        Album album = array_get(albums, album_index);
        if (album.artists.size == 0) album.artists = sv("<none>");
        if (album.name.size == 0) album.name = sv("<none>");
        if (album.year.size == 0) album.year = sv("<none>");
        
        float f = font_size, w = frame.width, s = i_album_scroll;
        ui_draw_texture(f*0.5f, f*0.5f+s, f*8.f, f*8.f, album.cover_x8);
        ui_draw_text(f*9.f, f*0.5f+s, album.name, theme->fg, 0, 0, w - f*9.5f);
        ui_draw_text(f*9.f, f*1.5f+s, album.artists, theme->mg, 0, 0, w - f*9.5f);
        ui_draw_text(f*9.f, f*2.5f+s, album.year, theme->mg, 0, 0, w - f*9.5f);

        album_selected = album_selected && !ui_draw_button(w - f*1.5f, f*0.5f+s, go_back, theme->bg, theme->mg_off, theme->fg, true);
        album_selected = album_selected && !IsKeyPressed(KEY_ESCAPE);
        array_foreach(album.tracks, i) {
            SongMetadata meta = array_get(album.tracks, i);
            if (meta.artist.size == 0) meta.artist = sv("<none>");
            if (meta.title.size == 0) meta.title = sv("<none>");
            bool hovered = ui_mouse_in(0, f*9.f + f*i+s, w, f) && ui_mouse_in_frame();
            Color gray = hovered ? theme->fg_off : theme->mg;
            if (hovered) {
                cursor = MOUSE_CURSOR_POINTING_HAND;
                ui_draw_rect(0, f*9.f + f*i+s, w, f, theme->mg_off);
            }

            if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                while (playlist->size > 0) music_remove_from_playlist(0);
                array_foreach(album.tracks, j) { music_insert_into_playlist(array_get(album.tracks, j).path); }
                music_unload();
                music_load(i);
            } else if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) music_insert_into_playlist(meta.path);
            else if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE)) array_foreach(album.tracks, j) { music_insert_into_playlist(array_get(album.tracks, j).path); }
            float c = 0;
            c += ui_draw_text(f*0.5f + c, f*9.f + f*i+s, sv((char*) TextFormat("%d. ", meta.track)), gray, 0, 0, w - c - f);
            c += ui_draw_text(f*0.5f + c, f*9.f + f*i+s, meta.artist, theme->fg, 0, 0, w - c - f);
            c += ui_draw_text(f*0.5f + c, f*9.f + f*i+s, sv(" - "), gray, 0, 0, w - c - f);
            c += ui_draw_text(f*0.5f + c, f*9.f + f*i+s, meta.title, theme->fg, 0, 0, w - c - f);
        }
        float size = f*9.5f + f*(album.tracks->size);
        ui_scrollable(size, &i_album_scroll);
        ui_scrollbar(size, i_album_scroll);
    } else if (album_add_focused) {
        if (ui_draw_button(font_size*0.5f, font_size*0.5f + aadd_scroll, go_back, theme->bg, theme->mg, theme->fg, true)) {
            album_add_focused = false; album_add->size = 0; album_add_cursor = 0;
            aadd_folder_exists = false; aadd_file = false;
        }
        float text_size = 0;
        float bar_size = 2;
        float bar_offset = 1 + bar_size;
        if (album_add->size) {
            float pre = ui_draw_text(font_size*2.f, font_size*0.5f + aadd_scroll, sv_from_bytes(album_add->data, album_add_cursor), theme->fg, 0, 0, frame.width - font_size*2.5f);
            text_size = pre + ui_draw_text(font_size*2.f + pre + bar_offset, font_size*0.5f + aadd_scroll, sv_from_bytes(album_add->data + album_add_cursor, album_add->size - album_add_cursor), theme->fg, 0, 0, frame.width - font_size*2.5f - pre - bar_offset);
            if (font_size*2.5f + pre < frame.width) ui_draw_rect(font_size*2.f + pre + 1, font_size*0.5f + aadd_scroll, bar_size, font_size, theme->fg_off);
        } else {
            ui_draw_rect(font_size*2.f, font_size*0.5f + aadd_scroll, bar_size, font_size, theme->fg_off);
            text_size = ui_draw_text(font_size*2.f + bar_offset, font_size*0.5f + aadd_scroll, sv("enter a path..."), theme->mg, 0, 0, frame.width - font_size*2.5f);
        }
        String enter = sv("Enter");
        float enter_size = ui_measure_text(enter);
        if (font_size*2.5f + text_size + enter_size < frame.width) ui_draw_text(frame.width - enter_size - font_size*0.5f, font_size*0.5f + aadd_scroll, enter, theme->mg, 0, 0, enter_size);
        
        if (ui_update_input(album_add, &album_add_cursor, &album_add_focused)) ui_update_aadd();

        int r = 0;
        float f = font_size, w = frame.width;
        if (aadd_folder_exists) {
            dir_change_cwd(aadd_real_path);
            array_foreach(aadd_folder_content, j) {
                String file = array_get(aadd_folder_content, j);
                bool selectable = dir_exists(file) || sv_endswith(file, sv(".mp3")) || sv_endswith(file, sv(".flac"));
                if (sv_compare(file, sv(".")) || sv_compare(file, sv("..")) || !selectable || !ui_startswith(file, aadd_leftover_path)) continue;
                bool hovered = ui_mouse_in_frame() && ui_mouse_in(0, f*2.f + f*r + aadd_scroll, w, f);
                if (hovered) ui_draw_rect(0, f*2.f + f*r + aadd_scroll, w, f, theme->mg);
                if (hovered) cursor = MOUSE_CURSOR_POINTING_HAND;
                if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    aadd_scroll = 0.0f;
                    if (!aadd_leftover_path.size) {
                        if (!sv_endswith(sv_from_sb(album_add), sv("/")) && !sv_endswith(sv_from_sb(album_add), sv("\\"))) {
                            *array_push(album_add) = '/';
                        }
                    }
                    array_sb_push_string(album_add, windows_unpath_convert(sv_from_bytes(file.bytes + windows_unpath_convert(aadd_leftover_path).size, file.size - windows_unpath_convert(aadd_leftover_path).size)));
                    album_add_cursor = album_add->size;
                    ui_update_aadd();
                }
                ui_draw_text(f*0.5f, f*2.f + f*r + aadd_scroll, windows_unpath_convert(file), theme->fg, 0, 0, w-f);
                r++;
            }
            dir_change_cwd(cwd);
        } else if (aadd_file) {
            if (isflac(aadd_real_path)) ui_draw_text(f*0.5f, f*2.f + f*r + aadd_scroll, sv("<FLAC file>"), theme->fg_off, 0, 0, w-f);
            else if (isid3v2(aadd_real_path)) ui_draw_text(f*0.5f, f*2.f + f*r + aadd_scroll, sv("<MP3 file>"), theme->fg_off, 0, 0, w-f);
            else ui_draw_text(f*0.5f, f*2.f + f*r + aadd_scroll, sv("<unsupported file>"), theme->fg_off, 0, 0, w-f);
            r++;
        } else {
            ui_draw_text(f*0.5f, f*2.f + f*r + aadd_scroll, sv("<no such directory>"), theme->fg_off, 0, 0, w-f);
            r++;
        }
        float size = (r+2.5f)*f;
        ui_scrollable(size, &aadd_scroll);
        ui_scrollbar(size, aadd_scroll);
        if (IsKeyPressed(KEY_ENTER)) {
            if (aadd_file) music_insert_into_playlist(aadd_real_path);
            else if (aadd_folder_exists) music_scan_folder(aadd_real_path);
            album_add_focused = false; album_add->size = 0; album_add_cursor = 0;
            aadd_folder_exists = false; aadd_file = false;
        }
    } else if (albums->size == 0) {
        ui_draw_text(frame.width/2, frame.height/2, sv("drag-n-drop a folder here"), theme->mg, 0.5f, 0.5f, 1e9);
        if (ui_draw_button(frame.width - font_size*1.5f, font_size*0.5f, add, theme->bg, theme->mg, theme->fg, true)) {
            album_add_focused = true;
        }
    } else {
        i_album_scroll = 0.f;
        float r = 0, c = 0, s = album_scroll, e = 0;
        float f = font_size, w = frame.width;
        float rw = f*7.f, rh = f*9.f;
        array_foreach(albums, i) {
            Album album = array_get(albums, i);
            
            float x = c*rw + f*0.5f, y = r*rh + f*0.5f + s;
            bool hovered = ui_mouse_in(x, y, rw, rh) && ui_mouse_in_frame() && !ui_mouse_in(frame.width - font_size*1.5f, font_size*0.5f, font_size, font_size);
            if (hovered) cursor = MOUSE_CURSOR_POINTING_HAND;
            if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                album_selected = true; album_index = i;
            }
            
            ui_draw_rect(x, y, rw, rh, hovered ? theme->mg_off : theme->bg);
            ui_draw_texture(x + f*0.5f, y + f*0.5f, f*6.f, f*6.f, album.cover_x6);
            String name = album.name.size ? album.name : sv("<none>");
            String artists = album.artists.size ? album.artists : sv("<none>");
            String year = album.year;
            ui_draw_text(x + f*0.5f, y + f*6.75f, name, theme->fg, 0, 0, rw - f);
            e = 0;
            e += ui_draw_text(x + f*0.5f + e, y + f*7.75f, artists, hovered ? theme->fg_off : theme->mg, 0, 0, rw - f);
            if (year.size) {
                e += ui_draw_text(x + f*0.5f + e, y + f*7.75f, sv(", "), hovered ? theme->fg_off : theme->mg, 0, 0, rw - e - f);
                e += ui_draw_text(x + f*0.5f + e, y + f*7.75f, year, hovered ? theme->fg_off : theme->mg, 0, 0, rw - e - f);
            }
            
            if (i != albums->size-1) {
                c++;
                if ((c+1)*rw + f*0.5f > w) { c = 0; r++; }
            }
        }
        float size = (r+1)*rh + f;
        ui_scrollable(size, &album_scroll);
        ui_scrollbar(size, album_scroll);
        
        if (ui_draw_button(frame.width - font_size*1.5f, font_size*0.5f, add, theme->bg, theme->mg, theme->fg, true)) {
            album_add_focused = true;
        }
    }
    
    ui_end_scissor_mode();
}

StringBuilder* search;
size_t search_cursor = 0;
bool search_focused = false;

float browse_scroll = 0.0f;

int ui_to_lower(int codepoint) {
    if (codepoint < '~') return (char)tolower((char)codepoint);
    if (codepoint >= 1040 && codepoint < 1072) return codepoint + 32;
    return codepoint;
}

int ui_get_codepoint(String string, size_t index, int* size) {
    int c = GetCodepoint(string.bytes + index, size);
    return ui_to_lower(c);
}

bool ui_compare(String a, String b) {
    if (a.size != b.size) return false;
    for (size_t i = 0; i < a.size; i++) {
        int size_a = 0, a_codep = ui_get_codepoint(a, i, &size_a);
        int size_b = 0, b_codep = ui_get_codepoint(b, i, &size_b);
        if (a_codep != b_codep) return false;
        i += size_a;
    }
    return true;
}

bool ui_startswith(String a, String b) {
    if (a.size < b.size) return false;
    return ui_compare(sv_from_bytes(a.bytes, b.size), b);
}

bool ui_string_in(String a, String b) {
    if (a.size == 0 || b.size == 0) return false;
    if (a.size < b.size) return false;
    for (size_t i = 0; i < a.size-b.size+1;) {
        int size = 0;
        if (ui_compare(sv_from_bytes(a.bytes + i, b.size), b)) return true;
        (void)GetCodepoint(a.bytes+i, &size); i += size;
    }
    return false;
}

bool ui_match_song(SongMetadata song, String s) {
    bool matched = false; 
   matched = matched || ui_string_in(song.title, s);
    matched = matched || ui_string_in(song.artist, s);
    return matched;
}

bool ui_match_album(Album album, String s) {
    bool matched = false;
    matched = matched || ui_string_in(album.name, s);
    matched = matched || ui_string_in(album.artists, s);
    matched = matched || ui_string_in(album.year, s);
    array_foreach(album.tracks, i) {
        matched = matched || ui_match_song(array_get(album.tracks, i), s);
    }
    return matched;
}

void ui_draw_browse(void) {
    if (IsFileDropped() && ui_mouse_in_frame()) {
        FilePathList files = LoadDroppedFiles();
        SetTraceLogLevel(LOG_NONE);
        for (size_t i = 0; i < files.count; i++) {
            music_scan_folder(windows_path_convert(sv(files.paths[i])));
        }
        SetTraceLogLevel(LOG_INFO);
        UnloadDroppedFiles(files);

        music_sort_albums();
    }
    
    ui_update_input(search, &search_cursor, &search_focused);
    
    Rectangle frame = ui_get_frame();

    float f = font_size, w = frame.width, s = browse_scroll;
    bool hovered = ui_mouse_in(0, f*0.5f, w, f);
    if (hovered) cursor = MOUSE_CURSOR_IBEAM;
    if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) search_focused = true;
    if (!hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) search_focused = false;
    if (search->size == 0 && !search_focused) {
        ui_draw_text(f*0.5f, f*0.5f+s, sv("click here to search..."), theme->mg, 0, 0, w - f);
    } else {
        float bar_size = 2;
        float pre_cursor_text_size = ui_draw_text(f*0.5f, f*0.5f+s, sv_from_bytes(search->data, search_cursor), theme->fg, 0, 0, w - f);
        ui_draw_text(f*0.5f + pre_cursor_text_size + (search_focused ? bar_size + 1 : 0), f*0.5f+s, sv_from_bytes(search->data + search_cursor, search->size-search_cursor), theme->fg, 0, 0, w - f - (pre_cursor_text_size + (search_focused ? bar_size + 1 : 0)));
        if (pre_cursor_text_size < w-f-bar_size && search_focused) {
            ui_draw_rect(f*0.5f + pre_cursor_text_size + 1, f*0.5f+s, bar_size, f, theme->fg_off);
        }
    }

    if (albums->size == 0) ui_draw_text(frame.width/2, frame.height/2, sv("drag-n-drop a folder here"), theme->mg, 0.5f, 0.5f, 1e9);
    
    bool first_album = true, first_song = true;
    
    float r = 0;
    array_foreach(albums, i) {
        Album album = array_get(albums, i);
        if (album.artists.size == 0) album.artists = sv("<none>");
        if (album.name.size == 0) album.name = sv("<none>");
        if (album.year.size == 0) album.year = sv("<none>");
        if (search->size && !ui_match_album(album, sv_from_sb(search))) continue;
        float height = f*2.f+s+f*r;
        array_foreach(album.tracks, j) {
            SongMetadata meta = array_get(album.tracks, j);
            if (search->size && !ui_match_song(meta, sv_from_sb(search))) continue;
            height += f;
        }
        if (height < 0) {
            r++;
            
            array_foreach(album.tracks, j) {
                SongMetadata meta = array_get(album.tracks, j);
                if (meta.artist.size == 0) meta.artist = sv("<none>");
                if (meta.title.size == 0) meta.title = sv("<none>");
                
                if (search->size && !ui_match_song(meta, sv_from_sb(search))) continue;

                r++;
            }
            continue;
        }
        if (f*2.f + f*r+s > frame.height) break;
        bool hovered = ui_mouse_in(0, f*2.f + f*r + s, w, f) && ui_mouse_in_frame();
        Color gray = hovered ? theme->fg_off : theme->mg;
        if (hovered) ui_draw_rect(0, f*2.f + f*r + s, w, f, theme->mg_off);
        if (hovered) cursor = MOUSE_CURSOR_POINTING_HAND;
        float c = 0;
        c += ui_draw_text(f*0.5f + c, f*2.f + f*r+s, album.artists, theme->fg, 0, 0, w-f-c);
        c += ui_draw_text(f*0.5f + c, f*2.f + f*r+s, sv(" - "), gray, 0, 0, w-f-c);
        c += ui_draw_text(f*0.5f + c, f*2.f + f*r+s, album.name, theme->fg, 0, 0, w-f-c);
        c += ui_draw_text(f*0.5f + c, f*2.f + f*r+s, sv(" ("), gray, 0, 0, w-f-c);
        c += ui_draw_text(f*0.5f + c, f*2.f + f*r+s, album.year, gray, 0, 0, w-f-c);
        c += ui_draw_text(f*0.5f + c, f*2.f + f*r+s, sv(")"), gray, 0, 0, w-f-c);
        if ((first_album && search->size && search_focused && IsKeyUp(KEY_LEFT_SHIFT) && IsKeyPressed(KEY_ENTER)) || (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))) {
            search_focused = false;
            music_unload();
            while (playlist->size > 0) music_remove_from_playlist(0);
            array_foreach(album.tracks, s) { music_insert_into_playlist(array_get(album.tracks, s).path); }
        }
        if (first_album && search->size && search_focused) {
            first_album = false;
            ui_draw_text(w - f*0.5f, f*2.f + f*r + s, sv("Enter"), gray, 1, 0, 1e9);
        }
        
        SongCache* tracklist = album.tracks;
        r++;
        array_foreach(tracklist, j) {
            SongMetadata meta = array_get(tracklist, j);
            if (meta.artist.size == 0) meta.artist = sv("<none>");
            if (meta.title.size == 0) meta.title = sv("<none>");
            
            if (search->size && !ui_match_song(meta, sv_from_sb(search))) continue;
            bool hovered = ui_mouse_in(0, f*2.f + f*r + s, w, f) && ui_mouse_in_frame();
            Color gray = hovered ? theme->fg_off : theme->mg;
            if (hovered) ui_draw_rect(0, f*2.f + f*r + s, w, f, theme->mg_off);
            if (hovered) cursor = MOUSE_CURSOR_POINTING_HAND;
            c = 0;
            c += ui_draw_text(f*1.5f + c, f*2.f + f*r+s, sv((char*) TextFormat("%d. ", meta.track)), gray, 0, 0, w-f-c);
            c += ui_draw_text(f*1.5f + c, f*2.f + f*r+s, meta.artist, theme->fg, 0, 0, w-f*2.f-c);
            c += ui_draw_text(f*1.5f + c, f*2.f + f*r+s, sv(" - "), gray, 0, 0, w-f*2.f-c);
            c += ui_draw_text(f*1.5f + c, f*2.f + f*r+s, meta.title, theme->fg, 0, 0, w-f*2.f-c);
            if ((first_song && search->size && search_focused && IsKeyDown(KEY_LEFT_SHIFT) && IsKeyPressed(KEY_ENTER)) || (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))) {
                search_focused = false;
                music_unload();
                while (playlist->size > 0) music_remove_from_playlist(0);
                music_insert_into_playlist(meta.path);
            }
            if (first_song && search->size && search_focused) {
                first_song = false;
                ui_draw_text(w - f*0.5f, f*2.f + f*r + s, sv("Shift + Enter"), gray, 1, 0, 1e9);
            }
            
            r++;
        }
    }
    r = 0;
    array_foreach(albums, i) {
        Album album = array_get(albums, i);
        if (search->size && !ui_match_album(album, sv_from_sb(search))) continue;
        r++;
        SongCache* tracklist = album.tracks;
        array_foreach(tracklist, j) {
            SongMetadata meta = array_get(tracklist, j);
            if (search->size && !ui_match_song(meta, sv_from_sb(search))) continue;
            r++;
        }
    }
    ui_scrollable(f*2.5f + r*f, &browse_scroll);
    ui_scrollbar(f*2.5f + r*f, browse_scroll);
}

float about_scroll = 0.0f;

int theme_selected = 0;

void ui_set_icon(void) {
    Image window_icon = LoadImageFromMemory(".png", (unsigned char*) _ICON_PNG, _ICON_PNG_LENGTH);
    ImageResize(&window_icon, 32, 32);
    SetWindowIcon(window_icon);
    UnloadImage(window_icon);
}

typedef struct {
    String name, description, link;
    Theme colors;
} CustomTheme;
define_array_struct(CustomThemes, CustomTheme)

CustomThemes* themes;

StringArray* fonts;

void ui_theme_select(int t) {
    if (t == 0) theme = &dark_theme;
    if (t == 1) theme = &light_theme;
    if (t > 1) theme = &array_get(themes, t-2).colors;
}

bool ui_select(float x, float y, String str, Color bg, Color fg, int* select, int value) {
    float text_width = ui_measure_text(str);
    bool hovered = ui_mouse_in(x, y, text_width + font_size*0.5f, font_size) && ui_mouse_in_frame();
    bool sel = *select == value;
    ui_draw_rect(x, y, text_width + font_size*0.5f, font_size, sel ? fg : bg);
    ui_draw_text(x + font_size*0.25f, y, str, sel ? bg : fg, 0, 0, text_width);
    if (hovered) cursor = MOUSE_CURSOR_POINTING_HAND;
    if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) *select = value;
    return hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

bool ui_selectb(float x, float y, String str, Color bg, Color fg, bool sel) {
    float text_width = ui_measure_text(str);
    bool hovered = ui_mouse_in(x, y, text_width + font_size*0.5f, font_size) && ui_mouse_in_frame();
    ui_draw_rect(x, y, text_width + font_size*0.5f, font_size, sel ? fg : bg);
    ui_draw_text(x + font_size*0.25f, y, str, sel ? bg : fg, 0, 0, text_width);
    if (hovered) cursor = MOUSE_CURSOR_POINTING_HAND;
    return hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

void ss_load_themes(void);
void ss_load_fonts(void);

int font_selected = 0;

void ui_draw_scale_select(float* c, float* y, float sc) {
    Rectangle frame = ui_get_frame();
    float f = font_size, w = frame.width;
    String ui_scale = sv("UI scale:");
    ui_draw_text(0.5f*f, *y + sc, ui_scale, theme->fg, 0, 0, 1e9);
    *c += ui_measure_text(ui_scale) + 0.5f*f;
    float sizes[] = {16.f, 20.f, 24.f, 28.f, 32.f, 36.f, 40.f};
    size_t sizes_c = sizeof(sizes)/sizeof(*sizes);
    for (size_t i = 0; i < sizes_c; i++) {
	String str = sv((char*) TextFormat("%.1fx", sizes[i]/24.f));
	if (*c + ui_measure_text(str) + f*0.75f > w) {
	    *y += 1.25f*f; *c = 0.f;
	}
	float tx = 0.5f*f + *c, ty = *y + sc;
	if (ui_selectb(tx, ty, str, theme->mg_off, theme->fg, font_size == sizes[i])) ui_reload_font(sizes[i], font_selected ? array_get(fonts, font_selected-1) : (String) {0});
	*c += ui_measure_text(str) + f*0.75f;
    }
}

void ui_draw_font_select(float* c, float* y, float sc) {
    Rectangle frame = ui_get_frame();
    float f = font_size, w = frame.width;
    String label = sv("Select font:");
    ui_draw_text(0.5f*f, *y + sc, label, theme->fg, 0, 0, 1e9);
    *c += ui_measure_text(label) + 0.5f*f;
    String str = sv("default");
    if (*c + ui_measure_text(str) + f*0.75f > w) {
	*y += 1.25f*f; *c = 0.f;
    }
    float tx = 0.5f*f + *c, ty = *y + sc;
    if (ui_select(tx, ty, str, theme->mg_off, theme->fg, &font_selected, 0)) ui_reload_font(font_size, (String) {0});
    *c += ui_measure_text(str) + f*0.75f;
    array_foreach(fonts, i) {
	String font = array_get(fonts, i);
	String str = font;
	for (size_t i = 0; i < str.size; i++) {
	    if (sv_index(str, i) == '\\' || sv_index(str, i) == '/') {
		str.bytes += i+1;
		str.size -= i+1;
		i = 0;
	    }
	}
	for (size_t i = str.size; i > 0; i--) {
	    if (sv_index(str, i-1) == '.') {
		str.size = i-1;
		break;
	    }
	}
	if (*c + ui_measure_text(str) + f*0.75f > w) {
	    *y += 1.25f*f; *c = 0.f;
	}
	float tx = 0.5f*f + *c, ty = *y + sc;
	if (ui_select(tx, ty, str, theme->mg_off, theme->fg, &font_selected, i+1)) ui_reload_font(font_size, font);
	*c += ui_measure_text(str) + f*0.75f;
    }
    if (*c + f*1.75f > w) {
	*y += 1.25f*f; *c = 0.f;
    }
    tx = 0.5f*f + *c, ty = *y + sc;
    if (ui_draw_button(tx, ty, refresh, theme->mg_off, theme->mg, theme->fg, true)) ss_load_fonts();
    *c += ui_measure_text(str) + f*0.75f;
}

void ui_draw_theme_select(float* c, float* y, float sc) {
    Rectangle frame = ui_get_frame();
    float f = font_size;
    *c += ui_draw_text(0.5f*f + *c, *y + sc, sv("Select theme:"), theme->fg, 0, 0, 1e9);
    *c += f*0.25f;
    if (ui_select(0.5f*f + *c, *y + sc, sv("dark"), theme->mg_off, theme->fg, &theme_selected, 0)) ui_theme_select(theme_selected);
    *c += f*0.5f + ui_measure_text(sv("dark"));
    *c += f*0.25f;
    if (ui_select(0.5f*f + *c, *y + sc, sv("light"), theme->mg_off, theme->fg, &theme_selected, 1)) ui_theme_select(theme_selected);
    *c += f*0.5f + ui_measure_text(sv("light"));
    
    array_foreach(themes, i) {
        CustomTheme ct = array_get(themes, i);
        float w = ui_measure_text(ct.name);
        *c += f*0.25f;
        if (*c + f + w >= frame.width-f*0.5f) {
            *c = 0; *y += f*1.25f;
        }
        if (ui_select(0.5*f + *c, *y + sc, ct.name, theme->mg_off, theme->fg, &theme_selected, 2+i)) ui_theme_select(theme_selected);
        *c += f*0.5f + w;
        
    }
    *c += f*0.25f;
    if (*c + f*1.5f >= frame.width-f*0.5f) {
        *c = 0; *y += f*1.25f;
    }
    if (ui_draw_button(*c + f*0.5f, *y + sc, refresh, theme->mg_off, theme->mg, theme->fg, true)) {
        ss_load_themes();
        if (theme_selected > 1 && themes->size+2 <= (size_t) theme_selected) theme_selected = 0;
        ui_theme_select(theme_selected);
    }
}

void ui_draw_about(void) {
    Rectangle frame = ui_get_frame();

    float f = font_size, w = frame.width, sc = about_scroll;

    float y = 0.5f*f;
    float c = 0;
    ui_draw_theme_select(&c, &y, sc);
    y += font_size*1.5f;
    c = 0;
    ui_draw_font_select(&c, &y, sc);
    y += font_size*1.5f;
    c = 0;
    ui_draw_scale_select(&c, &y, sc);
    y += font_size*1.5f;
    c = 0;

    if (theme_selected > 1) {
        CustomTheme ct = array_get(themes, theme_selected-2);
        if (ct.description.size) {
            ui_draw_text(f*0.5f, y + sc, ct.description, theme->fg, 0, 0, frame.width-font_size);
            y += font_size;
        }
        if (ct.link.size) {
            float w = ui_draw_text(f*0.5f, y + sc, ct.link, theme->link, 0, 0, frame.width-font_size);
            bool hovered = ui_mouse_in(f*0.5f, y + sc, w, font_size) && ui_mouse_in_frame();
            if (hovered) cursor = MOUSE_CURSOR_POINTING_HAND;
            if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                char* cstr = sv_to_cstr(ct.link);
                OpenURL(cstr);
                free(cstr);
            }
            y += font_size;
        }
    }
    
    float start_text = y;
    
    String s = sv_from_bytes(_ABOUT_TXT, _ABOUT_TXT_LENGTH);

    size_t n = 0;
    while (s.size) {
        String line = sv_split(&s, sv("\n"));
        if (sv_endswith(line, sv("\r"))) line.size -= 1;
        if (sv_endswith(line, sv("\n"))) line.size -= 1;
        if (sv_endswith(line, sv("\r"))) line.size -= 1;
        if (sv_endswith(line, sv("\n"))) line.size -= 1;
        float c = 0;
        bool found = false;
        size_t start_i = 0, i = 0;
        for (; i < line.size; i++) {
            if (sv_compare_at(line, sv("https://"), i)) {
                start_i = i; found = true;
                i += sv("https://").size;
                while (i < line.size && (isalnum(line.bytes[i]) || line.bytes[i] == '/' || line.bytes[i] == '.')) i++;
                break;
            }
        }
        if (found) {
            c += ui_draw_text(f*0.5f + c, f*0.5f + n*f + start_text + sc, sv_from_bytes(line.bytes, start_i), theme->fg, 0, 0, w-f-c);
            float start_c = c;
            String l = sv_from_bytes(line.bytes + start_i, i - start_i);
            c += ui_draw_text(f*0.5f + c, f*0.5f + n*f + start_text + sc, l, theme->link, 0, 0, w-f-c);
            bool hovered = ui_mouse_in_frame() && ui_mouse_in(f*0.5f + start_c, f*0.5f + n*f + start_text + sc, c - start_c, f);
            c += ui_draw_text(f*0.5f + c, f*0.5f + n*f + start_text + sc, sv_from_bytes(line.bytes + i, line.size - i), theme->fg, 0, 0, w-f-c);
            if (hovered) cursor = MOUSE_CURSOR_POINTING_HAND;
            if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                char* c = sv_to_cstr(l);
                OpenURL(c);
                free(c);
            }
        } else ui_draw_text(f*0.5f + c, f*0.5f + n*f + start_text + sc, line, theme->fg, 0, 0, w-f-c);
        
        n++;
    }
    n++;
    ui_scrollable(n*f + start_text, &about_scroll);
    ui_scrollbar(n*f + start_text, about_scroll);
}

void ui_draw_main(void) {
    ui_add_frame(0, font_size*1.5f, GetScreenWidth(), GetScreenHeight() - font_size*5.f);
    ui_clear_frame(theme->bg);

    Rectangle frame = ui_get_frame();

    if (current_tab == TAB_PLAYLIST) ui_draw_playlist();
    else if (current_tab == TAB_ALBUMS) ui_draw_albums();
    else if (current_tab == TAB_BROWSE) ui_draw_browse();
    else if (current_tab == TAB_ABOUT) ui_draw_about();
    else ui_draw_text(frame.width/2, frame.height/2, sv(":)"), theme->fg, 0.5f, 0.5f, 1e9);
    
    ui_pop_frame();
}

void ui_draw(void) {
    spinny_degrees += GetFrameTime()*100.f;
    ui_draw_menubar();
    ui_draw_statusbar();
    ui_draw_main();
}

Texture ui_load_icon(char* ptr, size_t size) {
    Image image = LoadImageFromMemory(".png", (unsigned char*) ptr, size);
    ImageResize(&image, (int) font_size, (int) font_size);
    Texture texture = LoadTextureFromImage(image);
    UnloadImage(image);
    return texture;
}

void ui_load_icons(void) {
    play = ui_load_icon(_PLAY_PNG, _PLAY_PNG_LENGTH);
    pause = ui_load_icon(_PAUSE_PNG, _PAUSE_PNG_LENGTH);
    backward = ui_load_icon(_BACKWARD_PNG, _BACKWARD_PNG_LENGTH);
    forward = ui_load_icon(_FORWARD_PNG, _FORWARD_PNG_LENGTH);
    repeat = ui_load_icon(_REPEAT_PNG, _REPEAT_PNG_LENGTH);
    repeat_one = ui_load_icon(_REPEAT_ONE_PNG, _REPEAT_ONE_PNG_LENGTH);
    go_back = ui_load_icon(_GO_BACK_PNG, _GO_BACK_PNG_LENGTH);
    refresh = ui_load_icon(_REFRESH_PNG, _REFRESH_PNG_LENGTH);
    add = ui_load_icon(_ADD_PNG, _ADD_PNG_LENGTH);
}

void ui_unload_icons(void) {
    UnloadTexture(play);
    UnloadTexture(pause);
    UnloadTexture(backward);
    UnloadTexture(forward);
    UnloadTexture(repeat);
    UnloadTexture(repeat_one);
    UnloadTexture(go_back);
    UnloadTexture(refresh);
    UnloadTexture(add);
}

void ui_load_font(float fs) {
    font_size = fs;
    scroll_factor = font_size*3.0f;

    int codepoints[96 + 255] = {0};
    
    for (int i = 0; i < 95; i++) codepoints[i] = 32 + i;
    for (int i = 0; i < 255; i++) codepoints[96 + i] = 0x400 + i;
    
    font = LoadFontFromMemory(".ttf", (unsigned char*) _FONT_TTF, _FONT_TTF_LENGTH, font_size, codepoints, 96+255);
}

void ui_load_font_file(String name, float fs) {
    font_size = fs;
    scroll_factor = font_size*3.0f;

    int codepoints[96 + 255] = {0};
    
    for (int i = 0; i < 95; i++) codepoints[i] = 32 + i;
    for (int i = 0; i < 255; i++) codepoints[96 + i] = 0x400 + i;

    char* cstr = sv_to_cstr(name);
    font = LoadFontEx(cstr, font_size, codepoints, 96+255);
    free(cstr);
}

void ui_unload_font(void) {
    UnloadFont(font);
}

void ui_reload_font(float size, String name) {
    float old_size = font_size;
    ui_unload_font();
    if (name.size) ui_load_font_file(name, size);
    else ui_load_font(size);
    if (old_size != size) {
	music_resize_covers();
	ui_unload_icons();
	ui_load_icons();
    }
    SetWindowMinSize(ui_measure_text(sv("musplaylistalbumsbrowseabout100%")) + size*4.5f, size*12.5f);
}

void ui_load(void) {
    ds = array_new(&main_arena);
    search = array_new(&main_arena);
    album_add = array_new(&main_arena);
    themes = array_new(&main_arena);
    fonts = array_new(&main_arena);
    cwd = dir_get_cwd(&main_arena);
    ui_load_font(24.f);
    ui_load_icons();
    ui_set_icon();
}

void ui_unload(void) {
    ui_unload_font();
    ui_unload_icons();
}
