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
#include <sys/stat.h>
#include <errno.h>

/* ── Platform layer ──────────────────────────────────────────────────────────
   Torvik's runtime is one shared C file across Linux, macOS, and Windows. The
   POSIX headers and the Win32 API cover the same ground with different names, so
   the platform-divergent pieces (terminal raw mode, process id, wall-clock ms,
   OS/arch/host info, memory stats, running a command, directory walk) each carry
   a _WIN32 branch. Everything else is portable C and compiles unchanged. */
#if defined(_WIN32)
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #include <windows.h>
  #include <direct.h>
  #include <io.h>
  #include <conio.h>
  #include <process.h>
  /* MSVCRT names a few POSIX-ish calls with leading underscores. */
  #define torvik_getcwd _getcwd
  #define torvik_access _access
  #define F_OK 0
#else
  #include <unistd.h>
  #include <dirent.h>
  #include <fcntl.h>
  #include <termios.h>
  #include <pthread.h>
  #include <sys/time.h>
  #include <sys/utsname.h>
  #include <sys/sysinfo.h>
  #include <sys/wait.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <netinet/tcp.h>
  #include <arpa/inet.h>
  #include <signal.h>
  #define torvik_getcwd getcwd
  #define torvik_access access
#endif

/* getline() is POSIX; the mingw/MSVC CRT doesn't provide it. Supply a minimal,
   compatible implementation on Windows: grows *lineptr as needed and returns the
   number of bytes read (including the newline), or -1 at EOF. Semantics match
   what torvik_read relies on. */
#if defined(_WIN32)
#include <sys/types.h>   /* ssize_t via mingw */
static ssize_t torvik_getline_impl(char **lineptr, size_t *n, FILE *stream) {
    if (!lineptr || !n || !stream) return -1;
    size_t cap = *n;
    char *buf = *lineptr;
    if (!buf || cap == 0) { cap = 128; buf = (char *)malloc(cap); if (!buf) return -1; }
    size_t len = 0;
    int c;
    while ((c = fgetc(stream)) != EOF) {
        if (len + 1 >= cap) {
            size_t ncap = cap * 2;
            char *nb = (char *)realloc(buf, ncap);
            if (!nb) { *lineptr = buf; *n = cap; return -1; }
            buf = nb; cap = ncap;
        }
        buf[len++] = (char)c;
        if (c == '\n') break;
    }
    if (len == 0 && c == EOF) { *lineptr = buf; *n = cap; return -1; }
    buf[len] = '\0';
    *lineptr = buf; *n = cap;
    return (ssize_t)len;
}
#define getline torvik_getline_impl
#endif

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

/* NOTE on the __int128 calling convention below: 128-bit integers are passed and
   returned BY POINTER, never by value. The Win64 ABI passes/returns __int128
   incompatibly with the way LLVM lowers a by-value i128 in the generated IR, which
   corrupts the value on Windows. Passing a pointer to the 16-byte value works
   identically on every target (System V and Win64 alike), so box/load/to_str/div/
   mod all take __int128* in and write results through an out pointer. retain and
   release already operate on the box pointer, so they are unchanged. */

void *torvik_i128_box(__int128 *vp) {
    TorvikI128Box *b = (TorvikI128Box *)tv_malloc(sizeof(TorvikI128Box));
    b->magic = TORVIK_I128_MAGIC;
    b->refcount = 0;
    b->value = *vp;
    return (char *)b + 16;   /* -> value field */
}
void torvik_i128_load(void *p, __int128 *out) {
    *out = *(__int128 *)p;
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
char *torvik_u128_to_str(unsigned __int128 *vp) {
    unsigned __int128 v = *vp;
    char tmp[44]; int i = 44;
    if (v == 0) tmp[--i] = '0';
    while (v > 0) { tmp[--i] = (char)('0' + (int)(v % 10)); v /= 10; }
    char *out = torvik_str_alloc((size_t)(44 - i));
    memcpy(out, tmp + i, (size_t)(44 - i));
    return out;
}
char *torvik_i128_to_str(__int128 *vp) {
    __int128 v = *vp;
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

/* unsigned/signed 128-bit div + mod with zero-guard (mirrors torvik_div/mod).
   Operands and result all travel by pointer (see the convention note above). */
void torvik_i128_div(__int128 *a, __int128 *b, __int128 *out) {
    if (*b == 0) torvik_panic("division by zero");
    *out = *a / *b;
}
void torvik_i128_mod(__int128 *a, __int128 *b, __int128 *out) {
    if (*b == 0) torvik_panic("modulo by zero");
    *out = *a % *b;
}
void torvik_u128_div(__int128 *a, __int128 *b, __int128 *out) {
    if (*b == 0) torvik_panic("division by zero");
    *out = (__int128)((unsigned __int128)*a / (unsigned __int128)*b);
}
void torvik_u128_mod(__int128 *a, __int128 *b, __int128 *out) {
    if (*b == 0) torvik_panic("modulo by zero");
    *out = (__int128)((unsigned __int128)*a % (unsigned __int128)*b);
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
#if defined(_WIN32)
    /* v1.1.3: _getch() reads the CONSOLE device, so with redirected stdin
       (pipes, files, test harnesses) it blocked waiting for a real keypress
       and ignored the piped input entirely. Use the console only when stdin
       IS the console; otherwise read the redirected stream. */
    if (_isatty(_fileno(stdin))) {
        return (char)_getch();
    }
    int c = getchar();
    return (char)(c == EOF ? 0 : c);
#else
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
#endif
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

/* find — byte index of the first occurrence of sub in s, or -1 (v1.2.0).
   An empty sub finds index 0, matching strstr semantics. */
int64_t torvik_find(const char *s, const char *sub) {
    const char *hit = strstr(s, sub);
    if (!hit) return -1;
    return (int64_t)(hit - s);
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

/* readenv — returns the variable's value, or "" when the variable is not set.
   (v1.1.0 fix: this used to return NULL for an unset variable, which the
   Torvik side cannot hold — any readenv() of an unset variable segfaulted.) */
char *torvik_readenv(const char *name) {
    const char *v = getenv(name);
    return torvik_str_dup(v ? v : "");
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
#if defined(_WIN32)
    srand((unsigned)time(NULL) ^ (unsigned)GetCurrentProcessId());
#else
    srand((unsigned)time(NULL) ^ (unsigned)getpid());
#endif
}

/* ── 6. Time & Terminal ──────────────────────────────────────────────────────── */

/* sleep(ms) — sleep for N milliseconds */
void torvik_sleep(int64_t ms) {
    if (ms <= 0) return;
#if defined(_WIN32)
    Sleep((DWORD)ms);
#else
    struct timespec ts;
    ts.tv_sec  = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
#endif
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
#if defined(_WIN32)
    /* FILETIME is 100-ns ticks since 1601-01-01; shift the epoch to 1970 and
       convert to milliseconds. 11644473600 seconds separate the two epochs. */
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    unsigned long long t = ((unsigned long long)ft.dwHighDateTime << 32)
                         | (unsigned long long)ft.dwLowDateTime;
    t /= 10000ULL;                      /* 100-ns ticks -> ms */
    t -= 11644473600000ULL;             /* 1601 epoch -> 1970 epoch */
    return (int64_t)t;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000LL + (int64_t)tv.tv_usec / 1000LL;
#endif
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

/* typeof is resolved at COMPILE TIME (Torvik is statically typed) — the
   compiler interns the type name directly, so no runtime helpers exist. */

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
int64_t torvik_list_contains(TorvikList *l, int64_t value) {  /* v1.1.3: i64 return to match the declared IR ABI (long is 32-bit on Win64) */
    if (!l) return 0;
    for (int64_t i = 0; i < l->len; i++) {
        if ((int64_t)l->data[i] == value) return 1;
    }
    return 0;
}

/* Membership for list<str>: 1 if `s` CONTENT-equals any element (strcmp), else
   0. Elements are char* riding the int64 data slots. NULL-safe on both sides.
   (v1.1.0: `<|` on string lists matches by value, not identity.) */
int64_t torvik_list_contains_str(TorvikList *l, const char *s) {  /* v1.1.3: i64 return, see above */
    if (!l || !s) return 0;
    for (int64_t i = 0; i < l->len; i++) {
        const char *e = (const char *)l->data[i];
        if (e && strcmp(e, s) == 0) return 1;
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
#if defined(_WIN32)
    /* RtlGetVersion is the reliable route, but for a simple string GetVersionEx
       is adequate; report "major.minor.build". */
    OSVERSIONINFOA vi;
    memset(&vi, 0, sizeof(vi));
    vi.dwOSVersionInfoSize = sizeof(vi);
#if defined(__GNUC__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    if (GetVersionExA(&vi)) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%lu.%lu.%lu",
                 (unsigned long)vi.dwMajorVersion,
                 (unsigned long)vi.dwMinorVersion,
                 (unsigned long)vi.dwBuildNumber);
        return torvik_str_dup(buf);
    }
#if defined(__GNUC__)
    #pragma GCC diagnostic pop
#endif
    return torvik_str_dup("unknown");
#else
    struct utsname u;
    uname(&u);
    return torvik_str_dup(u.release);
#endif
}

char *torvik_sys_arch(void) {
#if defined(_WIN32)
    SYSTEM_INFO si;
    GetNativeSystemInfo(&si);
    switch (si.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64: return torvik_str_dup("x86_64");
        case PROCESSOR_ARCHITECTURE_ARM64: return torvik_str_dup("arm64");
        case PROCESSOR_ARCHITECTURE_INTEL: return torvik_str_dup("x86");
        default:                           return torvik_str_dup("unknown");
    }
#else
    struct utsname u;
    uname(&u);
    return torvik_str_dup(u.machine);
#endif
}

char *torvik_sys_hostname(void) {
#if defined(_WIN32)
    char buf[256];
    DWORD n = (DWORD)sizeof(buf);
    if (GetComputerNameA(buf, &n)) return torvik_str_dup(buf);
    return torvik_str_dup("unknown");
#else
    struct utsname u;
    uname(&u);
    return torvik_str_dup(u.nodename);
#endif
}

char *torvik_sys_username(void) {
#if defined(_WIN32)
    const char *u = getenv("USERNAME");
    return torvik_str_dup(u ? u : "unknown");
#else
    const char *u = getenv("USER");
    if (!u) u = getenv("LOGNAME");
    return torvik_str_dup(u ? u : "unknown");
#endif
}

char *torvik_sys_home_dir(void) {
#if defined(_WIN32)
    const char *h = getenv("USERPROFILE");
    return torvik_str_dup(h ? h : "C:\\");
#else
    const char *h = getenv("HOME");
    return torvik_str_dup(h ? h : "/");
#endif
}

char *torvik_sys_cwd(void) {
    char buf[4096];
    if (torvik_getcwd(buf, sizeof(buf))) return torvik_str_dup(buf);
    return torvik_str_dup(".");
}

int64_t torvik_sys_cpu_count(void) {
#if defined(_WIN32)
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return si.dwNumberOfProcessors > 0 ? (int64_t)si.dwNumberOfProcessors : 1;
#else
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return n > 0 ? (int64_t)n : 1;
#endif
}

int64_t torvik_sys_mem_total(void) {
#if defined(_WIN32)
    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms)) return (int64_t)ms.ullTotalPhys;
    return 0;
#else
    struct sysinfo si;
    if (sysinfo(&si) == 0) return (int64_t)si.totalram * si.mem_unit;
    return 0;
#endif
}

int64_t torvik_sys_mem_free(void) {
#if defined(_WIN32)
    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms)) return (int64_t)ms.ullAvailPhys;
    return 0;
#else
    struct sysinfo si;
    if (sysinfo(&si) == 0) return (int64_t)si.freeram * si.mem_unit;
    return 0;
#endif
}

