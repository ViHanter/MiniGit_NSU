#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>
#include <time.h>
#include "core/types.h"

// Пути
char **split_path(const char *path, int *count);
void free_split_path(char **parts, int count);

// Хеширование
char *compute_commit_hash(const Commit *commit);
char *compute_hash(const char *data, size_t len);

// Время
char *time_to_string(time_t t);
time_t string_to_time(const char *str);

// Работа с реальными файлами
char *read_text_file(const char *path, size_t *size);
int write_text_file(const char *path, const char *content, size_t size);
int remove_text_file(const char *path);
int ensure_minigit_storage(void);
int store_blob_object(const char *hash, const char *content, size_t size);
void set_minigit_storage_base(const char *path);
void normalize_path(char *path);

#endif
