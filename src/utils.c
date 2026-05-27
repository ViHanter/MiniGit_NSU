#include "utils.h"
#include <stdlib.h>
#include "core/commit.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#include <wchar.h>
#else
#include <sys/stat.h>
#endif

static char *minigit_storage_base = NULL;

#ifdef _WIN32
static wchar_t *utf8_to_wide(const char *text) {
    if (!text) return NULL;

    int needed = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
    if (needed <= 0) return NULL;

    wchar_t *wide = (wchar_t*)malloc((size_t)needed * sizeof(wchar_t));
    if (!wide) return NULL;

    if (MultiByteToWideChar(CP_UTF8, 0, text, -1, wide, needed) <= 0) {
        free(wide);
        return NULL;
    }
    return wide;
}

static FILE *utf8_fopen(const char *path, const char *mode) {
    wchar_t *wide_path = utf8_to_wide(path);
    wchar_t *wide_mode = utf8_to_wide(mode);
    if (!wide_path || !wide_mode) {
        free(wide_path);
        free(wide_mode);
        return NULL;
    }

    FILE *file = _wfopen(wide_path, wide_mode);
    free(wide_path);
    free(wide_mode);
    return file;
}
#endif

// ================ ХЕШИРОВАНИЕ ================

static unsigned long simple_hash(const unsigned char *str, size_t len) {
    unsigned long hash = 5381;
    for (size_t i = 0; i < len; i++) {
        hash = ((hash << 5) + hash) + str[i];
    }
    return hash;
}

char *compute_hash(const char *data, size_t len) {
    if (!data) return NULL;
    unsigned long hash = simple_hash((const unsigned char*)data, len);
    char *hex = (char*)malloc(17);
    if (!hex) return NULL;
    sprintf(hex, "%016lx", hash);
    return hex;
}

char *compute_commit_hash(const Commit *commit) {
    if (!commit) return NULL;
    
    char buffer[2048];
    snprintf(buffer, sizeof(buffer), "%d|%s|%ld|%s",
             commit->id,
             commit->message ? commit->message : "",
             (long)commit->timestamp,
             commit->parent ? (commit->parent->hash ? commit->parent->hash : "none") : "none");
    
    return compute_hash(buffer, strlen(buffer));
}
// ================ РАЗБОР ПУТЕЙ ================

char **split_path(const char *path, int *parts_count) {
    if (!path || strlen(path) == 0) {
        *parts_count = 0;
        return NULL;
    }
    
    char *path_copy = strdup(path);
    if (!path_copy) {
        *parts_count = 0;
        return NULL;
    }
    
    char **parts = NULL;
    int count = 0;
    
    char *token = strtok(path_copy, "/");
    while (token) {
        char **new_parts = (char**)realloc(parts, (count + 1) * sizeof(char*));
        if (!new_parts) {
            free_split_path(parts, count);
            free(path_copy);
            *parts_count = 0;
            return NULL;
        }
        parts = new_parts;
        parts[count] = strdup(token);
        if (!parts[count]) {
            free_split_path(parts, count);
            free(path_copy);
            *parts_count = 0;
            return NULL;
        }
        count++;
        token = strtok(NULL, "/");
    }
    
    free(path_copy);
    *parts_count = count;
    return parts;
}

void free_split_path(char **parts, int count) {
    if (!parts) return;
    for (int i = 0; i < count; i++) {
        free(parts[i]);
    }
    free(parts);
}

// ================ ВРЕМЯ ================

char *time_to_string(time_t t) {
    char *buf = (char*)malloc(20);
    if (!buf) return NULL;
    strftime(buf, 20, "%Y-%m-%d %H:%M:%S", localtime(&t));
    return buf;
}

time_t string_to_time(const char *str) {
    struct tm tm = {0};
    sscanf(str, "%d-%d-%d %d:%d:%d", 
           &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
           &tm.tm_hour, &tm.tm_min, &tm.tm_sec);
    tm.tm_year -= 1900;
    tm.tm_mon -= 1;
    return mktime(&tm);
}

