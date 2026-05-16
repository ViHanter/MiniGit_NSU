#include "commit_op.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ================ ВНУТРЕННИЕ ФУНКЦИИ ================

static void ensure_changed_files_capacity(StagingArea *staging) {
    if (staging->changed_files_count >= staging->changed_files_capacity) {
        staging->changed_files_capacity = staging->changed_files_capacity == 0 ? 4 : staging->changed_files_capacity * 2;
        staging->changed_files = (char**)realloc(staging->changed_files,
                                                  staging->changed_files_capacity * sizeof(char*));
    }
}

// Добавляет путь в список изменённых файлов (если ещё не добавлен)
static void add_changed_file(StagingArea *staging, const char *path) {
    if (!staging || !path) return;
    
    // Проверяем, нет ли уже такого пути в списке
    for (int i = 0; i < staging->changed_files_count; i++) {
        if (strcmp(staging->changed_files[i], path) == 0) {
            return;  // уже есть
        }
    }
    
    ensure_changed_files_capacity(staging);
    staging->changed_files[staging->changed_files_count++] = strdup(path);
}

// ================ STAGING AREA ================

StagingArea *create_staging(Commit *base_commit) {
    StagingArea *staging = (StagingArea*)malloc(sizeof(StagingArea));
    if (!staging) return NULL;
    
    staging->base_commit = base_commit;
    staging->changed_files = NULL;
    staging->changed_files_count = 0;
    staging->changed_files_capacity = 0;
    
    // Копируем дерево из base_commit (если есть)
    if (base_commit && base_commit->root) {
        staging->root = copy_tree_deep(base_commit->root);  // глубокая копия для staging
    } else {
        staging->root = create_empty_tree();
    }
    
    return staging;
}

void free_staging(StagingArea *staging) {
    if (!staging) return;
    
    if (staging->root) {
        free_tree_node(staging->root);
    }
    
    for (int i = 0; i < staging->changed_files_count; i++) {
        free(staging->changed_files[i]);
    }
    free(staging->changed_files);
    
    free(staging);
}

void staging_add_file(StagingArea *staging, const char *path, const char *content) {
    if (!staging || !path || !content) return;
    
    // Создаём блоб
    Blob *new_blob = create_blob(content, strlen(content));
    if (!new_blob) return;
    
    // Копируем дерево с изменением (structural sharing)
    TreeNode *new_tree = copy_tree_with_change(staging->root, path, new_blob);
    if (!new_tree) {
        free_blob(new_blob);
        return;
    }
    
    // Заменяем дерево
    free_tree_node(staging->root);
    staging->root = new_tree;
    
    // Добавляем в список изменённых
    add_changed_file(staging, path);
    
    printf("staging: added/updated '%s'\n", path);
}

void staging_remove_file(StagingArea *staging, const char *path) {
    if (!staging || !path) return;
    
    // Проверяем, существует ли файл в текущем дереве staging
    if (!file_exists_in_tree(staging->root, path)) {
        printf("staging: file '%s' does not exist\n", path);
        return;
    }
    
    // Копируем дерево с удалением (new_blob = NULL)
    TreeNode *new_tree = copy_tree_with_change(staging->root, path, NULL);
    if (!new_tree) return;
    
    free_tree_node(staging->root);
    staging->root = new_tree;
    
    add_changed_file(staging, path);
    
    printf("staging: removed '%s'\n", path);
}

int staging_has_changes(const StagingArea *staging) {
    return staging && staging->changed_files_count > 0;
}

TreeNode *get_staging_tree(const StagingArea *staging) {
    return staging ? staging->root : NULL;
}

void staging_clear(StagingArea *staging) {
    if (!staging) return;
    
    // Создаём новое дерево на основе base_commit
    TreeNode *old_root = staging->root;
    if (staging->base_commit && staging->base_commit->root) {
        staging->root = copy_tree_deep(staging->base_commit->root);
    } else {
        staging->root = create_empty_tree();
    }
    free_tree_node(old_root);
    
    // Очищаем список изменённых файлов
    for (int i = 0; i < staging->changed_files_count; i++) {
        free(staging->changed_files[i]);
    }
    staging->changed_files_count = 0;
}

// ================ commit() — ГЛАВНАЯ ФУНКЦИЯ ПО ТЗ ================

Commit *commit(Repository *repo, StagingArea *staging, const char *message) {
    if (!staging) {
        printf("commit: nothing to commit (staging is NULL)\n");
        return NULL;
    }
    
    if (!staging_has_changes(staging)) {
        printf("commit: nothing to commit (no changes in staging)\n");
        return NULL;
    }
    
    if (!message || strlen(message) == 0) {
        message = "No message";
    }
    
    // Определяем родительский коммит
    Commit *parent = staging->base_commit;
    
    // Создаём новый коммит
    Commit *new_commit = create_commit(parent, 
                                       staging->root,  // дерево из staging
                                       message,
                                       staging->changed_files,
                                       staging->changed_files_count);
    
    if (!new_commit) {
        printf("commit: failed to create commit\n");
        return NULL;
    }
    
    // Обновляем репозиторий
    if (repo) {
        repo_add_commit(repo, new_commit);
        
        // Обновляем текущую ветку
        if (repo->current_branch_name) {
            repo_update_branch(repo, repo->current_branch_name, new_commit);
        }
        
        // Обновляем HEAD
        repo_set_head(repo, new_commit);
    }
    
    // Обновляем staging: base_commit становится новый коммит
    staging->base_commit = new_commit;
    
    // Очищаем список изменённых файлов (но дерево оставляем, оно теперь в коммите)
    for (int i = 0; i < staging->changed_files_count; i++) {
        free(staging->changed_files[i]);
    }
    staging->changed_files_count = 0;
    
    // Дерево в staging теперь ССЫЛАЕТСЯ на дерево коммита (shared)
    // Не освобождаем его — оно нужно для следующих операций
    
    printf("commit: created commit %d: '%s'\n", new_commit->id, message);
    
    return new_commit;
}