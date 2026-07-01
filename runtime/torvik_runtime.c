/*
 * torvik_runtime.c — Torvik Standard Library Runtime
 *
 * This file implements every built-in function available in Torvik programs.
 * It is compiled once and linked into every Torvik binary by torvc.
 *
 * Compile with:
 *   clang -c torvik_runtime.c -o torvik_runtime.o -O2
 *
 * Sections:
 *   1. Terminal I/O       (out, outln, read, readint, readfloat, readbool, readkey)
 *   2. String operations  (trim, triml, trimr, upper, lower, replace, contains,
 *                          starts, ends, split, len, fmt, tostr, toint, tofloat)
 *   3. File I/O           (readfile, writefile)
 *   4. Environment        (readenv, args)
 *   5. Math               (min, max, abs, rand, randint)
 *   6. Time & Terminal    (sleep, wait, clear_screen, time_now, time_ms,
 *                          date_str, time_str)
 *   7. Program control    (torvik_panic, assert)
 *   8. Collections        (list, table, bag)
 *   9. System (sys)       (os_name, cpu_count, mem_total, etc.)
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <termios.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <sys/wait.h>
#include <errno.h>

/* ── String refcount header (memory Stage B1) ─────────────────────────────────
   EVERY string a Torvik program can hold is allocated with a 16-byte header:
       [i64 magic][i64 refcount]["bytes...\0"]
                                 ^ all consumers receive THIS pointer
   C consumers (strcmp/strlen/puts/...) are unaffected — they see a plain char*.
   torvik_len's list-vs-string dispatch is unaffected — the bytes pointer still
   points at character data, not at this header.
   Interned @.s globals get the same header embedded in their constant bytes
   (Stage B2) with refcount == TORVIK_RC_IMMORTAL: retain/release no-op on them,
   they live in rodata and are never written or freed.
   refcount starts at 0 (slot-counting, same convention as lists): the store
   that places a string into a slot performs the first retain.
   INTERNAL copies that Torvik never sees (table keys, bag items) stay plain
   strdup/free on purpose. */
#define TORVIK_STR_MAGIC    0x544F525653545248LL  /* "TORVSTRH" */
#define TORVIK_RC_IMMORTAL  0x494D4D4F5254414CLL  /* "IMMORTAL" */
#define TORVIK_STR_HDR      16

typedef struct { int64_t magic; int64_t refcount; } TorvikStrHdr;

/* Memory hardening (v1.0): every heap allocation in the runtime routes through
   these. On exhaustion malloc/realloc/strdup return NULL; unchecked, the next
   dereference is a segfault — an opaque crash on exactly the "big workload" a
   user is most likely to push. These convert that into a clean Torvik panic
   with a message. torvik_panic is defined later in this file; forward-declare. */
void torvik_panic(const char *msg);

static void *tv_malloc(size_t n) {
    void *p = malloc(n);
    if (!p) torvik_panic("out of memory");
    return p;
}
static void *tv_realloc(void *p, size_t n) {
    void *q = realloc(p, n);
    if (!q) torvik_panic("out of memory (realloc)");
    return q;
}
static char *tv_strdup(const char *s) {
    char *p = strdup(s);
    if (!p) torvik_panic("out of memory (strdup)");
    return p;
}

static TorvikStrHdr *torvik_str_hdr(const char *bytes) {
    return (TorvikStrHdr *)((char *)bytes - TORVIK_STR_HDR);
}

/* Allocate a headered string with room for n bytes + NUL; returns the bytes
   pointer. The NUL at [n] is pre-set so fixed-size writers can't leave the
   buffer unterminated. Content area is padded to >= 8 bytes: torvik_len's
   list-vs-string dispatch reads 8 bytes at the bytes pointer, and BEFORE this
   padding that read went out of bounds on every heap string shorter than 8
   chars — silently absorbed by glibc's minimum chunk size until ASAN flagged
   it. The pad makes the dispatch read defined for every headered string. */
char *torvik_str_alloc(size_t n) {
    size_t content = n + 1;
    if (content < 8) content = 8;
    char *base = tv_malloc(TORVIK_STR_HDR + content);
    TorvikStrHdr *h = (TorvikStrHdr *)base;
    h->magic = TORVIK_STR_MAGIC;
    h->refcount = 0;
    char *bytes = base + TORVIK_STR_HDR;
    memset(bytes + n, 0, content - n);   /* NUL at [n] plus defined pad bytes */
    return bytes;
}

/* Headered strdup — the workhorse for converting the runtime's string fns. */
char *torvik_str_dup(const char *s) {
    size_t n = strlen(s);
    char *bytes = torvik_str_alloc(n);
    memcpy(bytes, s, n);
    return bytes;
}

void *torvik_str_retain(void *p) {
    if (!p) return p;
    TorvikStrHdr *h = torvik_str_hdr(p);
    if (h->magic != TORVIK_STR_MAGIC) return p;
    if (h->refcount == TORVIK_RC_IMMORTAL) return p;
    h->refcount++;
    return p;
}

void torvik_str_release(void *p) {
    if (!p) return;
    TorvikStrHdr *h = torvik_str_hdr(p);
    if (h->magic != TORVIK_STR_MAGIC) return;
    if (h->refcount == TORVIK_RC_IMMORTAL) return;
    h->refcount--;
    if (h->refcount <= 0) {
        h->magic = 0;   /* poison: a stale double-release fails the guard */
        free(h);
    }
}

/* ── 128-bit integer box (Tier 3) ─────────────────────────────────────────────
   i128/u128 values are 128 bits and cannot ride inside the 64-bit uniform-ptr
   slot the way i8..u64 / f64 do. They are heap-boxed: the slot holds a real
   pointer to [i64 magic][i64 refcount][i128 value], and the same slot-counting
   ARC that frees strings/lists frees these (magic-guarded, poison-on-free).
   The returned pointer points at the value field (16-byte aligned for load i128).*/
#define TORVIK_I128_MAGIC 0x544F565649313238LL  /* "TORVI128" */
typedef struct { int64_t magic; int64_t refcount; __int128 value; } TorvikI128Box;

void *torvik_i128_box(__int128 v) {
    TorvikI128Box *b = (TorvikI128Box *)tv_malloc(sizeof(TorvikI128Box));
    b->magic = TORVIK_I128_MAGIC;
    b->refcount = 0;
    b->value = v;
    return (char *)b + 16;   /* -> value field */
}
__int128 torvik_i128_load(void *p) {
    return *(__int128 *)p;
}
void *torvik_i128_retain(void *p) {
    if (!p) return p;
    TorvikI128Box *b = (TorvikI128Box *)((char *)p - 16);
    if (b->magic != TORVIK_I128_MAGIC) return p;
    if (b->refcount == TORVIK_RC_IMMORTAL) return p;
    b->refcount++;
    return p;
}
void torvik_i128_release(void *p) {
    if (!p) return;
    TorvikI128Box *b = (TorvikI128Box *)((char *)p - 16);
    if (b->magic != TORVIK_I128_MAGIC) return;
    if (b->refcount == TORVIK_RC_IMMORTAL) return;
    b->refcount--;
    if (b->refcount <= 0) { b->magic = 0; free(b); }
}

