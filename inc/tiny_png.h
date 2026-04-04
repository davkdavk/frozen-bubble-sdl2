#ifndef TINY_PNG_H
#define TINY_PNG_H

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct { const uint8_t *p; size_t pos, len; uint64_t bits; int nbits; } bs;

static int bs_pull(bs *s) {
    while (s->nbits < 56 && s->pos < s->len) {
        s->bits |= (uint64_t)s->p[s->pos++] << s->nbits;
        s->nbits += 8;
    }
    return s->nbits;
}

static uint32_t bs_bits(bs *s, int n) {
    bs_pull(s);
    uint32_t v = s->bits & ((1ULL << n) - 1);
    s->bits >>= n; s->nbits -= n;
    return v;
}

static void bs_align(bs *s) {
    while (s->nbits & 7) { s->bits >>= 1; s->nbits--; }
}

#define MAX_CODE_LEN 15
#define MAX_TABLE_BITS 9
#define MAX_SYMS 320

typedef struct { uint16_t table[1 << MAX_TABLE_BITS]; uint16_t tree[512]; int used; } huff;

static void huff_init(huff *h) {
    memset(h->table, 0, sizeof(h->table));
    memset(h->tree, 0, sizeof(h->tree));
    h->used = 0;
}

static int huff_build(huff *h, const uint8_t *lens, int nsyms, int tbits) {
    huff_init(h);
    int count[MAX_CODE_LEN + 1];
    int next[MAX_CODE_LEN + 1];
    memset(count, 0, sizeof(count));
    memset(next, 0, sizeof(next));
    if (nsyms > MAX_SYMS) return -1;
    for (int i = 0; i < nsyms; i++) if (lens[i] && lens[i] <= MAX_CODE_LEN) count[lens[i]]++;
    int code = 0; count[0] = 0;
    for (int i = 1; i <= MAX_CODE_LEN; i++) { code = (code + count[i-1]) << 1; next[i] = code; }
    for (int i = 0; i < nsyms; i++) {
        if (!lens[i] || lens[i] > MAX_CODE_LEN) continue;
        int len = lens[i];
        uint16_t rev = 0, tmp = (uint16_t)next[i];
        for (int j = 0; j < len; j++) { rev = (uint16_t)((rev << 1) | (tmp & 1)); tmp >>= 1; }
        next[i]++;
        if (len <= tbits) {
            int entries = 1 << (tbits - len);
            int base = rev;
            for (int j = 0; j < entries; j++) {
                int idx = base | (j << len);
                if (idx >= (1 << MAX_TABLE_BITS)) return -1;
                h->table[idx] = (uint16_t)(i | (len << 9));
            }
        } else {
            int idx = rev >> (len - tbits);
            if (idx >= (1 << tbits)) return -1;
            if (!(h->table[idx] >> 9)) {
                if (h->used >= 250) return -1;
                int node = h->used++;
                h->table[idx] = (uint16_t)((node + 1) | (1 << 15));
            }
            int node = (h->table[idx] & 0x1FF) - 1;
            if (node < 0 || node >= 250) return -1;
            for (int j = tbits; j < len; j++) {
                int bit = (rev >> (len - 1 - j)) & 1;
                if (j == len - 1) {
                    int nxt = node * 2 + bit;
                    if (nxt < 0 || nxt >= 512) return -1;
                    h->tree[nxt] = (uint16_t)(i | (1 << 14));
                } else {
                    int nxt = node * 2 + bit;
                    if (nxt < 0 || nxt >= 512) return -1;
                    if (!(h->tree[nxt] >> 9)) {
                        if (h->used >= 250) return -1;
                        int node2 = h->used++;
                        h->tree[nxt] = (uint16_t)((node2 + 1) | (1 << 15));
                    }
                    node = (h->tree[nxt] & 0x1FF) - 1;
                    if (node < 0 || node >= 250) return -1;
                }
            }
        }
    }
    return 0;
}

