
typedef struct {
    String path;
    String artist, title, album_artist, album_name, year;
    Image cover_art;
    Texture cover_art_6x;
    Texture cover_art_8x;
    size_t track;
} SongMetadata;

define_array_struct(SongCache, SongMetadata)
// TODO: use a hashmap

SongCache* cache;

StringArray* playlist;
int playing = -1; // index into playlist
// if < 0 then nothing is loaded

Music current;

typedef enum {
    REPEAT_NONE = 0,
    REPEAT_PLAYLIST,
    REPEAT_ONE,
    COUNT_REPEAT
} RepeatMode;

RepeatMode repeat_mode;
float volume = 1.0f;

typedef struct {
    String name, artists, year;
    Image cover;
    Texture cover_x6, cover_x8;
    SongCache* tracks;
} Album;

define_array_struct(Albums, Album)

Albums* albums;

U32Array* deferred_cover_array;

void music_load_coverart(size_t cache_index) {
    SetTraceLogLevel(LOG_NONE);
    SongMetadata meta = array_get(cache, cache_index);
    TagImage im = tags_get_cover_art(&main_arena, meta.path);
    Image img = {0};
    if (sv_compare(im.mime_type, sv("image/jpeg")) || sv_compare(im.mime_type, sv("image/jpg"))) img = LoadImageFromMemory(".jpg", (unsigned char*) im.data.bytes, im.data.size);
    else if (sv_compare(im.mime_type, sv("image/png"))) img = LoadImageFromMemory(".png", (unsigned char*) im.data.bytes, im.data.size);
    Image img_copy = ImageCopy(img);
    ImageResize(&img_copy, font_size*8.f, font_size*8.f);
    Texture t3 = LoadTextureFromImage(img_copy);
    ImageResize(&img_copy, font_size*6.f, font_size*6.f);
    Texture t2 = LoadTextureFromImage(img_copy);
    UnloadImage(img_copy);
    meta.cover_art = img;
    meta.cover_art_6x = t2;
    meta.cover_art_8x = t3;
    *array_set(cache, cache_index) = meta;
    array_foreach(albums, i) {
        Album album = array_get(albums, i);
        SongCache* tracklist = album.tracks;
        array_foreach(tracklist, j) {
            SongMetadata m = array_get(tracklist, j);
            if (!sv_compare(m.path, meta.path)) continue;
            if (album.cover.width != 0) break;
            if (sv_compare(m.path, meta.path)) {
                album.cover = img;
                album.cover_x6 = t2;
                album.cover_x8 = t3;
                *array_set(albums, i) = album;
                break;
            }
        }
    }
    SetTraceLogLevel(LOG_INFO);
}

SongMetadata music_get_metadata(String path) {
    // First, check the cache
    array_foreach(cache, i) {
        SongMetadata meta = array_get(cache, i);
        if (sv_compare(path, meta.path)) return meta;
    }
    // No cached meta found, construct one from scratch
    // Defer cover loading to the next frame
    *array_push(deferred_cover_array) = (size_t) cache->size;
    SongMetadata meta = {
        path, tags_get_artist(&main_arena, path), tags_get_title(&main_arena, path),
        tags_get_album_artist(&main_arena, path), tags_get_album(&main_arena, path),
        tags_get_year(&main_arena, path),
        {0}, {0}, {0},
        tags_get_track_number(&main_arena, path),
    };
    *array_push(cache) = meta;
    return meta;
}

void music_scan_file(String path) {
    SongMetadata meta = music_get_metadata(path);
    array_foreach(albums, i) {
        Album album = array_get(albums, i);
        for (size_t j = 0; j < album.tracks->size; j++) {
            if (sv_compare(array_get(album.tracks, j).path, meta.path)) return;
        }
        if (sv_compare(album.name, meta.album_name) && sv_compare(album.artists, meta.album_artist)) {
            *array_push(array_set(albums, i)->tracks) = meta;
            return;
        }
    }
    SongCache* tracklist = array_new(SongCache, &main_arena);
    *array_push(tracklist) = meta;
    Album album = {
        meta.album_name, meta.album_artist, meta.year,
        meta.cover_art, meta.cover_art_6x, meta.cover_art_8x,
        tracklist
    };
    *array_push(albums) = album;
}

void music_scan_folder(String path) {
    if (!dir_exists(path)) return;
    StringArray* list = dir_list(path, &main_arena);
    array_foreach(list, i) {
        String file = array_get(list, i);
        if (sv_compare(file, sv(".")) || sv_compare(file, sv(".."))) continue;
        char* pc = arena_malloc(&main_arena, path.size + 1 + file.size);
        memcpy(pc, path.bytes, path.size);
        pc[path.size] = '/';
        memcpy(pc + path.size + 1, file.bytes, file.size);
        String p = sv_from_bytes(pc, path.size + 1 + file.size);
        if (dir_exists(p)) { music_scan_folder(p); continue; }
        if (file_exists(p) && (sv_endswith(file, sv(".flac")) || sv_endswith(file, sv(".mp3")))) music_scan_file(p);
    }
}

