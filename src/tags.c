
unsigned int parse_syncsafe_int(unsigned int value) {
    unsigned int a, b, c, d, result = 0;
    a = value & 0xFF;
    b = (value >> 8) & 0xFF;
    c = (value >> 16) & 0xFF;
    d = (value >> 24) & 0xFF;

    result = result | a;
    result = result | (b << 7);
    result = result | (c << 14);
    result = result | (d << 21);

    return result;
}

unsigned int btoi(char* bytes, int size) {
    unsigned int result = 0;

    for (int i = 0; i < size; i++) {
        result = result << 8;
        result = result | (unsigned char) bytes[i];
    }

    return result;
}

unsigned int btoi_be(char* bytes, int size) {
    unsigned int result = 0;

    for (int i = size; i >= 0; i--) {
        result = result << 8;
        result = result | (unsigned char) bytes[i];
    }

    return result;
}

typedef struct {
    String data;
    String mime_type;
} TagImage;

String tags_id3v2_get_frame(Arena* arena, String path, String frame);
TagImage tags_id3v2_get_pic_frame(Arena* arena, String path, int pic_type);
String tags_flac_get_tag(Arena* arena, String path, String name);
TagImage tags_flac_get_picture(Arena* arena, String path, uint32_t pic_type);

String tags_get_artist(Arena* arena, String path) {
    if (sv_endswith(path, sv(".flac"))) return tags_flac_get_tag(arena, path, sv("ARTIST"));
    return tags_id3v2_get_frame(arena, path, sv("TPE1"));
}

String tags_get_album_artist(Arena* arena, String path) {
    String str = {0};
    if (sv_endswith(path, sv(".flac"))) str = tags_flac_get_tag(arena, path, sv("ALBUMARTIST"));
    else str = tags_id3v2_get_frame(arena, path, sv("TPE2"));
    if (!str.size) return tags_get_artist(arena, path);
    return str;
}

String tags_get_title(Arena* arena, String path) {
    if (sv_endswith(path, sv(".flac"))) return tags_flac_get_tag(arena, path, sv("TITLE"));
    return tags_id3v2_get_frame(arena, path, sv("TIT2"));
}

String tags_get_album(Arena* arena, String path) {
    if (sv_endswith(path, sv(".flac"))) return tags_flac_get_tag(arena, path, sv("ALBUM"));
    return tags_id3v2_get_frame(arena, path, sv("TALB"));
}

size_t tags_get_track_number(Arena* arena, String path) {
    String str = {0};
    if (sv_endswith(path, sv(".flac"))) str = tags_flac_get_tag(arena, path, sv("TRACKNUMBER"));
    else str = tags_id3v2_get_frame(arena, path, sv("TRCK"));
    while (str.size && str.bytes[0] == '0') { str.bytes++; str.size--; }
    return sv_to_int(str);
}

String tags_get_year(Arena* arena, String path) {
    if (sv_endswith(path, sv(".flac"))) return tags_flac_get_tag(arena, path, sv("DATE"));
    return tags_id3v2_get_frame(arena, path, sv("TYER"));
}

TagImage tags_get_cover_art(Arena* arena, String path) {
    if (sv_endswith(path, sv(".flac"))) return tags_flac_get_picture(arena, path, 3);
    return tags_id3v2_get_pic_frame(arena, path, 3);
}