int64_t torvik_sys_pid(void) {
#if defined(_WIN32)
    return (int64_t)GetCurrentProcessId();
#else
    return (int64_t)getpid();
#endif
}

int64_t torvik_sys_run(const char *cmd) {
    /* v1.3.0: the child writes to the inherited fds immediately, while our own
       buffered stdio may still be holding earlier output (rune's replayed
       diagnostics, for one) - flush so parent output always precedes the
       child's. */
    fflush(stdout);
    fflush(stderr);
#if defined(_WIN32)
    /* On Windows system() returns the command's exit code directly (or -1 if the
       command interpreter can't be started). No WEXITSTATUS wrapping needed. */
    int rc = system(cmd);
    if (rc == -1) return 127;
    return (int64_t)rc;
#else
    int rc = system(cmd);
    if (rc == -1) return 127;                              /* shell could not be launched */
    if (WIFEXITED(rc))   return (int64_t)WEXITSTATUS(rc);  /* normal exit: real 0-255 code */
    if (WIFSIGNALED(rc)) return (int64_t)(128 + WTERMSIG(rc)); /* killed by signal */
    return (int64_t)rc;
#endif
}

/* fs_exists — 1 if path exists (replaces shell `test -e`) */
int64_t torvik_fs_exists(const char *path) {
    return torvik_access(path, F_OK) == 0 ? 1 : 0;
}

/* fs_mtime — modification time in seconds, or -1 if the path is missing */
int64_t torvik_fs_mtime(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    return (int64_t)st.st_mtime;
}

/* fs_size — file size in bytes, or -1 if the path is missing or not a file.
   std::net uses it for a byte-exact Content-Length, but it is generally useful. */
int64_t torvik_fs_size(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    if ((st.st_mode & S_IFMT) == S_IFDIR) return -1;
    return (int64_t)st.st_size;
}

/* Create a single directory. On Windows _mkdir takes one argument; elsewhere
   mkdir takes a mode. A path separator is '/' on POSIX and either '/' or '\\'
   on Windows (the shell and APIs accept both). */
static int torvik_mkdir_one(const char *p) {
#if defined(_WIN32)
    return _mkdir(p);
#else
    return mkdir(p, 0755);
#endif
}

static int torvik_is_sep(char c) {
#if defined(_WIN32)
    return c == '/' || c == '\\';
#else
    return c == '/';
#endif
}

/* fs_mkdir — create a directory path (like `mkdir -p`) */
void torvik_fs_mkdir(const char *path) {
    char tmp[4096];
    size_t n = strlen(path);
    if (n == 0 || n >= sizeof(tmp)) return;
    memcpy(tmp, path, n + 1);
    if (torvik_is_sep(tmp[n-1])) tmp[n-1] = '\0';
    for (char *q = tmp + 1; *q; q++) {
        if (torvik_is_sep(*q)) { char sep = *q; *q = '\0'; torvik_mkdir_one(tmp); *q = sep; }
    }
    torvik_mkdir_one(tmp);
}

