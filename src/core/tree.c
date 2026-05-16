#include "tree.h"
#include "types.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


// Разбивает путь "src/main.c" на части ["src", "main.c"]
char **split_path(const char *path, int *parts_count) {
    if (!path || strlen(path) == 0) {
        *parts_count = 0;
        return NULL;
    }
    
    char *path_copy = strdup(path);
    char **parts = NULL;
    int count = 0;
    
    char *token = strtok(path_copy, "/");
    while (token) {
        parts = (char**)realloc(parts, (count + 1) * sizeof(char*));
        parts[count] = strdup(token);
        count++;
        token = strtok(NULL, "/");
    }
    
    free(path_copy);
    *parts_count = count;
    return parts;
}

void free_split_path(char **parts, int count) {
    for (int i = 0; i < count; i++) {
        free(parts[i]);
    }
    free(parts);
}

// ================ СОЗДАНИЕ И УДАЛЕНИЕ ================

TreeNode *create_empty_tree(void) {
    // Пустой корневой каталог
    TreeNode *root = (TreeNode*)malloc(sizeof(TreeNode));
    root->name = strdup("");
    root->is_directory = 1;
    root->blob = NULL;
    root->children = NULL;
    root->children_count = 0;
    return root;
}

Blob *create_blob(const char *content, size_t size) {
    if (!content) return NULL;
    
    Blob *blob = (Blob*)malloc(sizeof(Blob));
    blob->content = (char*)malloc(size + 1);
    memcpy(blob->content, content, size);
    blob->content[size] = '\0';
    blob->size = size;
    blob->hash = NULL;  // хеш вычислим позже
    
    return blob;
}

void free_blob(Blob *blob) {
    if (!blob) return;
    if (blob->content) free(blob->content);
    if (blob->hash) free(blob->hash);
    free(blob);
}

void free_tree_node(TreeNode *node) {
    if (!node) return;
    
    free(node->name);
    
    if (node->is_directory) {
        for (int i = 0; i < node->children_count; i++) {
            free_tree_node(node->children[i]);
        }
        if (node->children) free(node->children);
    } else {
        if (node->blob) free_blob(node->blob);
    }
    
    free(node);
}

// ================ ПОИСК В ДЕРЕВЕ ================

Blob *find_blob_by_path(TreeNode *tree, const char *path) {
    if (!tree || !path) return NULL;
    
    int parts_count;
    char **parts = split_path(path, &parts_count);
    if (parts_count == 0) {
        free_split_path(parts, parts_count);
        return NULL;
    }
    
    TreeNode *current = tree;
    
    for (int i = 0; i < parts_count; i++) {
        if (!current || !current->is_directory) {
            free_split_path(parts, parts_count);
            return NULL;
        }
        
        TreeNode *found = NULL;
        for (int j = 0; j < current->children_count; j++) {
            if (strcmp(current->children[j]->name, parts[i]) == 0) {
                found = current->children[j];
                break;
            }
        }
        
        if (!found) {
            free_split_path(parts, parts_count);
            return NULL;
        }
        
        current = found;
        
        // Если это последняя часть пути и это файл
        if (i == parts_count - 1 && !current->is_directory) {
            free_split_path(parts, parts_count);
            return current->blob;
        }
    }
    
    free_split_path(parts, parts_count);
    return NULL;
}

int file_exists_in_tree(TreeNode *tree, const char *path) {
    return find_blob_by_path(tree, path) != NULL;
}

// ================ STRUCTURAL SHARING — САМОЕ ВАЖНОЕ ================

