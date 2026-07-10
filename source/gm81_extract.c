// gm81_extract.c -- reads sprites/sounds directly out of a GM8.1 exe at
// runtime, so the port never bundles/redistributes the original game's
// copyrighted assets. Algorithm ported from OpenGMK's gm8exe crate
// (https://github.com/OpenGMK/OpenGMK, gm8exe/src/{gamedata/gm81.rs,
// gamedata/gm80.rs, reader.rs, asset/{sprite,sound,extension}.rs}),
// verified byte-for-bit correct against a real exe before this port.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <zlib.h>
#include "gm81_extract.h"

// ---- tiny cursor helpers ---------------------------------------------
typedef struct { uint8_t* d; size_t len; size_t pos; } Cur;
static uint32_t ru32(Cur* c){ uint32_t v; memcpy(&v, c->d+c->pos, 4); c->pos+=4; return v; }
static int32_t  ri32(Cur* c){ int32_t v; memcpy(&v, c->d+c->pos, 4); c->pos+=4; return v; }
static void     skip(Cur* c, size_t n){ c->pos += n; }
static uint8_t* take(Cur* c, size_t n){ uint8_t* p = c->d+c->pos; c->pos += n; return p; }
// GM8's "pascal string": u32 length + raw bytes, NOT null-terminated.
static char* pas_string(Cur* c){
    uint32_t n = ru32(c);
    char* s = (char*)malloc(n+1);
    memcpy(s, c->d+c->pos, n);
    s[n] = 0;
    c->pos += n;
    return s;
}

// ---- GM8.1 XOR-protection removal (gm81.rs decrypt / check) -----------
static uint32_t crc_table[256];
static void build_crc_table(void)
{
    for (int i = 0; i < 256; i++) {
        uint32_t c = (uint32_t)i;
        for (int k = 0; k < 8; k++)
            c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        crc_table[i] = c;
    }
}
// NOTE: this is exactly zlib's standard reflected CRC-32 table/algorithm
// (init 0xFFFFFFFF, no final xor -- YYG's variant skips the final ~result
// that a "textbook" CRC32 does). crc32() from zlib itself, called with
// the right init/final handling, gives an identical result, but we spell
// it out here to match the ported algorithm exactly and avoid relying on
// zlib CRC's own finalization behavior.
static uint32_t yyg_crc32(const uint8_t* data, size_t len)
{
    uint32_t result = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++)
        result = (result >> 8) ^ crc_table[(result & 0xFF) ^ data[i]];
    return result;
}

// Finds the 8-byte GM81 magic marker from `pos` onward. Returns the byte
// offset just past the match, or (size_t)-1 if not found.
static size_t seek_value(uint8_t* d, size_t len, size_t pos, uint32_t value)
{
    while (pos + 8 < len) {
        uint32_t d1, d2; memcpy(&d1, d+pos, 4); memcpy(&d2, d+pos+4, 4);
        uint32_t parsed = (d1 & 0xFF00FF00u) | (d2 & 0x00FF00FFu);
        if (parsed == value) return pos + 8;
        pos++;
    }
    return (size_t)-1;
}

// XOR-decrypts in place from `encryption_start` (derived internally) to
// EOF. Returns the cursor position right after reading hash_key+seed1
// (matches OpenGMK's decrypt(), which leaves the stream cursor there --
// the caller then does += 20 to reach the real gamedata start).
static size_t gm81_xor_decrypt(uint8_t* d, size_t len, size_t pos)
{
    uint32_t hk_num; memcpy(&hk_num, d+pos, 4); pos += 4;
    char hash_key[32];
    snprintf(hash_key, sizeof hash_key, "_MJD%u#RWK", hk_num);
    size_t hklen = strlen(hash_key);
    uint8_t* hk16 = (uint8_t*)malloc(hklen*2);
    for (size_t i = 0; i < hklen; i++) { hk16[i*2]=(uint8_t)hash_key[i]; hk16[i*2+1]=0; }

    uint32_t seed1; memcpy(&seed1, d+pos, 4); pos += 4;
    uint32_t seed2 = yyg_crc32(hk16, hklen*2);
    free(hk16);

    size_t encryption_start = pos + (seed2 & 0xFF) + 10;

    uint32_t s1 = seed1, s2 = seed2;
    size_t end = len - ((len - encryption_start) % 4);
    for (size_t off = encryption_start; off < end; off += 4) {
        s1 = (0xFFFFu & s1) * 0x9069u + (s1 >> 16);
        s2 = (0xFFFFu & s2) * 0x4650u + (s2 >> 16);
        uint32_t mask = (s1 << 16) + (s2 & 0xFFFFu);
        uint32_t dw; memcpy(&dw, d+off, 4);
        dw ^= mask;
        memcpy(d+off, &dw, 4);
    }
    return pos;
}