/* 128-bit -> decimal string (no libc format exists for __int128). */
char *torvik_u128_to_str(unsigned __int128 v) {
    char tmp[44]; int i = 44;
    if (v == 0) tmp[--i] = '0';
    while (v > 0) { tmp[--i] = (char)('0' + (int)(v % 10)); v /= 10; }
    char *out = torvik_str_alloc((size_t)(44 - i));
    memcpy(out, tmp + i, (size_t)(44 - i));
    return out;
}
char *torvik_i128_to_str(__int128 v) {
    char tmp[44]; int i = 44; int neg = 0;
    unsigned __int128 u;
    if (v < 0) { neg = 1; u = (unsigned __int128)(-(v + 1)) + 1; } else { u = (unsigned __int128)v; }
    if (u == 0) tmp[--i] = '0';
    while (u > 0) { tmp[--i] = (char)('0' + (int)(u % 10)); u /= 10; }
    if (neg) tmp[--i] = '-';
    char *out = torvik_str_alloc((size_t)(44 - i));
    memcpy(out, tmp + i, (size_t)(44 - i));
    return out;
}

/* unsigned/signed 128-bit div + mod with zero-guard (mirrors torvik_div/mod). */
__int128 torvik_i128_div(__int128 a, __int128 b) {
    if (b == 0) torvik_panic("division by zero");
    return a / b;
}
__int128 torvik_i128_mod(__int128 a, __int128 b) {
    if (b == 0) torvik_panic("modulo by zero");
    return a % b;
}
__int128 torvik_u128_div(__int128 a, __int128 b) {
    if (b == 0) torvik_panic("division by zero");
    return (__int128)((unsigned __int128)a / (unsigned __int128)b);
}
__int128 torvik_u128_mod(__int128 a, __int128 b) {
    if (b == 0) torvik_panic("modulo by zero");
    return (__int128)((unsigned __int128)a % (unsigned __int128)b);
}

/* Boundary copy-in for OS argv (Stage B2 wires this into the main wrapper):
   returns a fresh array of HEADERED copies so the everything-headered contract
   holds for args_get() results too. */
char **torvik_args_copy(int64_t argc, char **argv) {
    char **out = tv_malloc((size_t)(argc > 0 ? argc : 1) * sizeof(char *));
    for (int64_t i = 0; i < argc; i++) out[i] = torvik_str_dup(argv[i]);
    return out;
}

/* ── 1. Terminal I/O ────────────────────────────────────────────────────────── */

/* read(prompt) → heap-allocated string, caller owns it */
char *torvik_read(const char *prompt) {
    if (prompt && *prompt) {
        fputs(prompt, stdout);
        fflush(stdout);
    }
    char *line = NULL;
    size_t cap  = 0;
    ssize_t len = getline(&line, &cap, stdin);
    if (len < 0) { free(line); return torvik_str_dup(""); }
    /* Strip trailing newline */
    if (len > 0 && line[len-1] == '\n') line[--len] = '\0';
    char *r = torvik_str_dup(line);   /* B1: copy into headered string */
    free(line);                        /* getline's raw buffer */
    return r;
}

/* readln() — promptless alias for torvik_read("") */
char *torvik_readln(void) {
    return torvik_read("");
}

/* readint(prompt) — retries on bad input */
int64_t torvik_readint(const char *prompt) {
    while (1) {
        char *s = torvik_read(prompt);
        char *end;
        errno = 0;
        long long v = strtoll(s, &end, 10);
        torvik_str_release(s);   /* B1: s is headered now */
        if (*end == '\0' && errno == 0) return (int64_t)v;
        fputs("Invalid integer, please try again: ", stdout);
        fflush(stdout);
    }
}

/* readfloat(prompt) — retries on bad input */
double torvik_readfloat(const char *prompt) {
    while (1) {
        char *s = torvik_read(prompt);
        char *end;
        errno = 0;
        double v = strtod(s, &end);
        torvik_str_release(s);   /* B1: s is headered now */
        if (*end == '\0' && errno == 0) return v;
        fputs("Invalid number, please try again: ", stdout);
        fflush(stdout);
    }
}

/* readbool(prompt) — accepts y/yes/1/true → 1, n/no/0/false → 0 */
int torvik_readbool(const char *prompt) {
    while (1) {
        char *s = torvik_read(prompt);
        /* Lowercase the input */
        for (char *p = s; *p; p++) *p = tolower((unsigned char)*p);
        int v = -1;
        if (!strcmp(s,"y")||!strcmp(s,"yes")||!strcmp(s,"1")||!strcmp(s,"true"))  v = 1;
        if (!strcmp(s,"n")||!strcmp(s,"no") ||!strcmp(s,"0")||!strcmp(s,"false")) v = 0;
        torvik_str_release(s);   /* B1: s is headered now */
        if (v >= 0) return v;
        fputs("Please enter y or n: ", stdout);
        fflush(stdout);
    }
}

/* readkey() — single keypress, no Enter required */
char torvik_readkey(void) {
    struct termios old, raw;
    tcgetattr(STDIN_FILENO, &old);
    raw = old;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN]  = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    char c = getchar();
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &old);
    return c;
}

/* ── 2. String Operations ────────────────────────────────────────────────────── */

/* trim — remove leading and trailing whitespace, returns new heap string */
char *torvik_trim(const char *s) {
    while (isspace((unsigned char)*s)) s++;
    if (*s == '\0') return torvik_str_dup("");
    const char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) end--;
    size_t len = end - s + 1;
    char *r = torvik_str_alloc(len);
    memcpy(r, s, len);
    return r;
}

/* triml — remove leading whitespace only */
char *torvik_triml(const char *s) {
    while (isspace((unsigned char)*s)) s++;
    return torvik_str_dup(s);
}

/* trimr — remove trailing whitespace only */
char *torvik_trimr(const char *s) {
    size_t len = strlen(s);
    if (len == 0) return torvik_str_dup("");
    char *r = torvik_str_dup(s);
    char *end = r + len - 1;
    while (end >= r && isspace((unsigned char)*end)) *end-- = '\0';
    return r;
}

/* upper — returns new heap string */
char *torvik_upper(const char *s) {
    char *r = torvik_str_dup(s);
    for (char *p = r; *p; p++) *p = toupper((unsigned char)*p);
    return r;
}

/* lower — returns new heap string */
char *torvik_lower(const char *s) {
    char *r = torvik_str_dup(s);
    for (char *p = r; *p; p++) *p = tolower((unsigned char)*p);
    return r;
}

