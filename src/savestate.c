
void ss_write_int(FILE* f, int num) {
    fwrite(&num, 1, sizeof(int), f);
}

void ss_write_char(FILE* f, char num) {
    fwrite(&num, 1, sizeof(char), f);
}

void ss_write_float(FILE* f, float num) {
    fwrite(&num, 1, sizeof(float), f);
}

void ss_write_str(FILE* f, String str) {
    ss_write_int(f, str.size);
    fwrite(str.bytes, str.size, 1, f);
}

int ss_read_int(FILE* f) {
    int num = 0;
    fread(&num, 1, sizeof(int), f);
    return num;
}

char ss_read_char(FILE* f) {
    char num = 0;
    fread(&num, 1, sizeof(char), f);
    return num;
}

float ss_read_float(FILE* f) {
    float num = 0;
    fread(&num, 1, sizeof(float), f);
    return num;
}

String ss_read_str(FILE* f) {
    int num = ss_read_int(f);
    String str = sv_from_bytes(arena_malloc(&main_arena, num), num);
    fread(str.bytes, str.size, 1, f);
    return str;
}

String ss_read_line(FILE* f) {
    StringBuilder* sb = array_new(&main_arena);
    char c = 0;
    fread(&c, 1, 1, f);
    while (c != '\n' && !feof(f)) {
        *array_push(sb) = c;
        fread(&c, 1, 1, f);
    }
    *array_push(sb) = 0;
    return sv(sb->data);
}

void savestate_save(void) {
    FILE* f = fopen(".mus-savestate", "wb");
    if (f == NULL) return;
    
    ss_write_int(f, albums->size);
    array_foreach(albums, i) {
        Album album = array_get(albums, i);
        ss_write_int(f, album.tracks->size);
        array_foreach(album.tracks, j) ss_write_str(f, array_get(album.tracks, j).path);
    }

    ss_write_int(f, playlist->size);
    array_foreach(playlist, i) {
        String path = array_get(playlist, i);
        ss_write_str(f, path);
    }
    ss_write_int(f, playing);
    ss_write_float(f, music_get_volume());
    if (playing >= 0) ss_write_float(f, music_get_playing_seek());
    ss_write_char(f, music_paused());
    ss_write_char(f, repeat_mode);
    ss_write_char(f, current_tab);
    ss_write_char(f, theme_selected);
    ss_write_char(f, font_selected);
    ss_write_float(f, font_size);
    
    fclose(f);
}

unsigned char ss_parse_hex_octet(char* c) {
    unsigned char color = 0;
    char f = c[0], s = c[1];
    if (f >= '0' && f <= '9') color += f-'0';
    else if (f >= 'a' && f <= 'f') color += f-'a'+10;
    else if (f >= 'A' && f <= 'F') color += f-'A'+10;
    color *= 16;
    if (s >= '0' && s <= '9') color += s-'0';
    else if (s >= 'a' && s <= 'f') color += s-'a'+10;
    else if (s >= 'A' && s <= 'F') color += s-'A'+10;
    return color;
}

Color ss_parse_color(String color) {
    if (!color.size) return (Color) {0, 0, 0, 255};
    if (*color.bytes == '#') {
        if (color.size == 7) {
            Color c = {0, 0, 0, 255};
            c.r = ss_parse_hex_octet(color.bytes + 1);
            c.g = ss_parse_hex_octet(color.bytes + 3);
            c.b = ss_parse_hex_octet(color.bytes + 5);
            return c;
        } else if (color.size == 9) {
            Color c = {0, 0, 0, 0};
            c.r = ss_parse_hex_octet(color.bytes + 1);
            c.g = ss_parse_hex_octet(color.bytes + 3);
            c.b = ss_parse_hex_octet(color.bytes + 5);
            c.b = ss_parse_hex_octet(color.bytes + 7);
            return c;
        } else return (Color) {0, 0, 0, 255};
    }
    return (Color) {0, 0, 0, 255};
}