bool music_compare_alphabetically(String a, String b) {
    size_t a_offset = 0, b_offset = 0;
    while (a_offset < a.size && b_offset < b.size) {
        int a_size = 0, a_codepoint = GetCodepointNext(a.bytes + a_offset, &a_size);
        int b_size = 0, b_codepoint = GetCodepointNext(b.bytes + b_offset, &b_size);
        if (a_codepoint > b_codepoint) return true;
        a_offset += a_size;
        b_offset += b_size;
    }
    return false;
}

void music_sort_albums(void) {
    array_foreach(albums, i) {
        Album album = array_get(albums, i);
        SongCache* tracklist = album.tracks;
        for (size_t j = 0; j < tracklist->size; j++) {
            for (size_t i = 0; i < tracklist->size-1; i++) {
                if (array_get(tracklist, i).track > array_get(tracklist, i+1).track) {
                    SongMetadata temp = array_get(tracklist, i);
                    *array_set(tracklist, i) = array_get(tracklist, i+1);
                    *array_set(tracklist, i+1) = temp;
                }
            }
        }
    }
}

SongMetadata music_get_playing_metadata(void) {
    if (playing < 0) return (SongMetadata) { sv(""), sv(""), sv(""), sv(""), sv(""), sv(""), (Image) {0}, (Texture) {0}, (Texture) {0}, 0 };
    return music_get_metadata(array_get(playlist, playing));
}

float music_get_playing_seek(void) {
    if (playing < 0) return 0;
    return GetMusicTimePlayed(current);
}

float music_get_playing_length(void) {
    if (playing < 0) return 0;
    return GetMusicTimeLength(current);
}

void music_seek(float seek) {
    SeekMusicStream(current, seek);
}

void music_set_repeat(RepeatMode r) {
    repeat_mode = r;
}

RepeatMode music_repeat(void) { return repeat_mode; }

bool paused = false;

void music_load(int playlist_index) {
    // Load Music stream
    char* cstr = sv_to_cstr(array_get(playlist, playlist_index));
    current = LoadMusicStream(cstr);
    SetMusicVolume(current, volume);
    current.looping = false;
    PlayMusicStream(current);
    free(cstr);
    playing = playlist_index;
    paused = false;
}

void music_set_volume(float vol) {
    volume = vol;
    if (playing < 0) return;
    SetMusicVolume(current, volume);
}

float music_get_volume(void) {
    return volume;
}

void music_toggle_pause(void) {
    if (playing < 0) return;
    if (paused) PlayMusicStream(current);
    else PauseMusicStream(current);
    paused = !paused;
}

bool music_paused(void) { return playing < 0 || paused; }

bool music_is_playing(void) {
    return playing >= 0;
}

void music_unload(void) {
    if (playing < 0) return;
    playing = -1;
    UnloadMusicStream(current);
}

void music_remove_from_playlist(size_t index) {
    if (playing == (int) index) music_unload();
    if (playing > (int) index) playing--;
    array_remove(playlist, index);
}

void music_insert_into_playlist(String path) {
    char* copy = arena_malloc(&main_arena, path.size);
    memcpy(copy, path.bytes, path.size);
    String copied_path = sv_from_bytes(copy, path.size);
    *array_push(playlist) = copied_path;
    if (playing < 0) music_load(playlist->size-1);
}

void music_init(void) {
    playlist = array_new(StringArray, &main_arena);
    cache = array_new(SongCache, &main_arena);
    albums = array_new(Albums, &main_arena);
    deferred_cover_array = array_new(U32Array, &main_arena);
}

void music_playlist_backward(void) {
    if (playing <= 0) return;
    music_load(--playing);
}

void music_playlist_forward(void) {
    if (playing < 0 || (size_t) playing >= playlist->size-1) return;
    music_load(++playing);
}

void music_deinit(void) {
    music_unload();
    
    SetTraceLogLevel(LOG_NONE);
    array_foreach(cache, i) {
        SongMetadata song = array_get(cache, i);
        UnloadTexture(song.cover_art_6x);
        UnloadTexture(song.cover_art_8x);
        UnloadImage(song.cover_art);
    }
    SetTraceLogLevel(LOG_INFO);
}

void music_update(void) {
    if (playing >= 0) UpdateMusicStream(current);
    if (playing >= 0 && !paused && !IsMusicStreamPlaying(current)) {
        size_t old_playing = playing;
        music_unload();
        playing = old_playing;
        printf("EOF, repeat mode: ");
        if (repeat_mode == REPEAT_PLAYLIST) {
            printf("playlist, old playing: %d, ", playing);
            playing = (playing+1) % (int) playlist->size;
            printf("playing: %d\n", playing);
        } else if (repeat_mode == REPEAT_NONE) {
            printf("none, old playing: %d, ", playing);
            if (playing < (int)playlist->size-1) playing += 1;
            else playing = -1;
            printf("playing: %d\n", playing);
        } else {
            printf("one, playing: %d\n", playing);
        }
        if (playing >= 0) music_load(playing);
    }
}