/* replace — replace all occurrences of `from` with `to` in `s` */
char *torvik_replace(const char *s, const char *from, const char *to) {
    size_t from_len = strlen(from);
    size_t to_len   = strlen(to);
    if (from_len == 0) return torvik_str_dup(s);

    /* Count occurrences */
    size_t count = 0;
    const char *p = s;
    while ((p = strstr(p, from)) != NULL) { count++; p += from_len; }

    size_t new_len = strlen(s) + count * (to_len - from_len);
    char *result = torvik_str_alloc(new_len);
    char *dst = result;
    p = s;
    const char *found;
    while ((found = strstr(p, from)) != NULL) {
        size_t chunk = found - p;
        memcpy(dst, p, chunk);
        dst += chunk;
        memcpy(dst, to, to_len);
        dst += to_len;
        p = found + from_len;
    }
    strcpy(dst, p);
    return result;
}

/* contains — 1 if s contains sub, 0 otherwise */
int torvik_contains(const char *s, const char *sub) {
    return strstr(s, sub) != NULL;
}

/* starts — 1 if s starts with prefix */
int torvik_starts(const char *s, const char *prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

/* ends — 1 if s ends with suffix */
int torvik_ends(const char *s, const char *suffix) {
    size_t slen = strlen(s), suflen = strlen(suffix);
    if (suflen > slen) return 0;
    return strcmp(s + slen - suflen, suffix) == 0;
}

/* len — string length */
int64_t torvik_len_str(const char *s) {
    return (int64_t)strlen(s);
}

/* tostr — integer to decimal string */
char *torvik_tostr(int64_t n) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%lld", (long long)n);
    return torvik_str_dup(buf);
}

/* float_to_str — compact %g formatting (drops trailing zeros), but always reads
   back as a float: if %g produced a bare integer (no '.', exponent, or inf/nan),
   append ".0" so 7.0 prints "7.0" rather than "7". */
char *torvik_float_to_str(double d) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%g", d);
    int floaty = 0;
    for (char *p = buf; *p; p++) {
        char c = *p;
        if (c == '.' || c == 'e' || c == 'E' || c == 'n' || c == 'N' || c == 'i' || c == 'I') { floaty = 1; break; }
    }
    if (!floaty) {
        size_t n = strlen(buf);
        if (n + 2 < sizeof(buf)) { buf[n] = '.'; buf[n+1] = '0'; buf[n+2] = '\0'; }
    }
    return torvik_str_dup(buf);
}

/* to_float — parse string to double, 0.0 on failure (value-returning form of
 * torvik_tofloat, which reports success through an out-param) */
int torvik_tofloat(const char *s, double *out);
double torvik_to_float(const char *s) {
    double out = 0.0;
    torvik_tofloat(s, &out);
    return out;
}

/* toint — parse string to int64, returns 1 on success */
int64_t torvik_toint(const char *s) {
    char *end;
    long long v = strtoll(s, &end, 10);
    return (int64_t)v;
}

/* Guarded integer division / modulo: a zero divisor is a clean Torvik panic
   (exit 1) rather than C undefined behavior (a SIGFPE crash or a garbage
   value). Honors Torvik's no-silent-wrong-answers rule. */
int64_t torvik_div(int64_t a, int64_t b) {
    if (b == 0) torvik_panic("division by zero");
    if (a == INT64_MIN && b == -1) torvik_panic("division overflow (INT64_MIN / -1)");
    return a / b;
}
int64_t torvik_mod(int64_t a, int64_t b) {
    if (b == 0) torvik_panic("modulo by zero");
    if (a == INT64_MIN && b == -1) return 0;   /* mathematically 0; avoids the CPU trap */
    return a % b;
}
/* unsigned div/mod for u64: same 64 bits, divided as unsigned. No INT64_MIN/-1
   trap is possible in unsigned arithmetic, so only zero is guarded. */
int64_t torvik_udiv(int64_t a, int64_t b) {
    if (b == 0) torvik_panic("division by zero");
    return (int64_t)((uint64_t)a / (uint64_t)b);
}
int64_t torvik_umod(int64_t a, int64_t b) {
    if (b == 0) torvik_panic("modulo by zero");
    return (int64_t)((uint64_t)a % (uint64_t)b);
}

/* echo  -> print without a trailing newline (echo! uses puts, which adds one) */
void torvik_print(const char *s) {
    fputs(s, stdout);
}

/* tofloat — parse string to double, returns 1 on success */
int torvik_tofloat(const char *s, double *out) {
    char *end;
    errno = 0;
    double v = strtod(s, &end);
    if (*end != '\0' || errno != 0) return 0;
    *out = v;
    return 1;
}

/*
 * split — split s by delim, returns NULL-terminated array of heap strings.
 * Caller must free each element and the array itself.
 * *count is set to the number of elements.
 */
char **torvik_split(const char *s, const char *delim, int64_t *count) {
    size_t dlen  = strlen(delim);
    size_t cap   = 16;
    size_t n     = 0;
    char **parts = tv_malloc(cap * sizeof(char*));
    const char *p = s;
    const char *found;

    while ((found = dlen ? strstr(p, delim) : NULL) != NULL) {
        if (n >= cap) { cap *= 2; parts = tv_realloc(parts, cap * sizeof(char*)); }
        size_t chunk = found - p;
        char *part = torvik_str_alloc(chunk);
        memcpy(part, p, chunk);
        parts[n++] = part;
        p = found + dlen;
    }
    /* last segment — need room for parts[n] (the segment) AND parts[n+1] (the
     * NULL terminator), so ensure capacity is at least n+2. */
    if (n + 1 >= cap) { cap = n + 2; parts = tv_realloc(parts, cap * sizeof(char*)); }
    parts[n++] = torvik_str_dup(p);
    parts[n]   = NULL;
    *count     = (int64_t)n;
    return parts;
}


/*
 * fmt — named string interpolation: fmt("Hello {name}!")
 * Takes the template string and a NULL-terminated list of
 * name,value pairs: fmt_impl(template, "name", value, NULL)
 */
char *torvik_fmt_impl(const char *tmpl, ...) {
    /* For Phase 5, fmt() is implemented by substituting {varname} tokens.
     * The generated IR passes variable values as additional arguments. */
    /* Simple pass-through — full fmt() requires variadic name lookup.
     * Full implementation integrated in Phase 6 codegen. */
    return torvik_str_dup(tmpl);
}

/* fmt — positional interpolation. Each "{}" in the template is replaced, in
 * order, by the next string argument (codegen pre-stringifies non-string args).
 * Extra "{}" with no remaining argument are left literal; surplus arguments are
 * ignored. Returns a fresh headered (rc 0) Torvik string. */
char *torvik_fmt_apply(const char *tmpl, int64_t argc, ...) {
    va_list ap;
    va_start(ap, argc);
    size_t cap = 64, len = 0;
    char *buf = tv_malloc(cap);
    int64_t used = 0;
    const char *p = tmpl;
    while (*p) {
        if (p[0] == '{' && p[1] == '}' && used < argc) {
            const char *sub = va_arg(ap, const char *);
            used++;
            if (!sub) sub = "";
            size_t sl = strlen(sub);
            if (len + sl + 1 > cap) { while (len + sl + 1 > cap) cap *= 2; buf = tv_realloc(buf, cap); }
            memcpy(buf + len, sub, sl);
            len += sl;
            p += 2;
        } else {
            if (len + 2 > cap) { cap *= 2; buf = tv_realloc(buf, cap); }
            buf[len++] = *p++;
        }
    }
    buf[len] = '\0';
    va_end(ap);
    char *out = torvik_str_dup(buf);
    free(buf);
    return out;
}