/* fs_is_dir — 1 if the path exists and is a directory (v1.3.0) */
int64_t torvik_fs_is_dir(const char *path) {
#if defined(_WIN32)
    DWORD a = GetFileAttributesA(path);
    if (a == INVALID_FILE_ATTRIBUTES) return 0;
    return (a & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0;
#else
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISDIR(st.st_mode) ? 1 : 0;
#endif
}

/* dir_list — the entry NAMES in a directory as a managed list<str>, sorted
   bytewise for deterministic output across platforms and filesystems ("." and
   ".." excluded). An unopenable path is a clean panic, matching readfile:
   pointing a tool at the wrong directory should be loud, not an empty list
   (v1.3.0). */
static int torvik_dl_cmp(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}
/* table_keys — every key currently in the table, as a sorted list<str>
   (v1.3.0). Sorted because hash order shifts as the table resizes: loops over
   a table must be reproducible run to run. The list adopts fresh dups of the
   keys, so mutating the table afterward cannot disturb a captured key list. */
TorvikList *torvik_table_keys(void *p) {
    TorvikTable *t = (TorvikTable *)p;
    if (!t || t->magic != TORVIK_TABLE_MAGIC) torvik_panic("table_keys: this value is not a table");
    char **names = (char **)tv_malloc(sizeof(char *) * (size_t)(t->len > 0 ? t->len : 1));
    int64_t n = 0;
    for (int64_t i = 0; i < t->cap; i++) {
        if (t->entries[i].used) names[n++] = torvik_str_dup(t->entries[i].key);
    }
    if (n > 1) qsort(names, (size_t)n, sizeof(char *), torvik_dl_cmp);
    TorvikList *l = torvik_list_new();
    torvik_list_mark_managed(l);
    for (int64_t i = 0; i < n; i++) torvik_push(l, names[i]);   /* push adopts */
    free(names);
    return l;
}

TorvikList *torvik_dir_list(const char *path) {
    char  **names = NULL;
    int64_t n = 0, cap = 0;
#if defined(_WIN32)
    char pat[4096];
    size_t plen = strlen(path);
    if (plen == 0 || plen + 3 >= sizeof(pat)) torvik_panic("dir_list: path too long");
    memcpy(pat, path, plen);
    if (pat[plen-1] != '/' && pat[plen-1] != '\\') pat[plen++] = '\\';
    pat[plen] = '*'; pat[plen+1] = '\0';
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) {
        char msg[4200];
        snprintf(msg, sizeof(msg), "dir_list: cannot open '%s'", path);
        torvik_panic(msg);
    }
    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
        if (n == cap) { cap = cap ? cap * 2 : 16; names = tv_realloc(names, (size_t)cap * sizeof(char *)); }
        names[n++] = torvik_str_dup(fd.cFileName);   /* header'd - the list adopts it */
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    DIR *d = opendir(path);
    if (!d) {
        char msg[4200];
        snprintf(msg, sizeof(msg), "dir_list: cannot open '%s': %s", path, strerror(errno));
        torvik_panic(msg);
    }
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        if (n == cap) { cap = cap ? cap * 2 : 16; names = tv_realloc(names, (size_t)cap * sizeof(char *)); }
        names[n++] = torvik_str_dup(e->d_name);   /* header'd - the list adopts it */
    }
    closedir(d);
#endif
    if (n > 1) qsort(names, (size_t)n, sizeof(char *), torvik_dl_cmp);
    TorvikList *l = torvik_list_new();
    torvik_list_mark_managed(l);
    /* push ADOPTS each string (mirrors torvik_split_list: the strings transfer
       to the list; only the scratch array is freed). */
    for (int64_t i = 0; i < n; i++) torvik_push(l, names[i]);
    free(names);
    return l;
}

/* fs_copy — binary-safe file copy (v1.3.0). readfile is NUL-terminated text,
   so images and other binary assets cannot round-trip through it; this streams
   raw bytes. Failures are clean panics, matching readfile/writefile. */
void torvik_fs_copy(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    if (!in) {
        char msg[4200];
        snprintf(msg, sizeof(msg), "fs_copy: cannot open '%s': %s", src, strerror(errno));
        torvik_panic(msg);
    }
    FILE *out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        char msg[4200];
        snprintf(msg, sizeof(msg), "fs_copy: cannot create '%s': %s", dst, strerror(errno));
        torvik_panic(msg);
    }
    char buf[65536];
    size_t r;
    while ((r = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, r, out) != r) {
            fclose(in); fclose(out);
            torvik_panic("fs_copy: short write (disk full?)");
        }
    }
    fclose(in);
    if (fclose(out) != 0) torvik_panic("fs_copy: close failed (disk full?)");
}

/* try_writefile / try_appendline / try_fs_copy — recoverable forms of the
   matching builtins (v1.3.0): a failure is an err result the program can
   inspect and continue from, instead of a halt. Success is ok(0). */
void *torvik_try_writefile(const char *path, const char *data) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        char m[4200];
        snprintf(m, sizeof m, "cannot open '%s': %s", path, strerror(errno));
        return torvik_result_err((int64_t)errno, torvik_str_dup(m));
    }
    if (fputs(data, f) < 0 || fclose(f) != 0) {
        char m[4200];
        snprintf(m, sizeof m, "write to '%s' failed: %s", path, strerror(errno));
        return torvik_result_err((int64_t)errno, torvik_str_dup(m));
    }
    return torvik_result_ok((void *)(intptr_t)0);
}

void *torvik_try_appendline(const char *path, const char *line) {
    FILE *f = fopen(path, "ab");
    if (!f) {
        char m[4200];
        snprintf(m, sizeof m, "cannot open '%s': %s", path, strerror(errno));
        return torvik_result_err((int64_t)errno, torvik_str_dup(m));
    }
    if (fputs(line, f) < 0 || fputc('\n', f) == EOF || fclose(f) != 0) {
        char m[4200];
        snprintf(m, sizeof m, "append to '%s' failed: %s", path, strerror(errno));
        return torvik_result_err((int64_t)errno, torvik_str_dup(m));
    }
    return torvik_result_ok((void *)(intptr_t)0);
}

void *torvik_try_fs_copy(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    if (!in) {
        char m[4200];
        snprintf(m, sizeof m, "cannot open '%s': %s", src, strerror(errno));
        return torvik_result_err((int64_t)errno, torvik_str_dup(m));
    }
    FILE *out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        char m[4200];
        snprintf(m, sizeof m, "cannot create '%s': %s", dst, strerror(errno));
        return torvik_result_err((int64_t)errno, torvik_str_dup(m));
    }
    char buf[65536];
    size_t r;
    while ((r = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, r, out) != r) {
            fclose(in); fclose(out);
            return torvik_result_err(1, torvik_str_dup("short write (disk full?)"));
        }
    }
    fclose(in);
    if (fclose(out) != 0) return torvik_result_err(1, torvik_str_dup("close failed (disk full?)"));
    return torvik_result_ok((void *)(intptr_t)0);
}

/* fs_remove — recursively delete a file or directory (like `rm -rf`) */
#if defined(_WIN32)
void torvik_fs_remove(const char *path) {
    DWORD attr = GetFileAttributesA(path);
    if (attr == INVALID_FILE_ATTRIBUTES) return;
    /* If this is a reparse point (symlink / junction), delete the link itself
       and do NOT recurse through it — following it would let the deletion escape
       into the target directory (the symlink-swap TOCTOU risk CodeQL flags). */
    if (attr & FILE_ATTRIBUTE_REPARSE_POINT) {
        if (attr & FILE_ATTRIBUTE_DIRECTORY) _rmdir(path);
        else remove(path);
        return;
    }
    if (attr & FILE_ATTRIBUTE_DIRECTORY) {
        char pattern[4096];
        snprintf(pattern, sizeof(pattern), "%s\\*", path);
        WIN32_FIND_DATAA fd;
        HANDLE h = FindFirstFileA(pattern, &fd);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                if (!strcmp(fd.cFileName, ".") || !strcmp(fd.cFileName, "..")) continue;
                char child[4096];
                snprintf(child, sizeof(child), "%s\\%s", path, fd.cFileName);
                torvik_fs_remove(child);
            } while (FindNextFileA(h, &fd));
            FindClose(h);
        }
        _rmdir(path);
    } else {
        remove(path);
    }
}
#else
/* TOCTOU-safe recursive delete.
   The classic "lstat then opendir/rmdir on the same path" pattern has a race:
   the path can be swapped between the check and the use (e.g. a directory
   replaced by a symlink), so the operation lands somewhere unintended. We avoid
   the race by never re-resolving a path from a string mid-operation. Instead we
   recurse over directory FILE DESCRIPTORS: open a dir fd once with O_NOFOLLOW,
   then use fstatat / unlinkat / openat *relative to that fd* with
   AT_SYMLINK_NOFOLLOW. Each entry is resolved exactly once, relative to a fd we
   already hold, so there is no check-then-reresolve window to race, and symlinks
   are never traversed. */
