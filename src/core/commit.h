#ifndef COMMIT_H
#define COMMIT_H

#include "types.h"
#include "tree.h"
#include <time.h>

typedef struct repository Repository;

// Базовые операции (без авто-коммита!)
Commit* init_repo(void);
Commit* create_commit(Commit *parent, TreeNode *root, const char *message,
                      char **changed_files, int changed_files_count);
Commit* add_file(Repository *repo, Commit *current_commit, const char *path, const char *content);
Commit* remove_file(Repository *repo, Commit *current_commit, const char *path);

// Получение данных
char* get_file_content(Commit *commit, const char *path);
int get_file_exists(Commit *commit, const char *path);
int file_exists(Commit *commit, const char *path);

// Вывод
void print_commit(Commit *commit);
void print_history(Commit *commit);
void print_files(Commit *commit);
int restore_commit_files(Commit *previous_commit, Commit *target_commit);

// Управление памятью
void free_commit(Commit *commit);
Commit* find_commit_by_id(Commit **all_commits, int count, int id);
int count_objects(Commit *commit, int *tree_count, int *blob_count);

void add_child_commit(Commit *parent, Commit *child);
void commit_note_loaded_id(int id);

#endif