/* ── 3. File I/O ─────────────────────────────────────────────────────────────── */

/* readfile — read entire file, returns heap string. panics if not found. */
char *torvik_readfile(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "[Torvik panic] readfile: cannot open '%s': %s\n",
                path, strerror(errno));
        exit(1);
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    /* Security: ftell returns -1 on error (e.g. a pipe or unseekable file).
       Without this check (size_t)(-1) would request a near-zero allocation and
       the following fread would write SIZE_MAX bytes — a massive heap overflow. */
    if (size < 0) { fclose(f); torvik_panic("readfile: cannot determine file size"); }
    rewind(f);
    char *buf = torvik_str_alloc((size_t)size);
    size_t got = fread(buf, 1, (size_t)size, f);
    buf[got] = '\0';   /* terminate at the actual byte count (handles short reads) */
    fclose(f);
    return buf;
}

/* writefile — write string to file, creating or overwriting */
void torvik_writefile(const char *path, const char *data) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "[Torvik panic] writefile: cannot open '%s': %s\n",
                path, strerror(errno));
        exit(1);
    }
    fputs(data, f);
    fclose(f);
}

/* ── 4. Environment ──────────────────────────────────────────────────────────── */

/* readenv — returns heap string or NULL if not set */
char *torvik_readenv(const char *name) {
    const char *v = getenv(name);
    return v ? torvik_str_dup(v) : NULL;
}

/* ── 5. Math ─────────────────────────────────────────────────────────────────── */

int64_t torvik_min(int64_t a, int64_t b) { return a < b ? a : b; }
int64_t torvik_max(int64_t a, int64_t b) { return a > b ? a : b; }
int64_t torvik_abs(int64_t x)            { return x < 0 ? -x : x; }

/* rand() → [0.0, 1.0) */
double torvik_rand(void) {
    return (double)rand() / ((double)RAND_MAX + 1.0);
}

/* randint(lo, hi) → [lo, hi] inclusive */
int64_t torvik_randint(int64_t lo, int64_t hi) {
    if (hi <= lo) return lo;
    return lo + (int64_t)(rand() % (hi - lo + 1));
}

/* Seed rand on first call */
__attribute__((constructor))
static void torvik_seed_rand(void) {
    srand((unsigned)time(NULL) ^ (unsigned)getpid());
}

/* ── 6. Time & Terminal ──────────────────────────────────────────────────────── */

/* sleep(ms) — sleep for N milliseconds */
void torvik_sleep(int64_t ms) {
    if (ms <= 0) return;
    struct timespec ts;
    ts.tv_sec  = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

/* wait(secs) — sleep for N seconds (fractional) */
void torvik_wait(double secs) {
    if (secs <= 0) return;
    int64_t ms = (int64_t)(secs * 1000.0);
    torvik_sleep(ms);
}

/* clear_screen() — ANSI clear, works on any ANSI terminal */
void torvik_clear_screen(void) {
    fputs("\033[2J\033[H", stdout);
    fflush(stdout);
}

/* time_now() — Unix timestamp in seconds */
int64_t torvik_time_now(void) {
    return (int64_t)time(NULL);
}

/* time_ms() — milliseconds since epoch */
int64_t torvik_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000LL + (int64_t)tv.tv_usec / 1000LL;
}

/* date_str() — "YYYY-MM-DD" */
char *torvik_date_str(void) {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char *buf = torvik_str_alloc(10);
    strftime(buf, 11, "%Y-%m-%d", tm);
    return buf;
}

/* time_str() — "HH:MM:SS" */
char *torvik_time_str(void) {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char *buf = torvik_str_alloc(8);
    strftime(buf, 9, "%H:%M:%S", tm);
    return buf;
}

/* datetime_str() — "YYYY-MM-DD HH:MM:SS" local time, one headered string. */
char *torvik_datetime_str(void) {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char *buf = torvik_str_alloc(19);
    strftime(buf, 20, "%Y-%m-%d %H:%M:%S", tm);
    return buf;
}

/* ── 7. Program Control ──────────────────────────────────────────────────────── */

void torvik_panic(const char *msg) {
    fprintf(stderr, "[Torvik panic] %s\n", msg);
    exit(1);
}

void torvik_assert(int cond, const char *msg) {
    if (!cond) torvik_panic(msg);
}

char *torvik_typeof_str(void)   { return torvik_str_dup("str"); }
char *torvik_typeof_i64(void)   { return torvik_str_dup("i64"); }
char *torvik_typeof_f64(void)   { return torvik_str_dup("f64"); }
char *torvik_typeof_bool(void)  { return torvik_str_dup("bool"); }

/* ── 8. Collections ──────────────────────────────────────────────────────────── */

/* --- list<T> --- */
#define TORVIK_LIST_MAGIC 0x544F5256494B4C53LL  /* "TORVIKLS" */
typedef struct {
    int64_t magic;
    void  **data;
    int64_t len;
    int64_t cap;
    int64_t refcount;   /* memory Stage A: slot-count refcount (see retain/release) */
    int64_t managed;    /* memory Stage B3: 1 = elements are refcounted strings.
                           Set ONCE at creation via torvik_list_mark_managed,
                           emitted by codegen when the declared annotation is
                           exactly list<str>. The flag travels with the OBJECT,
                           so lists passed to functions keep their management.
                           NEVER set on lists holding raw i64s: the element
                           retain/release below would read 16 bytes before a
                           small-int 'pointer' and crash. The type annotation
                           is the contract. */
} TorvikList;

TorvikList *torvik_list_new(void) {
    TorvikList *l = tv_malloc(sizeof(TorvikList));
    l->magic = TORVIK_LIST_MAGIC;
    l->cap  = 8;
    l->len  = 0;
    l->refcount = 0;   /* slot-counting: no slot owns it yet; first store retains */
    l->managed  = 0;   /* B3: raw elements until codegen marks list<str> lists */
    l->data = tv_malloc(l->cap * sizeof(void*));
    return l;
}

/* B3: flip a list to managed-element mode. Idempotent; magic-guarded so a
   stray call on a non-list is a no-op. Emitted right after the slot store of
   an annotated `list<str>` declaration — before any push can occur.
   managed kind: 0 = raw (i64/inttoptr elements, never refcounted), 1 = str
   (elements are torvik strings), 2 = i128 (elements are i128/u128 heap boxes).
   Both 1 and 2 are truthy, so existing `if (l->managed)` gating still holds;
   the retain/release call dispatches on the exact kind. */
