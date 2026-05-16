#ifndef QUERY_H
#define QUERY_H

#include "../core/types.h"
#include "../core/tree.h"

// ================ ПО ТРЕБОВАНИЯМ ТЗ ================

// get_file_content: принимает коммит и путь к файлу.
// Возвращает содержимое файла в этой версии (строку) или NULL, если файла нет.
// ВНИМАНИЕ: возвращает копию строки, которую нужно освободить через free()!
char *get_file_content(Commit *commit, const char *path);

// get_file_exists: принимает коммит и путь.
// Возвращает 1, если файл есть в этой версии, иначе 0.
int get_file_exists(Commit *commit, const char *path);

// ================ ДОПОЛНИТЕЛЬНЫЕ УДОБНЫЕ ФУНКЦИИ ================

// Возвращает размер файла в байтах (0 если нет)
size_t get_file_size(Commit *commit, const char *path);

// Возвращает хеш файла (нужно освободить)
char *get_file_hash(Commit *commit, const char *path);

// Сохраняет содержимое файла в файловую систему (по настоящему пути)
int extract_file_to_disk(Commit *commit, const char *path, const char *output_path);

#endif