// Locates + removes GM8.1 protection. Returns byte offset of the real
// gamedata stream (start of `settings_len`), or (size_t)-1 on failure.
static size_t gm81_check(uint8_t* d, size_t len)
{
    static const uint8_t seq[8] = {0xE8,0x80,0xF2,0xDD,0xFF,0xC7,0x45,0xF0};
    if (len < 0x226D8A) return (size_t)-1;
    if (memcmp(d+0x00226CF3, seq, 8) != 0) return (size_t)-1;

    uint32_t header_start; memcpy(&header_start, d+0x00226CF3+8, 4);
    size_t p = 0x00226CF3 + 8 + 4 + 125;
    static const uint8_t cmp[3] = {0x81,0x7D,0xEC};
    if (memcmp(d+p, cmp, 3) != 0) return (size_t)-1;
    uint32_t magic; memcpy(&magic, d+p+3, 4);
    if (d[p+3+4] != 0x74) return (size_t)-1;

    size_t found = seek_value(d, len, header_start, magic);
    if (found == (size_t)-1) return (size_t)-1;

    size_t pos = gm81_xor_decrypt(d, len, found);
    pos += 20;
    return pos;
}

// ---- gm80-style swap-table decryption (always applied, even for 8.1) --
// Decrypts the whole remaining asset region in place; leaves cursor
// unchanged from where it started (same as the Rust version).
static size_t gm80_decrypt(uint8_t* d, size_t pos)
{
    uint32_t garbage1 = 4 * (uint32_t)( *(uint32_t*)(d+pos) ); pos += 4;
    uint32_t garbage2 = 4 * (uint32_t)( *(uint32_t*)(d+pos) ); pos += 4;
    pos += garbage1;
    uint8_t swap_table[256];
    memcpy(swap_table, d+pos, 256); pos += 256;
    pos += garbage2;
    uint32_t length; memcpy(&length, d+pos, 4); pos += 4;

    uint8_t reverse_table[256];
    for (int i = 0; i < 256; i++) reverse_table[swap_table[i]] = (uint8_t)i;

    // pass 1: i from pos+length down to pos+2 inclusive
    for (size_t i = pos + length; i >= pos + 2; i--) {
        uint8_t sub = (uint8_t)(d[i-2] + (uint8_t)((i - (pos+1)) & 0xFF));
        d[i-1] = (uint8_t)(reverse_table[d[i-1]] - sub);
    }
    // pass 2: i from pos+length-1 down to pos inclusive
    for (size_t i = pos + length; i-- > pos; ) {
        size_t off = i - pos;
        size_t sub = swap_table[off & 0xFF];
        size_t b = (i > sub + pos) ? (i - sub) : pos;
        if (b < pos) b = pos;
        uint8_t tmp = d[i]; d[i] = d[b]; d[b] = tmp;
    }
    return pos; // cursor unchanged, matches Rust
}