void torvik_list_mark_managed(void *p) {
    if (!p) return;
    if (*(int64_t *)p != TORVIK_LIST_MAGIC) return;
    ((TorvikList *)p)->managed = 1;
}
void torvik_list_mark_managed_i128(void *p) {
    if (!p) return;
    if (*(int64_t *)p != TORVIK_LIST_MAGIC) return;
    ((TorvikList *)p)->managed = 2;
}

void torvik_push(TorvikList *l, void *item) {
    if (l->len >= l->cap) {
        l->cap *= 2;
        l->data = tv_realloc(l->data, l->cap * sizeof(void*));
    }
    if (l->managed == 1) torvik_str_retain(item);   /* B3: list cell counts as holder */
    else if (l->managed == 2) torvik_i128_retain(item);
    l->data[l->len++] = item;
}

/* B3 NOTE — pop TRANSFERS ownership on managed lists: the list's retain moves
   with the returned value instead of being released here. Releasing inside pop
   could free the string in the instruction gap before the receiving slot's
   retain runs (pop -> freed -> retain reads freed memory). The cost is a
   one-count leak on popped strings until Stage E temp management; the win is
   that use-after-free is structurally impossible. */
void *torvik_pop(TorvikList *l) {
    if (l->len == 0) torvik_panic("pop: list is empty");
    return l->data[--l->len];
}

void torvik_insert(TorvikList *l, int64_t idx, void *item) {
    if (idx < 0 || idx > l->len) torvik_panic("insert: index out of bounds");
    if (l->len >= l->cap) {
        l->cap *= 2;
        l->data = tv_realloc(l->data, l->cap * sizeof(void*));
    }
    memmove(&l->data[idx+1], &l->data[idx], (l->len - idx) * sizeof(void*));
    if (l->managed == 1) torvik_str_retain(item);   /* B3 */
    else if (l->managed == 2) torvik_i128_retain(item);
    l->data[idx] = item;
    l->len++;
}

void torvik_remove(TorvikList *l, int64_t idx) {
    if (idx < 0 || idx >= l->len) torvik_panic("remove: index out of bounds");
    if (l->managed == 1) torvik_str_release(l->data[idx]);   /* B3 */
    else if (l->managed == 2) torvik_i128_release(l->data[idx]);
    memmove(&l->data[idx], &l->data[idx+1], (l->len - idx - 1) * sizeof(void*));
    l->len--;
}

int64_t torvik_list_len(TorvikList *l) { return l->len; }

/* Membership for list<i64>: 1 if `value` equals any element (compared as i64),
   else 0. v1.0.0 scope: i64/identity comparison (ints, and ptr-identity on
   object lists). String-VALUE membership is a post-1.0 refinement.
   NOTE: distinct from torvik_contains (string substring) to avoid name clash. */
long torvik_list_contains(TorvikList *l, int64_t value) {
    if (!l) return 0;
    for (int64_t i = 0; i < l->len; i++) {
        if ((int64_t)l->data[i] == value) return 1;
    }
    return 0;
}

/* ── Reference counting (memory Stage A) ──────────────────────────────────────
   Convention: SLOT-COUNTING. refcount == number of live variable slots that
   currently reference this list. torvik_list_new returns refcount 0 (nothing
   owns it yet); the store that places a list into a slot does the first retain;
   storing over a slot releases the old contents; scope exit (Stage C) releases.

   Both fns are null-safe AND magic-guarded, so handing them a non-list pointer
   is a no-op. In practice codegen only ever calls them on list-typed slots
   (gated on the type map) that have been null-initialised, so the guard is
   belt-and-suspenders — its real value is poison-detecting a double release. */
void *torvik_list_retain(void *p) {
    if (!p) return p;
    if (*(int64_t *)p != TORVIK_LIST_MAGIC) return p;
    ((TorvikList *)p)->refcount++;
    return p;
}

void torvik_list_release(void *p) {
    if (!p) return;
    if (*(int64_t *)p != TORVIK_LIST_MAGIC) return;
    TorvikList *l = (TorvikList *)p;
    l->refcount--;
    if (l->refcount <= 0) {
        if (l->managed == 1) {                  /* B3: cells release their strings */
            for (int64_t i = 0; i < l->len; i++) torvik_str_release(l->data[i]);
        } else if (l->managed == 2) {            /* i128/u128 element boxes */
            for (int64_t i = 0; i < l->len; i++) torvik_i128_release(l->data[i]);
        }
        free(l->data);
        l->magic = 0;   /* poison so a stale double-release fails the guard */
        free(l);
    }
}

/* Unified length: detect list (by magic header) vs C string */
int64_t torvik_len(void *p) {
    if (p == 0) return 0;
    int64_t maybe_magic = *(int64_t *)p;
    if (maybe_magic == TORVIK_LIST_MAGIC) {
        return ((TorvikList *)p)->len;
    }
    return (int64_t)strlen((const char *)p);
}

void *torvik_list_get(TorvikList *l, int64_t idx) {
    if (idx < 0 || idx >= l->len) torvik_panic("list index out of bounds");
    return l->data[idx];
}

void torvik_list_set(TorvikList *l, int64_t idx, int64_t val) {
    if (idx < 0 || idx >= l->len) torvik_panic("list index out of bounds");
    if (l->managed == 1) {
        torvik_str_retain((void *)val);        /* B3: retain new BEFORE releasing */
        torvik_str_release(l->data[idx]);      /* old — self-set (xs[i]=xs[i]) safe */
    } else if (l->managed == 2) {
        torvik_i128_retain((void *)val);
        torvik_i128_release(l->data[idx]);
    }
    l->data[idx] = (void *)val;
}

/* split(s, delim) -> managed list<str>. Bridges the char** splitter to a
 * TorvikList: each piece (rc 0 from the splitter) is pushed into a managed
 * list, whose push retains it to rc 1 — so the list is its sole owner and
 * torvik_list_release frees every piece. The holder array is freed here. */
TorvikList *torvik_split_list(const char *s, const char *delim) {
    int64_t n = 0;
    char **parts = torvik_split(s, delim, &n);
    TorvikList *l = torvik_list_new();
    torvik_list_mark_managed(l);
    for (int64_t i = 0; i < n; i++) torvik_push(l, parts[i]);
    free(parts);
    return l;
}

/* --- table<K, V> --- */
/* Simple open-addressing hash table with string keys */
typedef struct TorvikTableEntry {
    char  *key;
    void  *value;
    int    used;
} TorvikTableEntry;

typedef struct {
    int64_t           magic;
    int64_t           refcount;
    int64_t           managed;   /* 1 when values are torvik strings (table<str,str>) */
    TorvikTableEntry *entries;
    int64_t           cap;
    int64_t           len;
} TorvikTable;
#define TORVIK_TABLE_MAGIC 0x544F52564B544142LL  /* "TORVKTAB" */

static uint64_t fnv1a(const char *s) {
    uint64_t h = 14695981039346656037ULL;
    for (; *s; s++) { h ^= (uint8_t)*s; h *= 1099511628211ULL; }
    return h;
}