void normalize_path(char *path) {
    if (!path) return;
    for (char *p = path; *p; p++) {
        if (*p == '\\') *p = '/';
    }
}

static int make_dir(const char *path) {
    if (!path || strlen(path) == 0) return 1;
#ifdef _WIN32
    wchar_t *wide_path = utf8_to_wide(path);
    if (!wide_path) return 0;

    int ok = _wmkdir(wide_path) == 0 || errno == EEXIST;
    free(wide_path);
    if (ok) return 1;
#else
    if (mkdir(path, 0777) == 0 || errno == EEXIST) return 1;
#endif
    return 0;
}

static int ensure_parent_dirs(const char *path) {
    if (!path) return 0;

    char *copy = strdup(path);
    if (!copy) return 0;
    normalize_path(copy);

    for (char *p = copy; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (strlen(copy) > 0 && !make_dir(copy)) {
                free(copy);
                return 0;
            }
            *p = '/';
        }
    }

    free(copy);
    return 1;
}

char *read_text_file(const char *path, size_t *size) {
    if (size) *size = 0;
    if (!path) return NULL;

    FILE *file =
#ifdef _WIN32
        utf8_fopen(path, "rb");
#else
        fopen(path, "rb");
#endif
    if (!file) return NULL;

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }

    long len = ftell(file);
    if (len < 0) {
        fclose(file);
        return NULL;
    }
    rewind(file);

    char *buffer = (char*)malloc((size_t)len + 1);
    if (!buffer) {
        fclose(file);
        return NULL;
    }

    size_t read_count = fread(buffer, 1, (size_t)len, file);
    fclose(file);
    buffer[read_count] = '\0';
    if (size) *size = read_count;
    return buffer;
}

int write_text_file(const char *path, const char *content, size_t size) {
    if (!path || !content) return 0;
    if (!ensure_parent_dirs(path)) return 0;

    FILE *file =
#ifdef _WIN32
        utf8_fopen(path, "wb");
#else
        fopen(path, "wb");
#endif
    if (!file) return 0;

    int ok = fwrite(content, 1, size, file) == size;
    fclose(file);
    return ok;
}

int remove_text_file(const char *path) {
    if (!path) return 0;
#ifdef _WIN32
    wchar_t *wide_path = utf8_to_wide(path);
    if (!wide_path) return 0;

    int ok = _wremove(wide_path) == 0 || errno == ENOENT;
    free(wide_path);
    return ok;
#else
    return remove(path) == 0 || errno == ENOENT;
#endif
}

void set_minigit_storage_base(const char *path) {
    free(minigit_storage_base);
    minigit_storage_base = path && strlen(path) > 0 ? strdup(path) : NULL;
}

static void build_storage_path(char *buffer, size_t size, const char *suffix) {
    const char *base = minigit_storage_base ? minigit_storage_base : ".";
    snprintf(buffer, size, "%s/%s", base, suffix);
    normalize_path(buffer);
}

int ensure_minigit_storage(void) {
    char path[1024];

    build_storage_path(path, sizeof(path), ".minigit");
    if (!make_dir(path)) return 0;

    build_storage_path(path, sizeof(path), ".minigit/objects");
    if (!make_dir(path)) return 0;

    build_storage_path(path, sizeof(path), ".minigit/refs");
    if (!make_dir(path)) return 0;
    return 1;
}

int store_blob_object(const char *hash, const char *content, size_t size) {
    if (!hash || !content) return 0;
    if (!ensure_minigit_storage()) return 0;

    char suffix[512];
    char path[1024];
    snprintf(suffix, sizeof(suffix), ".minigit/objects/%s.blob", hash);
    build_storage_path(path, sizeof(path), suffix);
    return write_text_file(path, content, size);
}