static void torvik_fs_remove_at(int dirfd, const char *name) {
    struct stat st;
    if (fstatat(dirfd, name, &st, AT_SYMLINK_NOFOLLOW) != 0) return;
    if (S_ISDIR(st.st_mode)) {
        /* Open the child directory without following a symlink, then recurse
           over a fd obtained from that same open (no path re-resolution). */
        int cfd = openat(dirfd, name, O_RDONLY | O_NOFOLLOW | O_DIRECTORY);
        if (cfd >= 0) {
            DIR *d = fdopendir(cfd);
            if (d) {
                struct dirent *e;
                while ((e = readdir(d)) != NULL) {
                    if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
                    torvik_fs_remove_at(cfd, e->d_name);
                }
                closedir(d);            /* also closes cfd */
            } else {
                close(cfd);
            }
        }
        unlinkat(dirfd, name, AT_REMOVEDIR);
    } else {
        unlinkat(dirfd, name, 0);
    }
}

void torvik_fs_remove(const char *path) {
    /* Split the top-level path into parent + final component so the whole walk
       runs through the fd-relative helper. Operate on a private copy. */
    struct stat st;
    if (lstat(path, &st) != 0) return;

    size_t n = strlen(path);
    char tmp[4096];
    if (n == 0 || n >= sizeof(tmp)) return;
    memcpy(tmp, path, n + 1);
    /* strip a trailing slash (but keep a lone "/") */
    while (n > 1 && tmp[n-1] == '/') { tmp[--n] = '\0'; }

    /* find the final path component */
    char *slash = strrchr(tmp, '/');
    const char *parent; const char *base;
    if (slash == tmp) { parent = "/"; base = slash + 1; }   /* "/foo" */
    else if (slash)   { *slash = '\0'; parent = tmp; base = slash + 1; }
    else              { parent = "."; base = tmp; }         /* "foo" */

    if (*base == '\0') return;   /* path was "/" or empty after trimming */

    int pfd = open(parent, O_RDONLY | O_DIRECTORY);
    if (pfd < 0) return;
    torvik_fs_remove_at(pfd, base);
    close(pfd);
}
#endif

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

// Append a line to a file (with newline). Arg order matches writefile: (path, data).
void torvik_appendline(const char *path, const char *data) {
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

/* byte_str — a byte code as a fresh 1-char (headered) string. Non-printable
   values are kept verbatim (callers decide); negative/EOF yields "".
   v1.1.3: backs readkey()'s documented 1-char-string return. */
char *torvik_byte_str(int64_t b) {
    if (b < 0 || b > 255) return torvik_str_alloc(0);
    char *buf = torvik_str_alloc(1);
    buf[0] = (char)(unsigned char)b;
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
    /* v1.1.3: %lld, not %ld - `long` is 32-bit on Win64 (LLP64), so large i64
       values printed truncated on Windows (a silent wrong answer). */
    snprintf(buf, 32, "%lld", (long long)n);
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
            snprintf(nb, sizeof(nb), "%lld", (long long)(int64_t)(intptr_t)l->data[i]); /* v1.1.3: LLP64-safe */
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


/* ── 10. Concurrency — raven tasks ────────────────────────────────────────────
   Phase A of the v1.3.0 concurrency model (see docs/GUIDE.md "Ravens & Bridges").

   Design invariants (do not weaken):
   - No object is ever shared between threads. Arguments are DEEP-COPIED at
     spawn (tv_spawn_copy); the return value's +1-on-return claim transfers
     wholesale to the joiner. The non-atomic ARC therefore stays correct with
     zero atomic operations.
   - The pack owns its copies exactly like a caller owns statement temps: copy
     is retained to rc1 at spawn_arg; the callee entry-retains/exit-releases as
     usual; the trampoline drops the pack's claim after the call returns.
   - A panic on any thread is torvik_panic -> exit(1): the whole process halts,
     matching main-thread behavior. Background failures never vanish.
   - Task structs live in a registry (reachable from a global) for the life of
     the process: LeakSanitizer sees them as reachable, double-join is a clean
     panic instead of a use-after-free, and no atexit teardown races a still-
     running detached thread.

   Slot kinds (must match codegen's spawn emission):
     0 = raw scalar bits (all integer widths, bool, f64 bit-packed, aett)
     1 = str  (torvik heap string; copied with torvik_str_dup)
     2 = i128/u128 heap box (copied via load + re-box)                        */

#define TORVIK_TASK_MAX_ARGS 16
/* Defined in Section 11 (bridges); used by the spawn-kind helpers below. */
void *torvik_bridge_retain(void *p);
void torvik_bridge_release(void *p);

#define TV_SPAWN_RAW    0
#define TV_SPAWN_STR    1
#define TV_SPAWN_I128   2
#define TV_SPAWN_BRIDGE 3   /* retained, not copied - the one shared object */

typedef struct TorvikTask TorvikTask;
struct TorvikTask {
#if defined(_WIN32)
    HANDLE th;
#else
    pthread_t th;
#endif
    void *ret;
    int64_t ret_kind;
    int64_t joined;
    TorvikTask *next;     /* task registry chain */
};

typedef struct {
    void *fn;
    int64_t nargs;
    int64_t ret_kind;
    TorvikTask *task;     /* NULL for a detached (fire-and-forget) spawn */
    void *args[TORVIK_TASK_MAX_ARGS];
    int64_t kinds[TORVIK_TASK_MAX_ARGS];
} TorvikSpawnPack;

/* Registry: keeps every handled task reachable for the process lifetime.
   Guarded because nested ravens may register from task threads.             */
static TorvikTask *tv_task_registry = NULL;
#if defined(_WIN32)
static SRWLOCK tv_task_reg_mu = SRWLOCK_INIT;
static void tv_task_reg_lock(void)   { AcquireSRWLockExclusive(&tv_task_reg_mu); }
static void tv_task_reg_unlock(void) { ReleaseSRWLockExclusive(&tv_task_reg_mu); }
#else
static pthread_mutex_t tv_task_reg_mu = PTHREAD_MUTEX_INITIALIZER;
static void tv_task_reg_lock(void)   { pthread_mutex_lock(&tv_task_reg_mu); }
static void tv_task_reg_unlock(void) { pthread_mutex_unlock(&tv_task_reg_mu); }
#endif

static void *tv_spawn_copy(void *slot, int64_t kind) {
    if (kind == TV_SPAWN_STR) {
        if (!slot) return NULL;
        char *c = torvik_str_dup((const char *)slot);
        return torvik_str_retain(c);            /* the pack's claim (rc1) */
    }
    if (kind == TV_SPAWN_I128) {
        if (!slot) return NULL;
        __int128 v;
        torvik_i128_load(slot, &v);
        return torvik_i128_retain(torvik_i128_box(&v));
    }
    if (kind == TV_SPAWN_BRIDGE) {
        return torvik_bridge_retain(slot);       /* shared, never copied */
    }
    return slot;                                 /* raw bits, no ownership */
}

static void tv_spawn_release(void *slot, int64_t kind) {
    if (kind == TV_SPAWN_STR)         torvik_str_release(slot);
    else if (kind == TV_SPAWN_I128)   torvik_i128_release(slot);
    else if (kind == TV_SPAWN_BRIDGE) torvik_bridge_release(slot);
}

/* Call a compiled Torvik function through the uniform ptr(ptr,...) convention.
   i64/ptr share argument registers on both target ABIs (x86-64 SysV, Win64),
   which is the same convention torvc's call sites already rely on.          */
typedef void *(*TvF0)(void);
typedef void *(*TvF1)(void *);
typedef void *(*TvF2)(void *, void *);
typedef void *(*TvF3)(void *, void *, void *);
typedef void *(*TvF4)(void *, void *, void *, void *);
typedef void *(*TvF5)(void *, void *, void *, void *, void *);
typedef void *(*TvF6)(void *, void *, void *, void *, void *, void *);
typedef void *(*TvF7)(void *, void *, void *, void *, void *, void *, void *);
typedef void *(*TvF8)(void *, void *, void *, void *, void *, void *, void *, void *);
typedef void *(*TvF9)(void *, void *, void *, void *, void *, void *, void *, void *, void *);
typedef void *(*TvF10)(void *, void *, void *, void *, void *, void *, void *, void *, void *, void *);
typedef void *(*TvF11)(void *, void *, void *, void *, void *, void *, void *, void *, void *, void *, void *);
typedef void *(*TvF12)(void *, void *, void *, void *, void *, void *, void *, void *, void *, void *, void *, void *);
typedef void *(*TvF13)(void *, void *, void *, void *, void *, void *, void *, void *, void *, void *, void *, void *, void *);
typedef void *(*TvF14)(void *, void *, void *, void *, void *, void *, void *, void *, void *, void *, void *, void *, void *, void *);
typedef void *(*TvF15)(void *, void *, void *, void *, void *, void *, void *, void *, void *, void *, void *, void *, void *, void *, void *);
typedef void *(*TvF16)(void *, void *, void *, void *, void *, void *, void *, void *, void *, void *, void *, void *, void *, void *, void *, void *);

static void *tv_task_call(void *fn, int64_t n, void **a) {
    switch (n) {
        case 0:  return ((TvF0)fn)();
        case 1:  return ((TvF1)fn)(a[0]);
        case 2:  return ((TvF2)fn)(a[0], a[1]);
        case 3:  return ((TvF3)fn)(a[0], a[1], a[2]);
        case 4:  return ((TvF4)fn)(a[0], a[1], a[2], a[3]);
        case 5:  return ((TvF5)fn)(a[0], a[1], a[2], a[3], a[4]);
        case 6:  return ((TvF6)fn)(a[0], a[1], a[2], a[3], a[4], a[5]);
        case 7:  return ((TvF7)fn)(a[0], a[1], a[2], a[3], a[4], a[5], a[6]);
        case 8:  return ((TvF8)fn)(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7]);
        case 9:  return ((TvF9)fn)(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8]);
        case 10: return ((TvF10)fn)(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9]);
        case 11: return ((TvF11)fn)(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9], a[10]);
        case 12: return ((TvF12)fn)(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9], a[10], a[11]);
        case 13: return ((TvF13)fn)(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9], a[10], a[11], a[12]);
        case 14: return ((TvF14)fn)(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9], a[10], a[11], a[12], a[13]);
        case 15: return ((TvF15)fn)(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9], a[10], a[11], a[12], a[13], a[14]);
        case 16: return ((TvF16)fn)(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9], a[10], a[11], a[12], a[13], a[14], a[15]);
        default: torvik_panic("raven: internal error - unsupported argument count"); return NULL;
    }
}