TorvikTable *torvik_table_new(void) {
    TorvikTable *t = tv_malloc(sizeof(TorvikTable));
    t->magic    = TORVIK_TABLE_MAGIC;
    t->refcount = 0;
    t->managed  = 0;
    t->cap     = 16;
    t->len     = 0;
    t->entries = calloc(t->cap, sizeof(TorvikTableEntry));
    return t;
}

void torvik_table_mark_managed(void *p) {
    if (!p) return;
    if (*(int64_t *)p != TORVIK_TABLE_MAGIC) return;
    ((TorvikTable *)p)->managed = 1;
}

static void table_grow(TorvikTable *t) {
    int64_t old_cap = t->cap;
    TorvikTableEntry *old = t->entries;
    t->cap     *= 2;
    t->entries  = calloc(t->cap, sizeof(TorvikTableEntry));
    /* Re-insert: move each live entry into the new table. Keys and values move
     * as-is (no re-strdup, no refcount change) — ownership simply transfers. */
    for (int64_t i = 0; i < old_cap; i++) {
        if (old[i].used) {
            uint64_t h = fnv1a(old[i].key) % t->cap;
            while (t->entries[h].used) h = (h + 1) % t->cap;
            t->entries[h] = old[i];
        }
    }
    free(old);   /* len is unchanged: the same entries, rehomed */
}

void torvik_table_set(TorvikTable *t, const char *key, void *value) {
    if (t->len * 2 >= t->cap) table_grow(t);
    uint64_t h = fnv1a(key) % t->cap;
    while (t->entries[h].used && strcmp(t->entries[h].key, key) != 0)
        h = (h + 1) % t->cap;
    if (t->managed) torvik_str_retain(value);   /* retain new BEFORE releasing old */
    if (!t->entries[h].used) {
        t->entries[h].key  = tv_strdup(key);
        t->entries[h].used = 1;
        t->len++;
    } else if (t->managed) {
        torvik_str_release(t->entries[h].value); /* drop the overwritten value */
    }
    t->entries[h].value = value;
}

void *torvik_table_get(TorvikTable *t, const char *key) {
    uint64_t h = fnv1a(key) % t->cap;
    while (t->entries[h].used) {
        if (strcmp(t->entries[h].key, key) == 0) return t->entries[h].value;
        h = (h + 1) % t->cap;
    }
    return NULL;
}

int torvik_table_has(TorvikTable *t, const char *key) {
    return torvik_table_get(t, key) != NULL;
}

void torvik_table_del(TorvikTable *t, const char *key) {
    uint64_t h = fnv1a(key) % t->cap;
    while (t->entries[h].used) {
        if (strcmp(t->entries[h].key, key) == 0) {
            free(t->entries[h].key);
            if (t->managed) torvik_str_release(t->entries[h].value);
            t->entries[h].key  = NULL;
            t->entries[h].used = 0;
            t->len--;
            return;
        }
        h = (h + 1) % t->cap;
    }
}

int64_t torvik_table_len(TorvikTable *t) { return t->len; }

/* Table ARC: slot-counting like lists. Keys are tv_strdup'd C strings owned by
 * the table; values are torvik strings only when managed (table<str,str>). */
void *torvik_table_retain(void *p) {
    if (!p) return p;
    if (*(int64_t *)p != TORVIK_TABLE_MAGIC) return p;
    ((TorvikTable *)p)->refcount++;
    return p;
}

void torvik_table_release(void *p) {
    if (!p) return;
    if (*(int64_t *)p != TORVIK_TABLE_MAGIC) return;
    TorvikTable *t = (TorvikTable *)p;
    t->refcount--;
    if (t->refcount <= 0) {
        for (int64_t i = 0; i < t->cap; i++) {
            if (t->entries[i].used) {
                free(t->entries[i].key);
                if (t->managed) torvik_str_release(t->entries[i].value);
            }
        }
        free(t->entries);
        t->magic = 0;   /* poison against stale double-release */
        free(t);
    }
}

/* --- result<T> --- */
/* A refcounted ok/err box. Mirrors the table managed convention: a result<str>
   is "managed" — it owns (retains) its ok-value string. result<i64>/bare result
   is unmanaged (the value is an inttoptr i64, never refcounted). The err message
   is always a torvik string, so it is always retained/released. */
typedef struct {
    int64_t magic;
    int64_t refcount;   /* slot-counting, same convention as the other objects */
    int64_t managed;    /* 1 = ok-value is a torvik string (result<str>) */
    int64_t tag;        /* 0 = ok, 1 = err */
    void   *value;      /* ok payload (uniform ptr) */
    int64_t err_code;
    void   *err_msg;    /* err message (torvik str char*) — NULL on ok */
} TorvikResult;
#define TORVIK_RESULT_MAGIC 0x544F5256494B5253LL  /* "TORVIKRS" */

TorvikResult *torvik_result_ok(void *value) {
    TorvikResult *r = tv_malloc(sizeof(TorvikResult));
    r->magic    = TORVIK_RESULT_MAGIC;
    r->refcount = 0;        /* floating; codegen retains at production */
    r->managed  = 0;        /* codegen flips for result<str> via mark_managed */
    r->tag      = 0;
    r->value    = value;
    r->err_code = 0;
    r->err_msg  = NULL;
    return r;
}

TorvikResult *torvik_result_err(int64_t code, void *msg) {
    TorvikResult *r = tv_malloc(sizeof(TorvikResult));
    r->magic    = TORVIK_RESULT_MAGIC;
    r->refcount = 0;
    r->managed  = 0;
    r->tag      = 1;
    r->value    = NULL;
    r->err_code = code;
    r->err_msg  = msg;
    if (msg) torvik_str_retain(msg);   /* the box now holds the message */
    return r;
}

/* Flip an ok result<str> to managed: it now owns its string value. Idempotent. */
void torvik_result_mark_managed(void *p) {
    if (!p) return;
    if (*(int64_t *)p != TORVIK_RESULT_MAGIC) return;
    TorvikResult *r = (TorvikResult *)p;
    if (r->managed) return;
    r->managed = 1;
    if (r->tag == 0 && r->value) torvik_str_retain(r->value);
}

int64_t torvik_result_is_ok(void *p) {
    if (!p) return 0;
    if (*(int64_t *)p != TORVIK_RESULT_MAGIC) return 0;
    return ((TorvikResult *)p)->tag == 0 ? 1 : 0;
}

int64_t torvik_result_is_err(void *p) {
    if (!p) return 1;
    if (*(int64_t *)p != TORVIK_RESULT_MAGIC) return 1;
    return ((TorvikResult *)p)->tag == 0 ? 0 : 1;
}

void *torvik_result_unwrap(void *p) {
    if (p && *(int64_t *)p == TORVIK_RESULT_MAGIC) {
        TorvikResult *r = (TorvikResult *)p;
        if (r->tag == 0) return r->value;
        /* unwrap on err is a USER error (exit 1), not an internal fault */
        if (r->err_msg) fprintf(stderr, "error: unwrap() on an err result: %s\n", (char *)r->err_msg);
        else            fprintf(stderr, "error: unwrap() on an err result (code %lld)\n", (long long)r->err_code);
        exit(1);
    }
    fprintf(stderr, "error: unwrap() on a non-result value\n");
    exit(1);
}

