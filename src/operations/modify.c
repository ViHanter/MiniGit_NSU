#include "modify.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Внутренняя функция: создаёт список изменённых файлов
static char **create_changed_files_list(const char *path, int *count) {
    *count = 1;
    char **list = (char**)malloc(sizeof(char*));
    list[0] = strdup(path);
    return list;
}

// Внутренняя функция: освобождает список изменённых файлов
static void free_changed_files_list(char **list, int count) {
    if (!list) return;
    for (int i = 0; i < count; i++) {
        free(list[i]);
    }
    free(list);
}

// Внутренняя функция: обновляет репозиторий после создания коммита
void update_repo_after_commit(Repository *repo, Commit *new_commit) {
    if (!repo || !new_commit) return;
    
    // Добавляем коммит в репозиторий
    repo_add_commit(repo, new_commit);
    
    // Обновляем текущую ветку
    if (repo->current_branch_name) {
        repo_update_branch(repo, repo->current_branch_name, new_commit);
    }
    
    // Обновляем HEAD
    repo_set_head(repo, new_commit);
}

// add_file(commit, path, content) -> новый_commit
//
// 1. Создаёт блоб из содержимого
// 2. Копирует дерево с изменением (structural sharing)
// 3. Создаёт новый коммит
// 4. Обновляет репозиторий (ветку и HEAD)
//
Commit *add_file(Repository *repo, Commit *current_commit, 
                 const char *path, const char *content) {
    if (!path || !content) {
        printf("add_file: invalid arguments\n");
        return NULL;
    }
    
    // Если нет текущего коммита, создаём пустой репозиторий
    if (!current_commit) {
        current_commit = init_repo();
        if (repo) repo_add_commit(repo, current_commit);
    }
    
    // 1. Создаём блоб из содержимого
    size_t content_len = strlen(content);
    Blob *new_blob = create_blob(content, content_len);
    if (!new_blob) {
        printf("add_file: failed to create blob\n");
        return NULL;
    }
    
    // 2. Копируем дерево с заменой (structural sharing!)
    TreeNode *new_tree = copy_tree_with_change(current_commit->root, path, new_blob);
    if (!new_tree) {
        printf("add_file: failed to copy tree\n");
        free_blob(new_blob);
        return NULL;
    }
    
    // 3. Создаём список изменённых файлов
    int changed_count;
    char **changed_files = create_changed_files_list(path, &changed_count);
    
    // 4. Формируем сообщение коммита
    char *message = (char*)malloc(strlen(path) + 50);
    snprintf(message, strlen(path) + 50, "Added/updated: %s", path);
    
    // 5. Создаём новый коммит
    Commit *new_commit = commit(current_commit, new_tree, message, 
                                changed_files, changed_count);
    
    free(message);
    free_changed_files_list(changed_files, changed_count);
    
    if (!new_commit) {
        printf("add_file: failed to create commit\n");
        free_tree_node(new_tree);
        return NULL;
    }
    
    // 6. Обновляем репозиторий
    if (repo) {
        update_repo_after_commit(repo, new_commit);
    }
    
    printf("add_file: created commit %d with file '%s'\n", new_commit->id, path);
    return new_commit;
}

// remove_file(commit, path) -> новый_commit
//
// 1. Проверяет, существует ли файл
// 2. Копирует дерево, передавая NULL как новый блоб (означает удаление)
// 3. Создаёт новый коммит
// 4. Обновляет репозиторий
//
Commit *remove_file(Repository *repo, Commit *current_commit, 
                    const char *path) {
    if (!path) {
        printf("remove_file: invalid arguments\n");
        return NULL;
    }
    
    if (!current_commit) {
        printf("remove_file: no current commit\n");
        return NULL;
    }
    
    // Проверяем, существует ли файл
    if (!file_exists_in_tree(current_commit->root, path)) {
        printf("remove_file: file '%s' does not exist\n", path);
        return current_commit;  // возвращаем тот же коммит (по ТЗ)
    }
    
    // 1. Копируем дерево с удалением (new_blob = NULL)
    TreeNode *new_tree = copy_tree_with_change(current_commit->root, path, NULL);
    if (!new_tree) {
        printf("remove_file: failed to copy tree\n");
        return NULL;
    }
    
    // 2. Создаём список изменённых файлов
    int changed_count;
    char **changed_files = create_changed_files_list(path, &changed_count);
    
    // 3. Формируем сообщение коммита
    char *message = (char*)malloc(strlen(path) + 50);
    snprintf(message, strlen(path) + 50, "Removed: %s", path);
    
    // 4. Создаём новый коммит
    Commit *new_commit = commit(current_commit, new_tree, message,
                                changed_files, changed_count);
    
    free(message);
    free_changed_files_list(changed_files, changed_count);
    
    if (!new_commit) {
        printf("remove_file: failed to create commit\n");
        free_tree_node(new_tree);
        return NULL;
    }
    
    // 5. Обновляем репозиторий
    if (repo) {
        update_repo_after_commit(repo, new_commit);
    }
    
    printf("remove_file: created commit %d, removed '%s'\n", new_commit->id, path);
    return new_commit;
}

// Если у тебя есть staging area (промежуточное состояние),
// эта функция фиксирует его и создаёт коммит.
// Если staging нет — можно использовать add_file напрямую.
//
Commit *commit_changes(Repository *repo, Commit *staging, const char *message) {
    if (!staging) {
        printf("commit_changes: nothing to commit\n");
        return NULL;
    }
    
    if (!message || strlen(message) == 0) {
        message = "No message";
    }
    
    // Создаём коммит из staging-состояния
    Commit *new_commit = commit(staging->parent, staging->root, message,
                                staging->changed_files, staging->changed_files_count);
    
    if (!new_commit) {
        printf("commit_changes: failed to create commit\n");
        return NULL;
    }
    
    if (repo) {
        update_repo_after_commit(repo, new_commit);
    }
    
    printf("commit_changes: created commit %d: '%s'\n", new_commit->id, message);
    return new_commit;
}