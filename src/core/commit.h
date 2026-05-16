#ifndef COMMIT_H
#define COMMIT_H

#include "tree.h"
#include <time.h>
#include "types.h"

// Структура коммита (персистентная)

// Базовые операции
Commit* init_repo(void);
Commit* commit(Commit* current_state, const char* message);
Commit* add_file(Commit* current_commit, const char* path, const char* content);
Commit* remove_file(Commit* current_commit, const char* path);

// Получение данных
char* get_file_content(Commit* commit, const char* path);
int file_exists(Commit* commit, const char* path);

// Вывод
void print_commit(Commit* commit);
void print_history(Commit* commit);
void print_files(Commit* commit);

// Ветвление
void create_branch(Commit* commit, const char* branch_name);
Commit* get_branch_head(const char* branch_name);
Commit* checkout(Commit* commit);

// Анализ
int count_objects(Commit* commit);
void show_memory_saving(Commit* old_commit, Commit* new_commit);

// Управление памятью
void free_commit(Commit* commit);
void free_branches(void);

#endif