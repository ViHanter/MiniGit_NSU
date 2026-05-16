#ifndef UTILS_H
#define UTILS_H

#include <time.h>

// Хеширование — используем твою готовую SHA-1
// Просто объявляем, что она есть где-то
char *sha1_compute(const char *data, size_t len);  // твоя реализация

// Время — нужно для timestamp в коммитах
char *time_to_string(time_t t);
time_t string_to_time(const char *str);

// Вспомогательное — разбор пути "src/main.c" на части
char **split_path(const char *path, int *count);
void free_split_path(char **parts, int count);

#endif