/* Thread entry. Runs the spawned function, settles ownership, frees the pack. */
#if defined(_WIN32)
static unsigned __stdcall tv_task_tramp(void *arg) {
#else
static void *tv_task_tramp(void *arg) {
#endif
    TorvikSpawnPack *p = (TorvikSpawnPack *)arg;
    void *ret = tv_task_call(p->fn, p->nargs, p->args);
    /* Drop the pack's claim on each copied argument (the callee's entry
       retain / exit release balanced its own use, exactly as with a normal
       caller's statement temps). */
    for (int64_t i = 0; i < p->nargs; i++) tv_spawn_release(p->args[i], p->kinds[i]);
    if (p->task) {
        p->task->ret = ret;                     /* owned: the callee's +1-on-return */
    } else if (p->ret_kind != TV_SPAWN_RAW) {
        tv_spawn_release(ret, p->ret_kind);     /* detached: drop the unread result */
    }
    free(p);
#if defined(_WIN32)
    return 0;
#else
    return NULL;
#endif
}

/* --- builtins ------------------------------------------------------------ */

void *torvik_spawn_begin(int64_t nargs) {
    if (nargs < 0 || nargs > TORVIK_TASK_MAX_ARGS)
        torvik_panic("raven: a spawned function may take at most 16 arguments");
    TorvikSpawnPack *p = (TorvikSpawnPack *)tv_malloc(sizeof(TorvikSpawnPack));
    memset(p, 0, sizeof(*p));
    p->nargs = nargs;
    return p;
}

void torvik_spawn_arg(void *pack, int64_t idx, void *slot, int64_t kind) {
    TorvikSpawnPack *p = (TorvikSpawnPack *)pack;
    if (idx < 0 || idx >= p->nargs)
        torvik_panic("raven: internal error - spawn argument index out of range");
    p->args[idx]  = tv_spawn_copy(slot, kind);
    p->kinds[idx] = kind;
}

void *torvik_raven_spawn(void *pack, void *fn, int64_t ret_kind, int64_t wants_handle) {
    TorvikSpawnPack *p = (TorvikSpawnPack *)pack;
    p->fn = fn;
    p->ret_kind = ret_kind;
    TorvikTask *t = NULL;
    if (wants_handle) {
        t = (TorvikTask *)tv_malloc(sizeof(TorvikTask));
        memset(t, 0, sizeof(*t));
        t->ret_kind = ret_kind;
        p->task = t;
        tv_task_reg_lock();
        t->next = tv_task_registry;
        tv_task_registry = t;
        tv_task_reg_unlock();
    }
#if defined(_WIN32)
    uintptr_t h = _beginthreadex(NULL, 0, tv_task_tramp, p, 0, NULL);
    if (h == 0) torvik_panic("raven: the operating system could not create a task thread");
    if (t) t->th = (HANDLE)h;
    else CloseHandle((HANDLE)h);                 /* detached */
#else
    pthread_t th;
    pthread_attr_t at;
    pthread_attr_init(&at);
    if (!t) pthread_attr_setdetachstate(&at, PTHREAD_CREATE_DETACHED);
    int rc = pthread_create(&th, &at, tv_task_tramp, p);
    pthread_attr_destroy(&at);
    if (rc != 0) torvik_panic("raven: the operating system could not create a task thread");
    if (t) t->th = th;
#endif
    return t;
}

/* Wait for a task and take its return value. The join_str / join_i128 names
   exist so codegen's line-based temp classifier can tag the result's kind
   ("b:str" / "b:i128" — the value arrives owned, carrying the task function's
   +1-on-return claim). All three share one implementation.                  */
static void *tv_task_join_impl(void *task) {
    TorvikTask *t = (TorvikTask *)task;
    if (!t) torvik_panic("join: this handle is not a running task");
    if (t->joined) torvik_panic("join: this task was already joined - a task's result can be taken once. Bind it to a variable at the first join if it is needed twice");
#if defined(_WIN32)
    WaitForSingleObject(t->th, INFINITE);
    CloseHandle(t->th);
#else
    pthread_join(t->th, NULL);
#endif
    t->joined = 1;
    return t->ret;
}

void *torvik_task_join(void *task)      { return tv_task_join_impl(task); }
void *torvik_task_join_str(void *task)  { return tv_task_join_impl(task); }
void *torvik_task_join_i128(void *task) { return tv_task_join_impl(task); }

/* Statement-form `join(h);` — wait, then drop a managed result so nothing
   leaks when the value is deliberately discarded.                           */
void torvik_task_join_drop(void *task) {
    TorvikTask *t = (TorvikTask *)task;
    void *ret = tv_task_join_impl(task);
    if (t->ret_kind != TV_SPAWN_RAW) tv_spawn_release(ret, t->ret_kind);
}

/* ── 11. Concurrency — bridges ────────────────────────────────────────────────
   Phase B of the v1.3.0 concurrency model. A bridge is a typed, buffered,
   thread-safe queue: the ONLY object two threads ever share, with every touch
   of its interior behind its own lock.

   Ownership invariants (mirror the raven task rules):
   - Values DEEP-COPY on send, on the sender's thread, before the enqueue.
     After a send, sender and receiver share no object.
   - The queue holds one claim (rc1) on each managed element. recv hands that
     claim to the receiver wholesale — the value "arrives owned", so codegen
     classifies recv results class b, exactly like join results.
   - The bridge's OWN refcount is the single atomic in the entire design: a
     task's exit releases its retained bridge argument from another thread.
     Element refcounts stay non-atomic — only one thread ever holds a given
     element.
   - Destruction (last release) drains remaining managed elements, tears down
     the primitives, and frees.
   - close() is idempotent; it wakes every blocked sender and receiver.
     send on a closed bridge panics. recv on a closed AND drained bridge
     panics (that is a programming error); try_recv turns exactly that one
     condition into err(0, "bridge closed") - the worker-loop primitive.    */

#define TORVIK_BRIDGE_MAGIC 0x5456425247ll   /* "TVBRG" */

typedef struct {
    int64_t magic;
    volatile int64_t refcount;   /* atomic: crosses threads by design */
    int64_t elem_kind;           /* TV_SPAWN_RAW / _STR / _I128 (fixed per bridge) */
    int64_t cap;
    int64_t count;
    int64_t head;
    int64_t closed;
    void **slots;
#if defined(_WIN32)
    CRITICAL_SECTION mu;
    CONDITION_VARIABLE not_full;
    CONDITION_VARIABLE not_empty;
#else
    pthread_mutex_t mu;
    pthread_cond_t not_full;
    pthread_cond_t not_empty;
#endif
} TorvikBridge;

static void tv_bridge_lock(TorvikBridge *b) {
#if defined(_WIN32)
    EnterCriticalSection(&b->mu);
#else
    pthread_mutex_lock(&b->mu);
#endif
}
static void tv_bridge_unlock(TorvikBridge *b) {
#if defined(_WIN32)
    LeaveCriticalSection(&b->mu);
#else
    pthread_mutex_unlock(&b->mu);
#endif
}
static void tv_bridge_wait_not_full(TorvikBridge *b) {
#if defined(_WIN32)
    SleepConditionVariableCS(&b->not_full, &b->mu, INFINITE);
#else
    pthread_cond_wait(&b->not_full, &b->mu);
#endif
}
static void tv_bridge_wait_not_empty(TorvikBridge *b) {
#if defined(_WIN32)
    SleepConditionVariableCS(&b->not_empty, &b->mu, INFINITE);
#else
    pthread_cond_wait(&b->not_empty, &b->mu);
#endif
}
static void tv_bridge_wake_all(TorvikBridge *b) {
#if defined(_WIN32)
    WakeAllConditionVariable(&b->not_full);
    WakeAllConditionVariable(&b->not_empty);
#else
    pthread_cond_broadcast(&b->not_full);
    pthread_cond_broadcast(&b->not_empty);
#endif
}

void *torvik_bridge_new(int64_t cap, int64_t elem_kind) {
    if (cap < 1) torvik_panic("bridge_new: capacity must be at least 1");
    TorvikBridge *b = (TorvikBridge *)tv_malloc(sizeof(TorvikBridge));
    memset(b, 0, sizeof(*b));
    b->magic = TORVIK_BRIDGE_MAGIC;
    b->refcount = 0;             /* floating; codegen retains at production */
    b->elem_kind = elem_kind;
    b->cap = cap;
    b->slots = (void **)tv_malloc(sizeof(void *) * (size_t)cap);
#if defined(_WIN32)
    InitializeCriticalSection(&b->mu);
    InitializeConditionVariable(&b->not_full);
    InitializeConditionVariable(&b->not_empty);
#else
    pthread_mutex_init(&b->mu, NULL);
    pthread_cond_init(&b->not_full, NULL);
    pthread_cond_init(&b->not_empty, NULL);
#endif
    return b;
}

void *torvik_bridge_retain(void *p) {
    if (!p) return p;
    TorvikBridge *b = (TorvikBridge *)p;
    if (b->magic != TORVIK_BRIDGE_MAGIC) return p;
    __atomic_add_fetch(&b->refcount, 1, __ATOMIC_SEQ_CST);
    return p;
}

void torvik_bridge_release(void *p) {
    if (!p) return;
    TorvikBridge *b = (TorvikBridge *)p;
    if (b->magic != TORVIK_BRIDGE_MAGIC) return;
    if (__atomic_sub_fetch(&b->refcount, 1, __ATOMIC_SEQ_CST) > 0) return;
    /* Last claim gone: no thread can reach the bridge any more, so plain
       (unlocked) teardown is safe. Drain the queue's claims first. */
    for (int64_t i = 0; i < b->count; i++) {
        tv_spawn_release(b->slots[(b->head + i) % b->cap], b->elem_kind);
    }
#if defined(_WIN32)
    DeleteCriticalSection(&b->mu);
#else
    pthread_mutex_destroy(&b->mu);
    pthread_cond_destroy(&b->not_full);
    pthread_cond_destroy(&b->not_empty);
#endif
    b->magic = 0;
    free(b->slots);
    free(b);
}

static TorvikBridge *tv_bridge_check(void *p, const char *who) {
    TorvikBridge *b = (TorvikBridge *)p;
    if (!b || b->magic != TORVIK_BRIDGE_MAGIC) {
        torvik_panic(who);
    }
    return b;
}

void torvik_bridge_send(void *p, void *slot) {
    TorvikBridge *b = tv_bridge_check(p, "send: this value is not a bridge");
    /* Deep-copy on the SENDER's thread, before the lock: the copy touches
       only sender-owned objects. The copy carries the queue's claim (rc1). */
    void *copy = tv_spawn_copy(slot, b->elem_kind);
    tv_bridge_lock(b);
    while (b->count == b->cap && !b->closed) tv_bridge_wait_not_full(b);
    if (b->closed) {
        tv_bridge_unlock(b);
        tv_spawn_release(copy, b->elem_kind);
        torvik_panic("send: this bridge is closed - nothing can cross it any more");
    }
    b->slots[(b->head + b->count) % b->cap] = copy;
    b->count++;
#if defined(_WIN32)
    WakeConditionVariable(&b->not_empty);
#else
    pthread_cond_signal(&b->not_empty);
#endif
    tv_bridge_unlock(b);
}

/* Shared dequeue: blocks while empty and open. Returns 1 with the value in
   *out (ownership transfers to the caller), or 0 when closed AND drained.  */
static int tv_bridge_take(TorvikBridge *b, void **out) {
    tv_bridge_lock(b);
    while (b->count == 0 && !b->closed) tv_bridge_wait_not_empty(b);
    if (b->count == 0) {   /* closed and drained */
        tv_bridge_unlock(b);
        return 0;
    }
    *out = b->slots[b->head];
    b->head = (b->head + 1) % b->cap;
    b->count--;
#if defined(_WIN32)
    WakeConditionVariable(&b->not_full);
#else
    pthread_cond_signal(&b->not_full);
#endif
    tv_bridge_unlock(b);
    return 1;
}

/* recv variants: one implementation, three symbols so codegen's line-based
   temp classifier can tag ownership of the result ("b:str" / "b:i128"). */
static void *tv_bridge_recv_impl(void *p) {
    TorvikBridge *b = tv_bridge_check(p, "recv: this value is not a bridge");
    void *v = NULL;
    if (!tv_bridge_take(b, &v)) {
        torvik_panic("recv: this bridge is closed and drained - loop with try_recv to stop cleanly at end-of-stream");
    }
    return v;
}
void *torvik_bridge_recv(void *p)      { return tv_bridge_recv_impl(p); }
void *torvik_bridge_recv_str(void *p)  { return tv_bridge_recv_impl(p); }
void *torvik_bridge_recv_i128(void *p) { return tv_bridge_recv_impl(p); }

/* try_recv: blocks exactly like recv, but closed+drained becomes
   err(0, "bridge closed") instead of a panic - the worker-loop primitive.
   The ok value's queue claim transfers into the result box: for str elements
   the box is marked managed WITHOUT an extra retain (mark_managed would
   retain again, so the claim is handed over by construction instead).      */
void *torvik_bridge_try_recv(void *p) {
    TorvikBridge *b = tv_bridge_check(p, "try_recv: this value is not a bridge");
    void *v = NULL;
    if (!tv_bridge_take(b, &v)) {
        char *m = torvik_str_dup("bridge closed");
        void *r = torvik_result_err(0, m);   /* err retains m (rc1) */
        return r;
    }
    TorvikResult *r = torvik_result_ok(v);
    if (b->elem_kind == TV_SPAWN_STR) {
        r->managed = 1;   /* box adopts the queue's claim on v (already rc1) */
    }
    return r;
}

void torvik_bridge_close(void *p) {
    TorvikBridge *b = tv_bridge_check(p, "bridge_close: this value is not a bridge");
    tv_bridge_lock(b);
    b->closed = 1;       /* idempotent */
    tv_bridge_wake_all(b);
    tv_bridge_unlock(b);
}

/* ── 12. Networking — std::net backing (TCP + minimal HTTP transport) ──────────
   A small, blocking TCP layer that lets Torvik programs run a local HTTP server
   without shelling out. The protocol (HTTP request parsing, responses, MIME) is
   written in Torvik in std/net.tv; this file provides only the transport:
   listen / accept / recv / send / send_file / close, plus lazy Winsock init.

   Sockets bind to 127.0.0.1 only — a preview/dev server should never be exposed
   to the network by accident. File descriptors cross into Torvik as plain i64
   (an int fd on POSIX, a cast SOCKET on Windows). */

#if defined(_WIN32)
  typedef SOCKET tv_sock_t;
  #define TV_BAD_SOCK   INVALID_SOCKET
  #define TV_SENDFLAGS  0

  /* Winsock is resolved at RUNTIME (LoadLibrary + GetProcAddress) rather than
     linked against ws2_32 at build time. The runtime object therefore carries no
     Winsock imports and links no matter what flags the invoking compiler passes -
     including an OLDER torvc that predates the -lws2_32 link flag. That is exactly
     the situation when bootstrapping a new compiler on Windows from a previous
     release, and it keeps every future runtime addition from breaking that path.
     Only kernel32 (LoadLibrary/GetProcAddress) is needed, and it is always linked. */
  typedef SOCKET (WSAAPI *tv_pfn_socket)(int, int, int);
  typedef int    (WSAAPI *tv_pfn_bind)(SOCKET, const struct sockaddr *, int);
  typedef int    (WSAAPI *tv_pfn_listen)(SOCKET, int);
  typedef SOCKET (WSAAPI *tv_pfn_accept)(SOCKET, struct sockaddr *, int *);
  typedef int    (WSAAPI *tv_pfn_recv)(SOCKET, char *, int, int);
  typedef int    (WSAAPI *tv_pfn_send)(SOCKET, const char *, int, int);
  typedef int    (WSAAPI *tv_pfn_closesocket)(SOCKET);
  typedef int    (WSAAPI *tv_pfn_setsockopt)(SOCKET, int, int, const char *, int);
  typedef int    (WSAAPI *tv_pfn_wsastartup)(WORD, LPWSADATA);
  typedef int    (WSAAPI *tv_pfn_wsalasterr)(void);

  static tv_pfn_socket      tv_p_socket      = 0;
  static tv_pfn_bind        tv_p_bind        = 0;
  static tv_pfn_listen      tv_p_listen      = 0;
  static tv_pfn_accept      tv_p_accept      = 0;
  static tv_pfn_recv        tv_p_recv        = 0;
  static tv_pfn_send        tv_p_send        = 0;
  static tv_pfn_closesocket tv_p_closesocket = 0;
  static tv_pfn_setsockopt  tv_p_setsockopt  = 0;
  static tv_pfn_wsastartup  tv_p_wsastartup  = 0;
  static tv_pfn_wsalasterr  tv_p_wsalasterr  = 0;

  static int tv_wsa_started = 0;
  static void tv_wsa_init(void) {
      if (tv_wsa_started) return;
      tv_wsa_started = 1;               /* attempt once, succeed or not */
      {
          HMODULE h = LoadLibraryA("ws2_32.dll");
          if (!h) return;
          tv_p_socket      = (tv_pfn_socket)(void *)GetProcAddress(h, "socket");
          tv_p_bind        = (tv_pfn_bind)(void *)GetProcAddress(h, "bind");
          tv_p_listen      = (tv_pfn_listen)(void *)GetProcAddress(h, "listen");
          tv_p_accept      = (tv_pfn_accept)(void *)GetProcAddress(h, "accept");
          tv_p_recv        = (tv_pfn_recv)(void *)GetProcAddress(h, "recv");
          tv_p_send        = (tv_pfn_send)(void *)GetProcAddress(h, "send");
          tv_p_closesocket = (tv_pfn_closesocket)(void *)GetProcAddress(h, "closesocket");
          tv_p_setsockopt  = (tv_pfn_setsockopt)(void *)GetProcAddress(h, "setsockopt");
          tv_p_wsastartup  = (tv_pfn_wsastartup)(void *)GetProcAddress(h, "WSAStartup");
          tv_p_wsalasterr  = (tv_pfn_wsalasterr)(void *)GetProcAddress(h, "WSAGetLastError");
          if (tv_p_wsastartup) { WSADATA w; tv_p_wsastartup(MAKEWORD(2, 2), &w); }
      }
  }

  /* Null-safe shims: if ws2_32 could not be loaded, every call fails cleanly
     instead of jumping through a null pointer. */
  static SOCKET tv_ws_socket(int a, int b, int c) { tv_wsa_init(); return tv_p_socket ? tv_p_socket(a, b, c) : INVALID_SOCKET; }
  static int tv_ws_bind(SOCKET s, const struct sockaddr *a, int l) { return tv_p_bind ? tv_p_bind(s, a, l) : SOCKET_ERROR; }
  static int tv_ws_listen(SOCKET s, int b) { return tv_p_listen ? tv_p_listen(s, b) : SOCKET_ERROR; }
  static SOCKET tv_ws_accept(SOCKET s, struct sockaddr *a, int *l) { return tv_p_accept ? tv_p_accept(s, a, l) : INVALID_SOCKET; }
  static int tv_ws_recv(SOCKET s, char *b, int n, int f) { return tv_p_recv ? tv_p_recv(s, b, n, f) : SOCKET_ERROR; }
  static int tv_ws_send(SOCKET s, const char *b, int n, int f) { return tv_p_send ? tv_p_send(s, b, n, f) : SOCKET_ERROR; }
  static int tv_ws_closesocket(SOCKET s) { return tv_p_closesocket ? tv_p_closesocket(s) : 0; }
  static int tv_ws_setsockopt(SOCKET s, int lv, int nm, const char *v, int l) { return tv_p_setsockopt ? tv_p_setsockopt(s, lv, nm, v, l) : SOCKET_ERROR; }
  /* Byte-order helpers are pure arithmetic - no need to import them at all. */
  static unsigned short tv_ws_htons(unsigned short v) { return (unsigned short)((v << 8) | (v >> 8)); }
  static unsigned long  tv_ws_htonl(unsigned long v) {
      return ((v & 0x000000FFUL) << 24) | ((v & 0x0000FF00UL) << 8) |
             ((v & 0x00FF0000UL) >> 8)  | ((v & 0xFF000000UL) >> 24);
  }

  #undef socket
  #undef bind
  #undef listen
  #undef accept
  #undef recv
  #undef send
  #undef closesocket
  #undef setsockopt
  #undef htons
  #undef htonl
  #define socket(a, b, c)          tv_ws_socket((a), (b), (c))
  #define bind(s, a, l)            tv_ws_bind((s), (a), (l))
  #define listen(s, b)             tv_ws_listen((s), (b))
  #define accept(s, a, l)          tv_ws_accept((s), (a), (l))
  #define recv(s, b, n, f)         tv_ws_recv((s), (b), (n), (f))
  #define send(s, b, n, f)         tv_ws_send((s), (b), (n), (f))
  #define closesocket(s)           tv_ws_closesocket((s))
  #define setsockopt(s, l, n, v, z) tv_ws_setsockopt((s), (l), (n), (v), (z))
  #define htons(x)                 tv_ws_htons((unsigned short)(x))
  #define htonl(x)                 tv_ws_htonl((unsigned long)(x))
  #define tv_closesock  closesocket

  static int tv_sock_errno(void) { return tv_p_wsalasterr ? tv_p_wsalasterr() : (int)GetLastError(); }
#else
  typedef int tv_sock_t;
  #define TV_BAD_SOCK   (-1)
  #define tv_closesock  close
  #ifdef MSG_NOSIGNAL
    #define TV_SENDFLAGS MSG_NOSIGNAL
  #else
    #define TV_SENDFLAGS 0
  #endif
  static void tv_wsa_init(void) {}
  static int tv_sock_errno(void) { return errno; }
#endif

/* net_listen(port) -> result<i64>. Bind + listen on 127.0.0.1:port; the ok value
   is the listening socket fd, err carries a human message (the common case being
   a port already in use). */
void *torvik_net_listen(int64_t port) {
    tv_wsa_init();
#if !defined(_WIN32)
    /* A client that hangs up mid-response must not kill the whole server with a
       SIGPIPE. Ignore it once here (idempotent); sends also pass MSG_NOSIGNAL. */
    signal(SIGPIPE, SIG_IGN);
#endif
    if (port < 1 || port > 65535) {
        return torvik_result_err(1, torvik_str_dup("net_listen: port must be between 1 and 65535"));
    }
    tv_sock_t s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == TV_BAD_SOCK) {
        return torvik_result_err((int64_t)tv_sock_errno(), torvik_str_dup("net_listen: could not create a socket"));
    }
    int yes = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((unsigned short)port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);   /* 127.0.0.1 only */

    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        char msg[96];
        snprintf(msg, sizeof(msg), "net_listen: could not bind to port %lld (is it already in use?)",
                 (long long)port);
        tv_closesock(s);
        return torvik_result_err((int64_t)tv_sock_errno(), torvik_str_dup(msg));
    }
    if (listen(s, 16) != 0) {
        tv_closesock(s);
        return torvik_result_err((int64_t)tv_sock_errno(), torvik_str_dup("net_listen: could not listen on the socket"));
    }
    return torvik_result_ok((void *)(intptr_t)s);
}