// ---- extension skip (we don't need extension contents, just the cursor) -
#define ARG_MAX 17
static void skip_extension(Cur* c)
{
    skip(c, 4); // version
    free(pas_string(c)); // name
    free(pas_string(c)); // folder_name
    uint32_t file_count = ru32(c);
    for (uint32_t i = 0; i < file_count; i++) {
        skip(c, 4); // version
        free(pas_string(c)); // name
        skip(c, 4); // kind
        free(pas_string(c)); // initializer
        free(pas_string(c)); // finalizer
        uint32_t fn_count = ru32(c);
        for (uint32_t j = 0; j < fn_count; j++) {
            skip(c, 4); // version
            free(pas_string(c)); free(pas_string(c)); // name, external_name
            skip(c, 4 + 4 + 4 + 4*ARG_MAX + 4); // convention,id,arg_count,arg_types,return_type
        }
        uint32_t const_count = ru32(c);
        for (uint32_t j = 0; j < const_count; j++) {
            skip(c, 4);
            free(pas_string(c)); free(pas_string(c));
        }
    }
    uint32_t contents_len = ru32(c) - 4;
    ru32(c); // seed1_raw
    skip(c, contents_len);
}

// ---- zlib inflate helper (unknown output size -> growable buffer) -----
static uint8_t* zinflate_all(const uint8_t* src, size_t srclen, size_t* outlen)
{
    size_t cap = srclen * 4 + 256;
    uint8_t* out = (uint8_t*)malloc(cap);
    z_stream zs; memset(&zs, 0, sizeof zs);
    inflateInit(&zs);
    zs.next_in = (Bytef*)src; zs.avail_in = (uInt)srclen;
    zs.next_out = out; zs.avail_out = (uInt)cap;
    int ret;
    do {
        ret = inflate(&zs, Z_NO_FLUSH);
        if (ret == Z_STREAM_END) break;
        if (zs.avail_out == 0) {
            size_t used = cap;
            cap *= 2;
            out = (uint8_t*)realloc(out, cap);
            zs.next_out = out + used;
            zs.avail_out = (uInt)(cap - used);
        } else if (ret != Z_OK) {
            free(out); inflateEnd(&zs); *outlen = 0; return NULL;
        }
    } while (ret != Z_STREAM_END);
    *outlen = zs.total_out;
    inflateEnd(&zs);
    return out;
}

// Reads a length-prefixed list of zlib blobs; inflates each; returns
// arrays of (inflated ptr, len). Entries that are the well-known "empty
// asset" placeholder or deflate to a leading zero u32 are NULL (deleted
// slot in the editor).
typedef struct { uint8_t* data; size_t len; } Blob;
static Blob* read_asset_list(Cur* c, uint32_t* out_count)
{
    uint32_t count = ru32(c);
    Blob* list = (Blob*)calloc(count, sizeof(Blob));
    static const uint8_t EMPTY[12] = {0x78,0x9C,0x63,0x60,0x60,0x60,0x00,0x00,0x00,0x04,0x00,0x01};
    for (uint32_t i = 0; i < count; i++) {
        uint32_t len = ru32(c);
        uint8_t* chunk = take(c, len);
        if (len == 12 && memcmp(chunk, EMPTY, 12) == 0) continue;
        size_t outlen;
        uint8_t* inflated = zinflate_all(chunk, len, &outlen);
        if (!inflated || outlen < 4) { free(inflated); continue; }
        uint32_t marker; memcpy(&marker, inflated, 4);
        if (marker == 0) { free(inflated); continue; }
        list[i].data = inflated;
        list[i].len = outlen;
    }
    *out_count = count;
    return list;
}

// ---- public asset index -------------------------------------------------
#define MAX_SPRITES 64
#define MAX_SOUNDS  64
static Gm81Sprite g_sprites[MAX_SPRITES]; static int g_sprite_count;
static Gm81Sound  g_sounds[MAX_SOUNDS];   static int g_sound_count;
static uint8_t*   g_filebuf; // kept alive for the lifetime of the process; sprite/sound blobs point into inflated copies, not this buffer, so it could technically be freed after parsing -- kept simple.