void *torvik_result_unwrap_or(void *p, void *dflt) {
    if (!p) return dflt;
    if (*(int64_t *)p != TORVIK_RESULT_MAGIC) return dflt;
    TorvikResult *r = (TorvikResult *)p;
    return r->tag == 0 ? r->value : dflt;
}

int64_t torvik_result_err_code(void *p) {
    if (!p) return 0;
    if (*(int64_t *)p != TORVIK_RESULT_MAGIC) return 0;
    return ((TorvikResult *)p)->err_code;
}

void *torvik_result_err_msg(void *p) {
    if (!p) return torvik_str_dup("");
    if (*(int64_t *)p != TORVIK_RESULT_MAGIC) return torvik_str_dup("");
    TorvikResult *r = (TorvikResult *)p;
    if (r->err_msg) return r->err_msg;
    return torvik_str_dup("");
}

void *torvik_result_retain(void *p) {
    if (!p) return p;
    if (*(int64_t *)p != TORVIK_RESULT_MAGIC) return p;
    ((TorvikResult *)p)->refcount++;
    return p;
}

void torvik_result_release(void *p) {
    if (!p) return;
    if (*(int64_t *)p != TORVIK_RESULT_MAGIC) return;
    TorvikResult *r = (TorvikResult *)p;
    r->refcount--;
    if (r->refcount <= 0) {
        if (r->tag == 0) {
            if (r->managed && r->value) torvik_str_release(r->value);
        } else {
            if (r->err_msg) torvik_str_release(r->err_msg);
        }
        r->magic = 0;   /* poison against stale double-release */
        free(r);
    }
}

/* --- fallible builtins: return result<T> instead of panicking --- */
void *torvik_try_readfile(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        char m[512];
        snprintf(m, sizeof m, "cannot open '%s': %s", path, strerror(errno));
        return torvik_result_err((int64_t)errno, torvik_str_dup(m));
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    char *buf = torvik_str_alloc(size);
    fread(buf, 1, size, f);
    fclose(f);
    void *r = torvik_result_ok(buf);
    torvik_result_mark_managed(r);   /* owns the contents regardless of the caller */
    return r;
}

void *torvik_try_toint(const char *s) {
    char *end;
    errno = 0;
    long long v = strtoll(s, &end, 10);
    if (s[0] == '\0' || *end != '\0' || errno != 0) {
        char m[256];
        snprintf(m, sizeof m, "invalid integer: '%s'", s);
        return torvik_result_err(1, torvik_str_dup(m));
    }
    return torvik_result_ok((void *)(intptr_t)v);
}

void *torvik_try_tofloat(const char *s) {
    char *end;
    errno = 0;
    double v = strtod(s, &end);
    if (s[0] == '\0' || *end != '\0' || errno != 0) {
        char m[256];
        snprintf(m, sizeof m, "invalid float: '%s'", s);
        return torvik_result_err(1, torvik_str_dup(m));
    }
    int64_t bits;
    memcpy(&bits, &v, sizeof bits);
    return torvik_result_ok((void *)(intptr_t)bits);
}

/* --- bag<T> --- */
/* Bag = set of unique string values */
#define TORVIK_BAG_MAGIC 0x544F52564B424147LL  /* "TORVKBAG" */
typedef struct {
    int64_t magic;
    int64_t refcount;
    char  **items;
    int64_t len;
    int64_t cap;
} TorvikBag;

TorvikBag *torvik_bag_new(void) {
    TorvikBag *b = tv_malloc(sizeof(TorvikBag));
    b->magic    = TORVIK_BAG_MAGIC;
    b->refcount = 0;
    b->cap   = 8;
    b->len   = 0;
    b->items = tv_malloc(b->cap * sizeof(char*));
    return b;
}

int torvik_bag_has(TorvikBag *b, const char *item) {
    for (int64_t i = 0; i < b->len; i++)
        if (strcmp(b->items[i], item) == 0) return 1;
    return 0;
}

void torvik_bag_add(TorvikBag *b, const char *item) {
    if (torvik_bag_has(b, item)) return; /* ignore duplicates */
    if (b->len >= b->cap) {
        b->cap *= 2;
        b->items = tv_realloc(b->items, b->cap * sizeof(char*));
    }
    b->items[b->len++] = tv_strdup(item);
}

void torvik_bag_remove(TorvikBag *b, const char *item) {
    for (int64_t i = 0; i < b->len; i++) {
        if (strcmp(b->items[i], item) == 0) {
            free(b->items[i]);
            memmove(&b->items[i], &b->items[i+1], (b->len - i - 1) * sizeof(char*));
            b->len--;
            return;
        }
    }
}

int64_t torvik_bag_len(TorvikBag *b) { return b->len; }

/* Bag ARC: slot-counting like lists. Items are tv_strdup'd C strings owned
 * solely by the bag, so release frees them directly (no torvik_str refcount). */
void *torvik_bag_retain(void *p) {
    if (!p) return p;
    if (*(int64_t *)p != TORVIK_BAG_MAGIC) return p;
    ((TorvikBag *)p)->refcount++;
    return p;
}

void torvik_bag_release(void *p) {
    if (!p) return;
    if (*(int64_t *)p != TORVIK_BAG_MAGIC) return;
    TorvikBag *b = (TorvikBag *)p;
    b->refcount--;
    if (b->refcount <= 0) {
        for (int64_t i = 0; i < b->len; i++) free(b->items[i]);
        free(b->items);
        b->magic = 0;   /* poison against stale double-release */
        free(b);
    }
}

/* ── 9. System Info (sys module) ─────────────────────────────────────────────── */

char *torvik_sys_os_name(void) {
#if defined(__linux__)
    return torvik_str_dup("linux");
#elif defined(__APPLE__)
    return torvik_str_dup("macos");
#elif defined(_WIN32)
    return torvik_str_dup("windows");
#else
    return torvik_str_dup("unknown");
#endif
}

char *torvik_sys_os_version(void) {
    struct utsname u;
    uname(&u);
    return torvik_str_dup(u.release);
}

char *torvik_sys_arch(void) {
    struct utsname u;
    uname(&u);
    return torvik_str_dup(u.machine);
}

char *torvik_sys_hostname(void) {
    struct utsname u;
    uname(&u);
    return torvik_str_dup(u.nodename);
}

char *torvik_sys_username(void) {
    const char *u = getenv("USER");
    if (!u) u = getenv("LOGNAME");
    return torvik_str_dup(u ? u : "unknown");
}

char *torvik_sys_home_dir(void) {
    const char *h = getenv("HOME");
    return torvik_str_dup(h ? h : "/");
}

char *torvik_sys_cwd(void) {
    char buf[4096];
    if (getcwd(buf, sizeof(buf))) return torvik_str_dup(buf);
    return torvik_str_dup(".");
}