static int huff_decode(bs *s, huff *h, int tbits) {
    bs_pull(s);
    if (s->nbits < tbits) return -1;
    int idx = (int)(s->bits & ((1U << tbits) - 1));
    int entry = h->table[idx];
    int len = entry >> 9;
    if (len && len <= tbits) { s->bits >>= len; s->nbits -= len; return entry & 0x1FF; }
    if (!len) return -1;
    int node = (entry & 0x1FF) - 1;
    s->bits >>= tbits; s->nbits -= tbits;
    for (int i = tbits; i < MAX_CODE_LEN && s->nbits > 0; i++) {
        int bit = s->bits & 1; s->bits >>= 1; s->nbits--;
        int nxt = node * 2 + bit;
        if (nxt < 0 || nxt >= 512) return -1;
        entry = h->tree[nxt];
        if (entry & (1 << 14)) return entry & 0x1FF;
        node = (entry & 0x1FF) - 1;
        if (node < 0 || node >= 250) return -1;
    }
    return -1;
}

static int inflate(const uint8_t *src, size_t slen, uint8_t *dst, size_t dlen) {
    bs s; memset(&s, 0, sizeof(s)); s.p = src; s.len = slen;
    size_t out = 0;
    static const int lbase[29] = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
    static const int lext[29] = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
    static const int dbase[32] = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577,32769,49153};
    static const int dext[32] = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};
    static const uint8_t clord[19] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
    static uint8_t lens[MAX_SYMS];
    static uint8_t cls_arr[19];
    static huff lh, dh, clh;

    int last = 0;
    while (!last) {
        if (s.nbits < 3) bs_pull(&s);
        if (s.nbits < 3) return -1;
        last = s.bits & 1;
        int btype = (s.bits >> 1) & 3;
        s.bits >>= 3; s.nbits -= 3;

        if (btype == 0) {
            bs_align(&s);
            if (s.pos + 4 > slen) return -1;
            int len = s.p[s.pos] | (s.p[s.pos+1] << 8);
            int nlen = s.p[s.pos+2] | (s.p[s.pos+3] << 8);
            s.pos += 4;
            if (len != (~nlen & 0xFFFF)) return -1;
            if (out + len > dlen || s.pos + len > slen) return -1;
            memcpy(dst + out, s.p + s.pos, len);
            s.pos += len; out += len;
        } else if (btype == 1 || btype == 2) {
            int nlit, ndist;
            if (btype == 1) {
                nlit = 288; ndist = 32;
                memset(lens, 0, sizeof(uint8_t) * MAX_SYMS);
                for (int i = 0; i < 144; i++) lens[i] = 8;
                for (int i = 144; i < 256; i++) lens[i] = 9;
                for (int i = 256; i < 280; i++) lens[i] = 7;
                for (int i = 280; i < 288; i++) lens[i] = 8;
                for (int i = 0; i < 32; i++) lens[288+i] = 5;
            } else {
                if (s.nbits < 14) bs_pull(&s);
                if (s.nbits < 14) return -1;
                nlit = 257 + (s.bits & 0x1F); s.bits >>= 5; s.nbits -= 5;
                ndist = 1 + (s.bits & 0x1F); s.bits >>= 5; s.nbits -= 5;
                int ncl = 4 + (s.bits & 0xF); s.bits >>= 4; s.nbits -= 4;
                if (nlit > 286 || ndist > 30 || ncl > 19) return -1;
                memset(cls_arr, 0, sizeof(cls_arr));
                for (int i = 0; i < ncl; i++) {
                    if (s.nbits < 3) bs_pull(&s);
                    if (s.nbits < 3) return -1;
                    cls_arr[clord[i]] = s.bits & 7; s.bits >>= 3; s.nbits -= 3;
                }
                if (huff_build(&clh, cls_arr, 19, 7) < 0) return -1;
                int total = 0;
                while (total < nlit + ndist) {
                    int sym = huff_decode(&s, &clh, 7);
                    if (sym < 0) return -1;
                    if (sym < 16) { lens[total++] = sym; }
                    else {
                        int rep = 0, val = 0;
                        if (sym == 16) { if (s.nbits < 2) bs_pull(&s); if (s.nbits < 2) return -1; rep = 3 + (s.bits & 3); s.bits >>= 2; s.nbits -= 2; val = total > 0 ? lens[total-1] : 0; }
                        else if (sym == 17) { if (s.nbits < 3) bs_pull(&s); if (s.nbits < 3) return -1; rep = 3 + (s.bits & 7); s.bits >>= 3; s.nbits -= 3; }
                        else { if (s.nbits < 7) bs_pull(&s); if (s.nbits < 7) return -1; rep = 11 + (s.bits & 0x7F); s.bits >>= 7; s.nbits -= 7; }
                        for (int r = 0; r < rep && total < nlit + ndist; r++) lens[total++] = val;
                    }
                }
            }
            if (huff_build(&lh, lens, nlit, MAX_TABLE_BITS) < 0) return -1;
            if (huff_build(&dh, lens + nlit, ndist, 5) < 0) return -1;
            while (1) {
                int sym = huff_decode(&s, &lh, MAX_TABLE_BITS);
                if (sym < 0) return -1;
                if (sym < 256) { if (out >= dlen) return -1; dst[out++] = (uint8_t)sym; }
                else if (sym == 256) break;
                else {
                    int li = sym - 257;
                    if (li >= 29) return -1;
                    int mlen = lbase[li];
                    int extra = lext[li];
                    if (extra) { if (s.nbits < extra) bs_pull(&s); if (s.nbits < extra) return -1; mlen += s.bits & ((1 << extra) - 1); s.bits >>= extra; s.nbits -= extra; }
                    int dc = huff_decode(&s, &dh, 5);
                    if (dc < 0 || dc >= 32) return -1;
                    int dist = dbase[dc];
                    extra = dext[dc];
                    if (extra) { if (s.nbits < extra) bs_pull(&s); if (s.nbits < extra) return -1; dist += s.bits & ((1 << extra) - 1); s.bits >>= extra; s.nbits -= extra; }
                    if (dist <= 0 || dist > (int)out || out + mlen > dlen) return -1;
                    for (int i = 0; i < mlen; i++) dst[out+i] = dst[out-dist+i];
                    out += mlen;
                }
            }
        } else return -1;
    }
    return (int)out;
}