String tags_flac_get_tag(Arena* arena, String path, String name) {
    char* cstr = sv_to_cstr(path);
    FILE* f = fopen(cstr, "rb");
    if (f == NULL) return sv_from_bytes(NULL, 0);

    char magic[4];
    fread(magic, 1, 4, f);
    if (memcmp(magic, "fLaC", 4) != 0) return sv_from_bytes(NULL, 0);

    String result = {0};
    while (true) {
        char metadata_type;
        fread(&metadata_type, 1, 1, f);

        char metadata_length[3];
        fread(metadata_length, 1, 3, f);
        int length = btoi(metadata_length, 3);

        char metadata[length];
        fread(metadata, 1, length, f);

        if ((metadata_type & 0b01111111) == 4) {
            size_t c = 0;
            uint32_t vendor_length = btoi_be(metadata + c, 4);
            c += 4 + vendor_length;
            uint32_t comments = btoi_be(metadata + c, 4);
            c += 4;
            for (size_t i = 0; i < comments; i++) {
                uint32_t length = btoi_be(metadata + c, 4);
                c += 4;
                char string[length];
                memcpy(string, metadata + c, length);
                String key = sv_from_bytes(string, length);
                String value = sv_split(&key, sv("="));
                if (sv_compare(key, name)) {
                    char* str = arena_malloc(arena, value.size);
                    memcpy(str, value.bytes, value.size);
                    result = sv_from_bytes(str, value.size);
                }
                c += length;
            }
        }
        if (metadata_type & 0b10000000) break;
    }
    
    fclose(f);
    return result;
}

TagImage tags_flac_get_picture(Arena* arena, String path, uint32_t pic_type) {
    char* cstr = sv_to_cstr(path);
    FILE* f = fopen(cstr, "rb");
    if (f == NULL) return (TagImage) {sv_from_bytes(NULL, 0), sv_from_bytes(NULL, 0)};

    char magic[4];
    fread(magic, 1, 4, f);
    if (memcmp(magic, "fLaC", 4) != 0) return (TagImage) {sv_from_bytes(NULL, 0), sv_from_bytes(NULL, 0)};

    TagImage result = {0};
    while (true) {
        char metadata_type;
        fread(&metadata_type, 1, 1, f);

        char metadata_length[3];
        fread(metadata_length, 1, 3, f);
        int length = btoi(metadata_length, 3);

        char metadata[length];
        fread(metadata, 1, length, f);

        if ((metadata_type & 0b01111111) == 6) {
            size_t c = 0;
            uint32_t type = btoi(metadata + c, 4);
            c += 4;
            if (type != pic_type) continue;
            uint32_t mime_size = btoi(metadata + c, 4);
            c += 4;
            char mime[mime_size];
            memcpy(mime, metadata + c, mime_size);
            c += mime_size;
            uint32_t d_length = btoi(metadata + c, 4);
            c += 4 + d_length + 16;
            uint32_t data_length = btoi(metadata + c, 4);
            c += 4;
            char data[data_length];
            memcpy(data, metadata + c, data_length);

            char* mc = arena_malloc(arena, mime_size);
            memcpy(mc, mime, mime_size);
            result.mime_type = sv_from_bytes(mc, mime_size);
            char* dc = arena_malloc(arena, data_length);
            memcpy(dc, data, data_length);
            result.data = sv_from_bytes(dc, data_length);
            break;
        }
        if (metadata_type & 0b10000000) break;
    }
    
    fclose(f);
    return result;
}

String tags_utf16_to_utf8(Arena* arena, String utf16);