/* net_accept(server_fd) -> i64. Block for the next client; return its fd, or -1
   on error. */
int64_t torvik_net_accept(int64_t server_fd) {
    tv_sock_t s = (tv_sock_t)(intptr_t)server_fd;
    tv_sock_t c = accept(s, NULL, NULL);
    if (c == TV_BAD_SOCK) return -1;
    return (int64_t)(intptr_t)c;
}

/* True once the buffer contains the end-of-headers marker CRLF-CRLF. Scans the
   raw bytes (not strstr) so an embedded NUL can't cut the search short. */
static int tv_has_header_end(const char *buf, size_t n) {
    if (n < 4) return 0;
    for (size_t i = 0; i + 3 < n; i++) {
        if (buf[i] == '\r' && buf[i+1] == '\n' && buf[i+2] == '\r' && buf[i+3] == '\n')
            return 1;
    }
    return 0;
}

/* net_recv(client_fd) -> str. Read the request (up to 64 KB) into a Torvik
   string, stopping once the header terminator CRLF-CRLF is seen or the peer
   stops sending. Text-oriented: HTTP request lines and headers are ASCII, which
   is all a static server needs to route on. Returns "" on immediate error/EOF. */
char *torvik_net_recv(int64_t client_fd) {
    tv_sock_t s = (tv_sock_t)(intptr_t)client_fd;
    size_t cap = 64 * 1024;
    char *buf = torvik_str_alloc(cap);
    size_t total = 0;
    for (;;) {
        if (total >= cap) break;
        int n = (int)recv(s, buf + total, (int)(cap - total), 0);
        if (n <= 0) break;
        total += (size_t)n;
        buf[total] = '\0';
        /* Stop as soon as the full header block has arrived. */
        if (tv_has_header_end(buf, total)) break;
    }
    buf[total] = '\0';
    return buf;
}