static uint8_t *tp_load(const uint8_t *data, size_t size, int *out_w, int *out_h) {
    if (size < 8 || data[0] != 0x89 || data[1] != 0x50 || data[2] != 0x4E || data[3] != 0x47) return NULL;
    if (data[4] != 0x0D || data[5] != 0x0A || data[6] != 0x1A || data[7] != 0x0A) return NULL;

    int w = 0, h = 0, depth = 0, ctype = 0;
    uint8_t *idata = NULL; size_t isize = 0, icap = 0;
    uint8_t *palette = NULL; int pal_size = 0;
    size_t pos = 8;

    while (pos + 8 <= size) {
        uint32_t clen = (data[pos] << 24) | (data[pos+1] << 16) | (data[pos+2] << 8) | data[pos+3];
        uint32_t ctype2 = (data[pos+4] << 24) | (data[pos+5] << 16) | (data[pos+6] << 8) | data[pos+7];
        pos += 8;
        if (pos + clen > size) break;
        if (ctype2 == 0x49484452) {
            w = (data[pos] << 24) | (data[pos+1] << 16) | (data[pos+2] << 8) | data[pos+3];
            h = (data[pos+4] << 24) | (data[pos+5] << 16) | (data[pos+6] << 8) | data[pos+7];
            depth = data[pos+8]; ctype = data[pos+9];
        } else if (ctype2 == 0x49444154) {
            if (isize + clen > icap) { icap = (isize + clen) * 2 + 4096; idata = (uint8_t *)realloc(idata, icap); }
            memcpy(idata + isize, data + pos, clen); isize += clen;
        } else if (ctype2 == 0x504C5445) {
            palette = (uint8_t *)data + pos; pal_size = clen;
        } else if (ctype2 == 0x49454E44) { break; }
        pos += clen + 4;
    }

    int bpp = 0;
    if (ctype == 0) bpp = 1; else if (ctype == 2) bpp = 3; else if (ctype == 3) bpp = 1;
    else if (ctype == 4) bpp = 2; else if (ctype == 6) bpp = 4; else { free(idata); return NULL; }
    if (w <= 0 || h <= 0 || bpp == 0) { free(idata); return NULL; }

    int row_bytes = w * bpp;
    size_t raw_size = (size_t)(row_bytes + 1) * h;
    uint8_t *raw = (uint8_t *)malloc(raw_size);
    if (!raw) { free(idata); return NULL; }
    int dlen = inflate(idata, isize, raw, raw_size);
    free(idata);
    if (dlen < 0) { free(raw); return NULL; }

    uint8_t *result = (uint8_t *)malloc(w * h * 4);
    if (!result) { free(raw); return NULL; }

    uint8_t *prev = NULL;
    for (int y = 0; y < h; y++) {
        uint8_t *row = raw + y * (row_bytes + 1);
        int filter = row[0];
        uint8_t *pix = row + 1;
        if (filter == 1) { for (int x = bpp; x < row_bytes; x++) pix[x] += pix[x - bpp]; }
        else if (filter == 2 && prev) { for (int x = 0; x < row_bytes; x++) pix[x] += prev[x]; }
        else if (filter == 3 && prev) { for (int x = 0; x < bpp; x++) pix[x] += prev[x] >> 1; for (int x = bpp; x < row_bytes; x++) pix[x] += (pix[x-bpp] + prev[x]) >> 1; }
        else if (filter == 4 && prev) {
            for (int x = 0; x < row_bytes; x++) {
                int a = x >= bpp ? pix[x-bpp] : 0, b = prev[x], c = x >= bpp ? prev[x-bpp] : 0;
                int p = a + b - c;
                int pa = p - a < 0 ? a - p : p - a, pb = p - b < 0 ? b - p : p - b, pc = p - c < 0 ? c - p : p - c;
                pix[x] += (pa <= pb && pa <= pc) ? a : (pb <= pc) ? b : c;
            }
        }
        uint32_t *out = (uint32_t *)result + y * w;
        if (ctype == 0) { for (int x = 0; x < w; x++) { uint8_t v = pix[x]; out[x] = 0xFF000000 | (v << 16) | (v << 8) | v; } }
        else if (ctype == 2) { for (int x = 0; x < w; x++) out[x] = 0xFF000000 | (pix[x*3] << 16) | (pix[x*3+1] << 8) | pix[x*3+2]; }
        else if (ctype == 3 && palette) { for (int x = 0; x < w; x++) { int idx = pix[x] * 3; if (idx + 2 < pal_size) out[x] = 0xFF000000 | (palette[idx] << 16) | (palette[idx+1] << 8) | palette[idx+2]; } }
        else if (ctype == 4) { for (int x = 0; x < w; x++) { uint8_t v = pix[x*2], a = pix[x*2+1]; out[x] = (a << 24) | (v << 16) | (v << 8) | v; } }
        else if (ctype == 6) { for (int x = 0; x < w; x++) out[x] = (pix[x*4+3] << 24) | (pix[x*4] << 16) | (pix[x*4+1] << 8) | pix[x*4+2]; }
        prev = pix;
    }
    free(raw);
    *out_w = w; *out_h = h;
    return result;
}

#endif
