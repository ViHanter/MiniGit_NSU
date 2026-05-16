#ifndef COMMIT_OP_H
#define COMMIT_OP_H

#include "../core/types.h"
#include "../core/tree.h"
#include "../core/commit.h"
#include "../core/repo.h"

// ================ STAGING AREA (промежуточное состояние) ================

// Структура, которая хранит изменения до коммита
typedef struct staging_area {
    TreeNode *root;              // текущее состояние staging (дерево файлов)
    Commit *base_commit;         // от какого коммита делаем изменения
    char **changed_files;        // список изменённых/добавленных/удалённых
    int changed_files_count;     // количество изменённых файлов
    int changed_files_capacity;  // выделенная память
} StagingArea;

// ================ ОСНОВНЫЕ ФУНКЦИИ ПО ТЗ ================

// Создаёт новый staging area на основе коммита
StagingArea *create_staging(Commit *base_commit);

// Освобождает staging area
void free_staging(StagingArea *staging);

// Добавляет файл в staging (не создаёт коммит!)
void staging_add_file(StagingArea *staging, const char *path, const char *content);

// Удаляет файл в staging
void staging_remove_file(StagingArea *staging, const char *path);

// Проверяет, есть ли изменения в staging
int staging_has_changes(const StagingArea *staging);

// ================ commit() ПО ТРЕБОВАНИЯМ ТЗ ================

// Принимает текущее состояние (staging) и сообщение коммита.
// Создаёт финальный коммит с вычисленным хешем и привязкой к родителю.
// Возвращает новый коммит.
Commit *commit(Repository *repo, StagingArea *staging, const char *message);

// ================ ВСПОМОГАТЕЛЬНЫЕ ================

// Получает текущее дерево из staging (для отладки)
TreeNode *get_staging_tree(const StagingArea *staging);

// Очищает staging после коммита (создаёт новый на основе base_commit)
void staging_clear(StagingArea *staging);

#endif