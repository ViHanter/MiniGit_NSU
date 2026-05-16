#include "utils.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Время
char *time_to_string(time_t t) {
    char *buf = malloc(20);
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

// Разбор пути (нужно для tree.c)
char **split_path(const char *path, int *count) {
    if (!path || strlen(path) == 0) {
        *count = 0;
        return NULL;
    }
    
    char *copy = strdup(path);
    char **parts = NULL;
    *count = 0;
    
    char *token = strtok(copy, "/");
    while (token) {
        parts = realloc(parts, (*count + 1) * sizeof(char*));
        parts[*count] = strdup(token);
        (*count)++;
        token = strtok(NULL, "/");
    }
    
    free(copy);
    return parts;
}

void free_split_path(char **parts, int count) {
    for (int i = 0; i < count; i++) free(parts[i]);
    free(parts);
}