#ifndef STAGING_H
#define STAGING_H

#include "types.h"
#include "tree.h"

// Создание/уничтожение
StagingArea *staging_create(Commit *base);
void staging_destroy(StagingArea *staging);

// Операции со staging
void staging_add_file(StagingArea *staging, const char *path, const char *content);
void staging_remove_file(StagingArea *staging, const char *path);
int staging_has_changes(const StagingArea *staging);

// Коммит (переносит staging в коммит)
Commit *staging_commit(Repository *repo, StagingArea *staging, const char *message);
Commit *commit(Repository *repo, StagingArea *staging, const char *message);

// Получение дерева (для отладки)
TreeNode *staging_get_tree(StagingArea *staging);

#endif