static void parse_sprite_blob(const uint8_t* data, size_t len)
{
    Cur c = { (uint8_t*)data, len, 4 }; // skip marker u32 already checked != 0
    char* name = pas_string(&c);
    ru32(&c); // version
    int32_t ox = ri32(&c), oy = ri32(&c);
    uint32_t frame_count = ru32(&c);
    if (frame_count == 0 || g_sprite_count >= MAX_SPRITES) { free(name); return; }

    Gm81Sprite* s = &g_sprites[g_sprite_count];
    s->name = name;
    s->origin_x = ox; s->origin_y = oy;
    s->frame_count = frame_count;
    s->frame_w = s->frame_h = 0;
    s->rgba = NULL;

    // Read each frame's raw RGBA and concatenate horizontally into one
    // strip, matching the layout gmgl_*/gmsprite_uv() already expect.
    uint32_t fw = 0, fh = 0;
    uint8_t** frames = (uint8_t**)malloc(sizeof(uint8_t*) * frame_count);
    for (uint32_t i = 0; i < frame_count; i++) {
        ru32(&c); // frame version
        uint32_t w = ru32(&c), h = ru32(&c);
        uint32_t flen = ru32(&c);
        uint8_t* fdata = take(&c, flen);
        if (i == 0) { fw = w; fh = h; }
        frames[i] = fdata; // points into `data`, still valid (caller keeps blob alive)
    }
    s->frame_w = fw; s->frame_h = fh;
    s->rgba = (uint8_t*)malloc((size_t)fw * frame_count * fh * 4);
    for (uint32_t i = 0; i < frame_count; i++) {
        for (uint32_t y = 0; y < fh; y++) {
            memcpy(s->rgba + ((size_t)y * fw * frame_count + (size_t)i * fw) * 4,
                   frames[i] + (size_t)y * fw * 4, (size_t)fw * 4);
        }
    }
    free(frames);
    g_sprite_count++;
}

static void parse_sound_blob(const uint8_t* data, size_t len)
{
    Cur c = { (uint8_t*)data, len, 4 };
    char* name = pas_string(&c);
    ru32(&c); // version
    ru32(&c); // kind
    char* extension = pas_string(&c);
    char* source = pas_string(&c);
    uint32_t has_data = ru32(&c);
    uint8_t* blob = NULL; uint32_t blen = 0;
    if (has_data) { blen = ru32(&c); blob = take(&c, blen); }
    free(extension); free(source);

    if (!blob || g_sound_count >= MAX_SOUNDS) { free(name); return; }
    Gm81Sound* snd = &g_sounds[g_sound_count];
    snd->name = name;
    snd->wav_data = blob; // points into `data`
    snd->wav_len = blen;
    g_sound_count++;
}