/* net_send(client_fd, data) -> i64. Send the whole NUL-terminated string,
   looping over partial writes; return bytes sent, or -1 on error. Suitable for
   response headers and text bodies (HTML/CSS/JS/SVG/JSON). */
int64_t torvik_net_send(int64_t client_fd, const char *data) {
    tv_sock_t s = (tv_sock_t)(intptr_t)client_fd;
    size_t len = strlen(data);
    size_t sent = 0;
    while (sent < len) {
        int n = (int)send(s, data + sent, (int)(len - sent), TV_SENDFLAGS);
        if (n <= 0) return -1;
        sent += (size_t)n;
    }
    return (int64_t)sent;
}

/* net_send_file(client_fd, path) -> i64. Stream a file's raw bytes to the socket,
   binary-safe (no strlen), for images and other assets a text string can't carry.
   Returns bytes sent, or -1 if the file can't be opened or a write fails. */
int64_t torvik_net_send_file(int64_t client_fd, const char *path) {
    tv_sock_t s = (tv_sock_t)(intptr_t)client_fd;
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    char chunk[64 * 1024];
    int64_t total = 0;
    for (;;) {
        size_t got = fread(chunk, 1, sizeof(chunk), f);
        if (got == 0) break;
        size_t sent = 0;
        while (sent < got) {
            int n = (int)send(s, chunk + sent, (int)(got - sent), TV_SENDFLAGS);
            if (n <= 0) { fclose(f); return -1; }
            sent += (size_t)n;
        }
        total += (int64_t)got;
    }
    fclose(f);
    return total;
}

/* net_close(fd) — close a listening or client socket. */
void torvik_net_close(int64_t fd) {
    tv_sock_t s = (tv_sock_t)(intptr_t)fd;
    tv_closesock(s);
}
