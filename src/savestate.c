
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
    
    fclose(f);
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
        *array_push(playlist) = ss_read_str(f);
    }
    playing = ss_read_int(f);
    if (playing >= 0) music_load(playing);
    music_set_volume(ss_read_float(f));
    if (playing >= 0) music_seek(ss_read_float(f));
    if (ss_read_char(f)) music_toggle_pause();
    music_set_repeat(ss_read_char(f));
    current_tab = ss_read_char(f);
    
    fclose(f);
}