void ss_load_themes(void) {
    themes->size = 0;
    if (!dir_exists(sv(".mus-themes"))) return;
    StringArray* d = dir_list(sv(".mus-themes"), &main_arena);
    dir_change_cwd(sv(".mus-themes"));
    
    array_foreach(d, i) {
        String file = array_get(d, i);
        if (dir_exists(file) || !sv_endswith(file, sv(".txt"))) continue;
	
        char* cstr = sv_to_cstr(file);
        FILE* f = fopen(cstr, "r");
        free(cstr);
        if (f == NULL) continue;

        CustomTheme ct = { sv("custom"), {0}, {0}, dark_theme };
        
        while (!feof(f) && !ferror(f)) {
            String value = ss_read_line(f);
            if (!value.size) continue;
            String key = sv_split(&value, sv("="));
            if (sv_compare(key, sv("name")))   ct.name = value;
            if (sv_compare(key, sv("description"))) ct.description = value;
            if (sv_compare(key, sv("author"))) ct.link = value;
            if (sv_compare(key, sv("bg")))     ct.colors.bg = ss_parse_color(value);
            if (sv_compare(key, sv("mg_off"))) ct.colors.mg_off = ss_parse_color(value);
            if (sv_compare(key, sv("mg")))     ct.colors.mg = ss_parse_color(value);
            if (sv_compare(key, sv("fg_off"))) ct.colors.fg_off = ss_parse_color(value);
            if (sv_compare(key, sv("fg")))     ct.colors.fg = ss_parse_color(value);
            if (sv_compare(key, sv("link")))   ct.colors.link = ss_parse_color(value);
        }
        
        *array_push(themes) = ct;
        fclose(f);
    }
    dir_change_cwd(sv(".."));
    ui_theme_select(theme_selected);
}

void ss_load_fonts(void) {
    fonts->size = 0;
    if (!dir_exists(sv(".mus-themes"))) return;
    StringArray* d = dir_list(sv(".mus-themes"), &main_arena);
    dir_change_cwd(sv(".mus-themes"));
    
    array_foreach(d, i) {
        String file = array_get(d, i);
        if (dir_exists(file) || !sv_endswith(file, sv(".ttf"))) continue;
	*array_push(fonts) = arena_sprintf(&main_arena, ".mus-themes/"SV_FMT, SvFmt(file));
    }
    dir_change_cwd(sv(".."));
}

void savestate_load(void) {
    FILE* f = fopen(".mus-savestate", "rb");
    if (f == NULL) return;
    
    int albums_size = ss_read_int(f);
    SetTraceLogLevel(LOG_NONE);
    for (int i = 0; i < albums_size; i++) {
        int tracklist_size = ss_read_int(f);
        for (int j = 0; j < tracklist_size; j++) music_scan_file(ss_read_str(f));
    }
    SetTraceLogLevel(LOG_INFO);

    int playlist_size = ss_read_int(f);
    for (int i = 0; i < playlist_size; i++) {
        music_insert_into_playlist(ss_read_str(f));
    }
    playing = ss_read_int(f);
    if (playing >= 0) music_load(playing);
    music_set_volume(ss_read_float(f));
    if (playing >= 0) music_seek(ss_read_float(f));
    if (ss_read_char(f)) music_toggle_pause();
    music_set_repeat(ss_read_char(f));
    current_tab = ss_read_char(f);
    theme_selected = ss_read_char(f);
    if (theme_selected < 2) ui_theme_select(theme_selected);
    font_selected = ss_read_char(f);
    font_size = ss_read_float(f);
    
    fclose(f);

    ss_load_themes();
    ss_load_fonts();
    ui_reload_font(font_size, font_selected ? array_get(fonts, font_selected-1) : (String) {0});

    if (theme_selected > 1 && themes->size+2 <= (size_t) theme_selected) {
        theme_selected = 0;
        ui_theme_select(theme_selected);
    }
}