String tags_id3v2_get_frame(Arena* arena, String path, String frame) {
    char* cstr = sv_to_cstr(path);
    FILE* f = fopen(cstr, "rb");
    free(cstr);
    if (f == NULL) return sv_from_bytes(NULL, 0);
    
    char magic[3];
    fread(magic, 3, 1, f);
    if (memcmp(magic, "ID3", 3) != 0) return sv_from_bytes(NULL, 0);

    char version[2];
    fread(version, 2, 1, f);
    if (*version != 3 && *version != 4) return sv_from_bytes(NULL, 0);

    char major = *version;
    
    char flags;
    fread(&flags, 1, 1, f);

    bool unsync = false, extended = false, footer = false;
    
    if (flags & 0b10000000) unsync = true;
    if (flags & 0b1000000) extended = true;
    if (flags & 0b10000) footer = true;

    if (unsync) return sv_from_bytes(NULL, 0);

    char size_bytes[4];
    fread(size_bytes, 4, 1, f);
    unsigned int size = parse_syncsafe_int(btoi(size_bytes, 4));

    if (extended) {
        char eh_size_bytes[4];
        fread(eh_size_bytes, 4, 1, f);
        unsigned int eh_size = parse_syncsafe_int(btoi(size_bytes, 4));
        fseek(f, eh_size, SEEK_CUR);
    }

    String result = {0};

    (void)footer;
    
    while (ftell(f) < (long) size) {
        char id[4];
        fread(id, 4, 1, f);
        if (memcmp(id, "\0\0\0\0", 4) == 0) break;
        String id_str = sv_from_bytes(id, 4);
        char size[4];
        fread(size, 4, 1, f);
        unsigned int frame_size = btoi(size, 4);
        if (major == 4) frame_size = parse_syncsafe_int(frame_size);
        char flags[2];
        fread(flags, 2, 1, f);
        if (*id != 'T') { fseek(f, frame_size, SEEK_CUR); continue; }
        char encoding;
        fread(&encoding, 1, 1, f);
        char text[frame_size-1];
        fread(text, frame_size-1, 1, f);
        if (sv_compare(frame, id_str)) {
            if (encoding == 0) {
                result.bytes = arena_malloc(arena, frame_size-1);
                memcpy(result.bytes, text, frame_size-1);
                result.size = frame_size-1;
            } else result = tags_utf16_to_utf8(arena, sv_from_bytes(text, frame_size-1));
            break;
        }
    }

    fclose(f);
    return result;
}

String utf16_readstr(Arena* arena, FILE* f) {
    StringBuilder* sb = array_new(StringBuilder, arena);
    char word[2];
    fread(word, 1, 2, f);
    while (!(word[0] == 0 && word[1] == 0)) {
        *array_push(sb) = word[0];
        *array_push(sb) = word[1];
        fread(word, 1, 2, f);
    }
    *array_push(sb) = 0;
    *array_push(sb) = 0;
    return sv_from_sb(sb);
}

String utf8_readstr(Arena* arena, FILE* f) {
    StringBuilder* sb = array_new(StringBuilder, arena);
    char octet;
    fread(&octet, 1, 1, f);
    while (octet != 0) {
        *array_push(sb) = octet;
        fread(&octet, 1, 1, f);
    }
    return sv_from_sb(sb);
}

TagImage tags_id3v2_get_pic_frame(Arena* arena, String path, int pic_type) {
    String frame = sv("APIC");
    char* cstr = sv_to_cstr(path);
    FILE* f = fopen(cstr, "rb");
    free(cstr);
    if (f == NULL) return (TagImage) {sv_from_bytes(NULL, 0), sv_from_bytes(NULL, 0)};
    
    char magic[3];
    fread(magic, 3, 1, f);
    if (memcmp(magic, "ID3", 3) != 0) return (TagImage) {sv_from_bytes(NULL, 0), sv_from_bytes(NULL, 0)};

    char version[2];
    fread(version, 2, 1, f);
    if (*version != 3 && *version != 4) return (TagImage) {sv_from_bytes(NULL, 0), sv_from_bytes(NULL, 0)};

    char major = *version;
    
    char flags;
    fread(&flags, 1, 1, f);

    bool unsync = false, extended = false, footer = false;
    
    if (flags & 0b10000000) unsync = true;
    if (flags & 0b1000000) extended = true;
    if (flags & 0b10000) footer = true;

    if (unsync) return (TagImage) {sv_from_bytes(NULL, 0), sv_from_bytes(NULL, 0)};

    char size_bytes[4];
    fread(size_bytes, 4, 1, f);
    unsigned int size = parse_syncsafe_int(btoi(size_bytes, 4));

    if (extended) {
        char eh_size_bytes[4];
        fread(eh_size_bytes, 4, 1, f);
        unsigned int eh_size = parse_syncsafe_int(btoi(size_bytes, 4));
        fseek(f, eh_size, SEEK_CUR);
    }

    TagImage result = {0};

    (void)footer;
    
    while (size) {
        char id[4];
        fread(id, 4, 1, f);
        if (memcmp(id, "\0\0\0\0", 4) == 0) break;
        String id_str = sv_from_bytes(id, 4);
        char size[4];
        fread(size, 4, 1, f);
        unsigned int frame_size = btoi(size, 4);
        if (major == 4) frame_size = parse_syncsafe_int(frame_size);
        char flags[2];
        fread(flags, 2, 1, f);
        if (!sv_compare(frame, id_str)) { fseek(f, frame_size, SEEK_CUR); continue; }
        unsigned char etype;
        fread(&etype, 1, 1, f);
        String mime_type = utf8_readstr(arena, f);
        unsigned char type;
        fread(&type, 1, 1, f);
        (void)pic_type; //if (type != pic_type) { fseek(f, frame_size - 2 - mime_type.size, SEEK_CUR); continue; }
        String description = etype == 0 ? utf8_readstr(arena, f) : utf16_readstr(arena, f);

        uint32_t pic_size = frame_size - 1 - mime_type.size - 1 - 1 - description.size - (etype == 0);
        char pic[pic_size];
        fread(pic, 1, pic_size, f);

        result.mime_type = mime_type;
        result.data.bytes = arena_malloc(arena, pic_size);
        memcpy(result.data.bytes, pic, pic_size);
        result.data.size = pic_size;
        break;
    }

    fclose(f);
    return result;
}