// Копирует узел дерева с изменением по пути
// depth: текущая глубина (индекс в parts)
// parts: массив частей пути
// parts_count: общее количество частей
// new_blob: новый блоб (если NULL — удаление)
static TreeNode *copy_tree_node_recursive(TreeNode *old_node, 
                                          char **parts, int parts_count,
                                          int depth, Blob *new_blob) {
    // Если мы не на целевой ветке — ВОЗВРАЩАЕМ СТАРЫЙ УЗЕЛ (shared!)
    if (old_node->is_directory && depth < parts_count - 1) {
        // Ищем, есть ли среди детей целевой путь
        int has_target_child = 0;
        for (int i = 0; i < old_node->children_count; i++) {
            if (strcmp(old_node->children[i]->name, parts[depth]) == 0) {
                has_target_child = 1;
                break;
            }
        }
        
        // Если среди детей нет целевого пути — весь поддерево не меняется
        if (!has_target_child) {
            return old_node;  // ← SHARED! Просто возвращаем старый узел
        }
    }
    
    // Создаём НОВЫЙ узел ТОЛЬКО когда что-то реально меняется
    TreeNode *new_node = (TreeNode*)malloc(sizeof(TreeNode));
    new_node->name = strdup(old_node->name);
    new_node->is_directory = old_node->is_directory;
    new_node->children = NULL;
    new_node->children_count = 0;
    new_node->blob = NULL;
    
    if (!new_node->is_directory) {
        // Файл
        if (depth == parts_count - 1) {
            // Целевой файл — новый блоб
            if (new_blob) {
                new_node->blob = new_blob;
            } else {
                new_node->blob = NULL;  // удаление
            }
        } else {
            // Не целевой файл — shared!
            return old_node;  // ← просто возвращаем старый узел
        }
        return new_node;
    }
    
    // Директория — копируем только изменяющихся детей
    new_node->children_count = old_node->children_count;
    new_node->children = (TreeNode**)malloc(new_node->children_count * sizeof(TreeNode*));
    
    for (int i = 0; i < old_node->children_count; i++) {
        TreeNode *old_child = old_node->children[i];
        
        if (depth < parts_count && strcmp(old_child->name, parts[depth]) == 0) {
            // Этот ребёнок на пути изменений — идём глубже
            new_node->children[i] = copy_tree_node_recursive(
                old_child, parts, parts_count, depth + 1, new_blob
            );
        } else {
            // Этот ребёнок не меняется — ПРЯМАЯ ССЫЛКА на старый узел
            new_node->children[i] = old_child;  // ← SHARED!
        }
    }
    
    return new_node;
}

// Основная функция: копирует дерево с изменением одного файла
TreeNode *copy_tree_with_change(TreeNode *old_tree, const char *path, 
                                Blob *new_blob) {
    if (!old_tree) {
        old_tree = create_empty_tree();
    }
    
    if (!path || strlen(path) == 0) {
        // Пустой путь — меняем корень?
        TreeNode *new_root = (TreeNode*)malloc(sizeof(TreeNode));
        new_root->name = strdup(old_tree->name);
        new_root->is_directory = old_tree->is_directory;
        new_root->children_count = old_tree->children_count;
        new_root->children = old_tree->children;  // shared
        new_root->blob = new_blob;  // новый блоб на месте корня (редко)
        return new_root;
    }
    
    int parts_count;
    char **parts = split_path(path, &parts_count);
    
    TreeNode *result = copy_tree_node_recursive(
        old_tree, parts, parts_count, 0, new_blob
    );
    
    free_split_path(parts, parts_count);
    return result;
}

TreeNode *copy_tree_deep(const TreeNode *src) {
    if (!src) return NULL;
    
    TreeNode *dst = (TreeNode*)malloc(sizeof(TreeNode));
    dst->name = strdup(src->name);
    dst->is_directory = src->is_directory;
    dst->children_count = src->children_count;
    dst->blob = NULL;
    
    if (!src->is_directory) {
        // Файл: копируем блоб
        if (src->blob) {
            dst->blob = create_blob(src->blob->content, src->blob->size);
            if (src->blob->hash) {
                dst->blob->hash = strdup(src->blob->hash);
            }
        } else {
            dst->blob = NULL;
        }
        dst->children = NULL;
    } else {
        // Директория: рекурсивно копируем детей
        dst->children = (TreeNode**)malloc(dst->children_count * sizeof(TreeNode*));
        for (int i = 0; i < dst->children_count; i++) {
            dst->children[i] = copy_tree_deep(src->children[i]);
        }
    }
    
    return dst;
}
// ================ ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ДЛЯ ОТЛАДКИ ================

void print_tree(TreeNode *node, int level) {
    if (!node) return;
    
    for (int i = 0; i < level; i++) printf("  ");
    
    if (node->is_directory) {
        printf("📁 %s/\n", node->name);
        for (int i = 0; i < node->children_count; i++) {
            print_tree(node->children[i], level + 1);
        }
    } else {
        printf("📄 %s", node->name);
        if (node->blob) {
            printf(" [%zu bytes", node->blob->size);
            if (node->blob->hash) printf(", hash: %s", node->blob->hash);
            printf("]");
        } else {
            printf(" [DELETED]");
        }
        printf("\n");
    }
}

