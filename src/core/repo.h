#ifndef REPO_H
#define REPO_H

#include "types.h"
#include "commit.h"
#include "tree.h"

// Создание и уничтожение
Repository *repo_create(void);
void repo_destroy(Repository *repo);

// Работа с коммитами
void repo_add_commit(Repository *repo, Commit *commit);
Commit *repo_find_commit_by_id(Repository *repo, int id);
void repo_set_head(Repository *repo, Commit *commit);
Commit *repo_get_head(Repository *repo);

// Работа с ветками
void repo_create_branch(Repository *repo, const char *branch_name, Commit *commit);
void repo_delete_branch(Repository *repo, const char *branch_name);
void repo_checkout_branch(Repository *repo, const char *branch_name);
Commit *repo_get_branch_head(Repository *repo, const char *branch_name);
int repo_branch_exists(Repository *repo, const char *branch_name);
void repo_update_branch(Repository *repo, const char *branch_name, Commit *commit);
char **repo_list_branches(Repository *repo, int *count);
Commit *merge_simple(Repository *repo, Commit *base, Commit *other, const char *message);

// Вспомогательные
void repo_print_status(Repository *repo);
void free_branches(void);  // <-- ДОБАВИТЬ ЭТУ СТРОКУ

#endif