int gm81_open(const char* exe_path)
{
    FILE* f = fopen(exe_path, "rb");
    if (!f) { printf("[gm81] could not open %s\n", exe_path); return 0; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t* d = (uint8_t*)malloc(sz);
    if (fread(d, 1, sz, f) != (size_t)sz) { printf("[gm81] short read\n"); fclose(f); free(d); return 0; }
    fclose(f);
    g_filebuf = d;

    build_crc_table();
    size_t pos = gm81_check(d, sz);
    if (pos == (size_t)-1) { printf("[gm81] not a recognized GM8.1 exe (or protection patched)\n"); return 0; }

    Cur c = { d, (size_t)sz, pos };
    uint32_t settings_len = ru32(&c); skip(&c, settings_len);
    uint32_t dllname_len = ru32(&c); skip(&c, dllname_len);
    uint32_t dll_len = ru32(&c); skip(&c, dll_len);

    c.pos = gm80_decrypt(d, c.pos);

    uint32_t garbage_dwords = ru32(&c); skip(&c, garbage_dwords * 4);
    ru32(&c); // pro_flag
    ru32(&c); // game_id
    skip(&c, 16); // guid

    ru32(&c); // extensions header (700)
    uint32_t ext_count = ru32(&c);
    for (uint32_t i = 0; i < ext_count; i++) skip_extension(&c);

    ru32(&c); // triggers header (800)
    { uint32_t n; Blob* l = read_asset_list(&c, &n); for (uint32_t i=0;i<n;i++) free(l[i].data); free(l); }

    ru32(&c); // constants header (800)
    uint32_t const_count = ru32(&c);
    for (uint32_t i = 0; i < const_count; i++) { free(pas_string(&c)); free(pas_string(&c)); }

    ru32(&c); // sounds header (800)
    { uint32_t n; Blob* l = read_asset_list(&c, &n);
      for (uint32_t i=0;i<n;i++) if (l[i].data) parse_sound_blob(l[i].data, l[i].len);
      free(l); }

    ru32(&c); // sprites header (800)
    { uint32_t n; Blob* l = read_asset_list(&c, &n);
      for (uint32_t i=0;i<n;i++) if (l[i].data) { parse_sprite_blob(l[i].data, l[i].len); free(l[i].data); }
      free(l); }

    printf("[gm81] parsed %d sprites, %d sounds from %s\n", g_sprite_count, g_sound_count, exe_path);
    return 1;
}

const Gm81Sprite* gm81_find_sprite(const char* name)
{
    for (int i = 0; i < g_sprite_count; i++)
        if (strcmp(g_sprites[i].name, name) == 0) return &g_sprites[i];
    return NULL;
}
const Gm81Sound* gm81_find_sound(const char* name)
{
    for (int i = 0; i < g_sound_count; i++)
        if (strcmp(g_sounds[i].name, name) == 0) return &g_sounds[i];
    return NULL;
}

// ---- WAV decode + resample to 48kHz stereo s16 --------------------------
// GM8.1's embedded WAVs in this game are 44.1kHz; the existing mixer
// assumes its PCM sources are already at the output rate (48kHz, no
// runtime resampling), so we convert once here at load time.
int16_t* gm81_sound_to_pcm48(const Gm81Sound* snd, int* out_frames)
{
    if (!snd || snd->wav_len < 44) { *out_frames = 0; return NULL; }
    const uint8_t* d = snd->wav_data;
    if (memcmp(d, "RIFF", 4) != 0 || memcmp(d+8, "WAVE", 4) != 0) { *out_frames = 0; return NULL; }

    uint16_t channels = 0, bits = 0; uint32_t rate = 0;
    const uint8_t* data_ptr = NULL; uint32_t data_len = 0;

    size_t pos = 12;
    while (pos + 8 <= snd->wav_len) {
        char id[5] = {0}; memcpy(id, d+pos, 4);
        uint32_t clen; memcpy(&clen, d+pos+4, 4);
        const uint8_t* body = d + pos + 8;
        if (memcmp(id, "fmt ", 4) == 0 && pos + 8 + 16 <= snd->wav_len) {
            memcpy(&channels, body+2, 2);
            memcpy(&rate,     body+4, 4);
            memcpy(&bits,     body+14, 2);
        } else if (memcmp(id, "data", 4) == 0) {
            data_ptr = body; data_len = clen;
        }
        pos += 8 + clen + (clen & 1); // chunks are word-aligned
    }
    if (!data_ptr || channels == 0 || bits != 16 || rate == 0) { *out_frames = 0; return NULL; }

    uint32_t src_frames = data_len / (channels * 2);
    const int16_t* src = (const int16_t*)data_ptr;

    const uint32_t DST_RATE = 48000;
    uint32_t dst_frames = (uint32_t)((uint64_t)src_frames * DST_RATE / rate);
    int16_t* out = (int16_t*)malloc((size_t)dst_frames * 2 * sizeof(int16_t));

    for (uint32_t i = 0; i < dst_frames; i++) {
        double srcpos = (double)i * rate / DST_RATE;
        uint32_t i0 = (uint32_t)srcpos;
        uint32_t i1 = (i0 + 1 < src_frames) ? i0 + 1 : i0;
        double frac = srcpos - i0;
        for (int ch = 0; ch < 2; ch++) {
            int srcch = (channels == 1) ? 0 : ch;
            int16_t s0 = src[(size_t)i0 * channels + srcch];
            int16_t s1 = src[(size_t)i1 * channels + srcch];
            out[i*2+ch] = (int16_t)(s0 + (s1 - s0) * frac);
        }
    }
    *out_frames = (int)dst_frames;
    return out;
}
