#include "commit.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static int next_commit_id = 1;

// Вычисляет SHA-1 (заглушка — реальный хеш через openssl или свою реализацию)
char *compute_commit_hash(const Commit *commit) {
    // Временная заглушка: хеш = "hash_" + id
    char *hash = (char*)malloc(64);
    snprintf(hash, 64, "hash_%d_%ld", commit->id, commit->timestamp);
    return hash;
}

Commit *init_repo(void) {
    // Создаём пустое дерево
    TreeNode *empty_root = create_empty_tree();
    
    // Создаём начальный коммит без родителя
    Commit *initial = (Commit*)malloc(sizeof(Commit));
    initial->hash = NULL;
    initial->id = next_commit_id++;
    initial->parent = NULL;
    initial->children = NULL;
    initial->children_count = 0;
    initial->children_capacity = 0;
    initial->root = empty_root;
    initial->message = strdup("Initial commit");
    initial->timestamp = time(NULL);
    initial->changed_files = NULL;
    initial->changed_files_count = 0;
    
    initial->hash = compute_commit_hash(initial);
    
    return initial;
}

Commit *commit(Commit *parent, TreeNode *root, const char *message,
               char **changed_files, int changed_files_count) {
    Commit *new_commit = (Commit*)malloc(sizeof(Commit));
    new_commit->hash = NULL;
    new_commit->id = next_commit_id++;
    new_commit->parent = parent;
    new_commit->children = NULL;
    new_commit->children_count = 0;
    new_commit->children_capacity = 0;
    new_commit->root = root;
    new_commit->message = message ? strdup(message) : strdup("");
    new_commit->timestamp = time(NULL);
    
    // Копируем список изменённых файлов
    if (changed_files && changed_files_count > 0) {
        new_commit->changed_files_count = changed_files_count;
        new_commit->changed_files = (char**)malloc(changed_files_count * sizeof(char*));
        for (int i = 0; i < changed_files_count; i++) {
            new_commit->changed_files[i] = strdup(changed_files[i]);
        }
    } else {
        new_commit->changed_files = NULL;
        new_commit->changed_files_count = 0;
    }
    
    new_commit->hash = compute_commit_hash(new_commit);
    
    // Если есть родитель, добавляем себя как дочерний коммит
    if (parent) {
        add_child_commit(parent, new_commit);
    }
    
    return new_commit;
}

void add_child_commit(Commit *parent, Commit *child) {
    if (!parent || !child) return;
    
    if (parent->children_count >= parent->children_capacity) {
        parent->children_capacity = parent->children_capacity == 0 ? 4 : parent->children_capacity * 2;
        parent->children = (Commit**)realloc(parent->children,
                                              parent->children_capacity * sizeof(Commit*));
    }
    parent->children[parent->children_count++] = child;
}

void free_commit(Commit *commit) {
    if (!commit) return;
    
    if (commit->message) free(commit->message);
    if (commit->hash) free(commit->hash);
    
    for (int i = 0; i < commit->changed_files_count; i++) {
        free(commit->changed_files[i]);
    }
    if (commit->changed_files) free(commit->changed_files);
    
    if (commit->children) free(commit->children);
    
    // Дерево не освобождаем здесь — оно может быть shared между коммитами
    // Освобождение дерева — отдельная ответственность
    
    free(commit);
}

Commit *find_commit_by_id(Commit **all_commits, int count, int id) {
    if (!all_commits) return NULL;
    for (int i = 0; i < count; i++) {
        if (all_commits[i] && all_commits[i]->id == id) {
            return all_commits[i];
        }
    }
    return NULL;
}