int64_t torvik_sys_cpu_count(void) {
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return n > 0 ? (int64_t)n : 1;
}

int64_t torvik_sys_mem_total(void) {
    struct sysinfo si;
    if (sysinfo(&si) == 0) return (int64_t)si.totalram * si.mem_unit;
    return 0;
}

int64_t torvik_sys_mem_free(void) {
    struct sysinfo si;
    if (sysinfo(&si) == 0) return (int64_t)si.freeram * si.mem_unit;
    return 0;
}

int64_t torvik_sys_pid(void) {
    return (int64_t)getpid();
}

int64_t torvik_sys_run(const char *cmd) {
    int rc = system(cmd);
    if (rc == -1) return 127;                              /* shell could not be launched */
    if (WIFEXITED(rc))   return (int64_t)WEXITSTATUS(rc);  /* normal exit: real 0-255 code */
    if (WIFSIGNALED(rc)) return (int64_t)(128 + WTERMSIG(rc)); /* killed by signal */
    return (int64_t)rc;
}

/* fs_exists — 1 if path exists (replaces shell `test -e`) */
int64_t torvik_fs_exists(const char *path) {
    return access(path, F_OK) == 0 ? 1 : 0;
}

/* fs_mtime — modification time in seconds, or -1 if the path is missing */
int64_t torvik_fs_mtime(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    return (int64_t)st.st_mtime;
}

/* fs_mkdir — create a directory path (like `mkdir -p`) */
void torvik_fs_mkdir(const char *path) {
    char tmp[4096];
    size_t n = strlen(path);
    if (n == 0 || n >= sizeof(tmp)) return;
    memcpy(tmp, path, n + 1);
    if (tmp[n-1] == '/') tmp[n-1] = '\0';
    for (char *q = tmp + 1; *q; q++) {
        if (*q == '/') { *q = '\0'; mkdir(tmp, 0755); *q = '/'; }
    }
    mkdir(tmp, 0755);
}

/* fs_remove — recursively delete a file or directory (like `rm -rf`) */
void torvik_fs_remove(const char *path) {
    struct stat st;
    if (lstat(path, &st) != 0) return;
    if (S_ISDIR(st.st_mode)) {
        DIR *d = opendir(path);
        if (d) {
            struct dirent *e;
            while ((e = readdir(d)) != NULL) {
                if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
                char child[4096];
                snprintf(child, sizeof(child), "%s/%s", path, e->d_name);
                torvik_fs_remove(child);
            }
            closedir(d);
        }
        rmdir(path);
    } else {
        unlink(path);
    }
}

// Write a list<str> to a file, one element per line
// This avoids O(N^2) string concatenation in join_ir
void torvik_write_ir(TorvikList *ir_list, const char *path, int64_t append) {
    if (!ir_list || !path) return;
    FILE *f = fopen(path, append ? "a" : "w");
    if (!f) return;
    for (int64_t i = 0; i < ir_list->len; i++) {
        const char *line = (const char *)(uintptr_t)ir_list->data[i];
        if (line) {
            fputs(line, f);
            fputc('\n', f);
        }
    }
    fclose(f);
}

// Append a line to a file (with newline)
void torvik_appendline(const char *data, const char *path) {
    if (!data || !path) return;
    FILE *f = fopen(path, "a");
    if (!f) return;
    fputs(data, f);
    fputc('\n', f);
    fclose(f);
}

/* === Self-hosting string primitives (added for bootstrap) === */
int64_t torvik_char_at(const char *s, int64_t i) {
    return (int64_t)(unsigned char)s[i];
}

/* char_str — the character at index i as a fresh 1-char (headered) string.
   Out-of-range/negative indices yield "" (matches substr's safe clamping). */
char *torvik_char_str(const char *s, int64_t i) {
    int64_t slen = (int64_t)strlen(s);
    if (i < 0 || i >= slen) return torvik_str_alloc(0);
    char *buf = torvik_str_alloc(1);
    buf[0] = s[i];
    return buf;
}

char *torvik_substr(const char *s, int64_t start, int64_t end) {
    /* Security: clamp the range into [0, strlen(s)] so out-of-range or negative
       indices can never read past the buffer (a heap over-read / info leak).
       Matches Python/JS slice semantics — an empty result for an empty range. */
    int64_t slen = (int64_t)strlen(s);
    if (start < 0) start = 0;
    if (start > slen) start = slen;
    if (end < start) end = start;
    if (end > slen) end = slen;
    int64_t len = end - start;
    char *buf = torvik_str_alloc((size_t)len);
    memcpy(buf, s + start, (size_t)len);
    return buf;
}

char *torvik_int_to_str(int64_t n) {
    char *buf = torvik_str_alloc(31);
    snprintf(buf, 32, "%ld", (long)n);
    return buf;
}

/* uint_to_str — u64 formatting: the same 64 bits read as unsigned (%lu). */
char *torvik_uint_to_str(int64_t n) {
    char *buf = torvik_str_alloc(31);
    snprintf(buf, 32, "%llu", (unsigned long long)(uint64_t)n);
    return buf;
}

/* bool_to_str — canonical "true"/"false" for echo/interpolation */
char *torvik_bool_to_str(int64_t b) {
    return torvik_str_dup(b ? "true" : "false");
}

/* list_to_str — "[e0, e1, ...]" for echo/interpolation. Element formatting is
   driven by the list's own `managed` flag (set only for list<str>), so we never
   guess: managed -> char* elements printed as-is, otherwise raw i64 elements. */
char *torvik_list_to_str(TorvikList *l) {
    if (!l) return torvik_str_dup("[]");
    size_t cap = 32, len = 0;
    char *buf = (char *)tv_malloc(cap);
    buf[0] = '\0';
    #define TV_LS_APPEND(SRC) do {                                   \
        const char *_s = (SRC); size_t _sl = strlen(_s);             \
        while (len + _sl + 1 > cap) { cap *= 2; buf = (char *)tv_realloc(buf, cap); } \
        memcpy(buf + len, _s, _sl); len += _sl; buf[len] = '\0';     \
    } while (0)
    TV_LS_APPEND("[");
    for (int64_t i = 0; i < l->len; i++) {
        if (i > 0) TV_LS_APPEND(", ");
        if (l->managed) {
            const char *e = (const char *)l->data[i];
            TV_LS_APPEND(e ? e : "");
        } else {
            char nb[32];
            snprintf(nb, sizeof(nb), "%ld", (long)(int64_t)(intptr_t)l->data[i]);
            TV_LS_APPEND(nb);
        }
    }
    TV_LS_APPEND("]");
    #undef TV_LS_APPEND
    char *out = torvik_str_dup(buf);
    free(buf);
    return out;
}

char *torvik_str_concat(const char *a, const char *b) {
    size_t la = strlen(a), lb = strlen(b);
    char *buf = torvik_str_alloc(la + lb);
    memcpy(buf, a, la);
    memcpy(buf + la, b, lb);
    return buf;
}

int torvik_str_eq(const char *a, const char *b) {
    return strcmp(a, b) == 0;
}