void tags_write_utf8_codepoint(StringBuilder* sb, unsigned int codepoint) {
    if (codepoint < 0x80) *array_push(sb) = codepoint & 0xFF;
    else if (codepoint < 0x800) {
        *array_push(sb) = 0b11000000 | ((codepoint >> 6) & 0b11111);
        *array_push(sb) = 0b10000000 | (codepoint & 0b111111);
    } else if (codepoint < 0x10000) {
        *array_push(sb) = 0b11100000 | ((codepoint >> 12) & 0b1111);
        *array_push(sb) = 0b10000000 | ((codepoint >> 6) & 0b111111);
        *array_push(sb) = 0b10000000 | (codepoint & 0b111111);
    } else {
        *array_push(sb) = 0b11110000 | ((codepoint >> 18) & 0b111);
        *array_push(sb) = 0b10000000 | ((codepoint >> 12) & 0b111111);
        *array_push(sb) = 0b10000000 | ((codepoint >> 6) & 0b111111);
        *array_push(sb) = 0b10000000 | (codepoint & 0b111111);
    }
}

String tags_utf16_to_utf8(Arena* arena, String utf16) {
    if (utf16.size == 0) return utf16;
    StringBuilder* sb = array_new(StringBuilder, arena);
    bool big_endian = sv_startswith(utf16, sv("\xFF\xFE"));
    utf16.bytes += 2;
    utf16.size -= 2;
    while (utf16.size > 0 && memcmp(utf16.bytes, "\0\0", 2) != 0) {
        unsigned short codepoint = btoi(utf16.bytes, 2);
        if (big_endian) codepoint = ((codepoint & 0xFF) << 8) | ((codepoint >> 8) & 0xFF);
        utf16.bytes += 2;
        utf16.size -= 2;
        
        if (codepoint < 0xD800 || codepoint > 0xDFFF) tags_write_utf8_codepoint(sb, codepoint);
        else if (codepoint > 0xDC00) {}
        else {
            unsigned int code = (unsigned int) (codepoint & 0x3FF) << 10;
            unsigned short trailing = btoi(utf16.bytes, 2);
            if (big_endian) trailing = ((trailing & 0xFF) << 8) | ((trailing >> 8) & 0xFF);
            utf16.bytes += 2;
            utf16.size -= 2;
            if (trailing >= 0xDC00 && trailing <= 0xDFFF) tags_write_utf8_codepoint(sb, (code | (trailing & 0x3FF)) + 0x10000);
        }
    }
    return sv_from_sb(sb);
}
