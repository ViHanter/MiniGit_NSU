#ifndef REPO_H
#define REPO_H

#include "types.h"
#include "commit.h"
#include "tree.h"

// ================ ОСНОВНЫЕ ФУНКЦИИ ================

// Создаёт структуру репозитория (не путать с init_repo из ТЗ!)
Repository *repo_create(void);

// Освобождает всю память репозитория
void repo_destroy(Repository *repo);

// ================ РАБОТА С КОММИТАМИ ================

void repo_add_commit(Repository *repo, Commit *commit);
Commit *repo_find_commit_by_id(Repository *repo, int id);
void repo_set_head(Repository *repo, Commit *commit);
Commit *repo_get_head(Repository *repo);

// ================ РАБОТА С ВЕТКАМИ ================

void repo_create_branch(Repository *repo, const char *branch_name, Commit *commit);
void repo_delete_branch(Repository *repo, const char *branch_name);
void repo_checkout_branch(Repository *repo, const char *branch_name);
Commit *repo_get_branch_head(Repository *repo, const char *branch_name);
int repo_branch_exists(Repository *repo, const char *branch_name);
void repo_update_branch(Repository *repo, const char *branch_name, Commit *commit);
char **repo_list_branches(Repository *repo, int *count);

// ================ ВСПОМОГАТЕЛЬНЫЕ ================

void repo_print_status(Repository *repo);

#endif