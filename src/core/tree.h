#ifndef TREE_H
#define TREE_H

#include <stddef.h>
#include <time.h>
#include "types.h"

// Создание
TreeNode *create_empty_tree(void);
Blob *create_blob(const char *content, size_t size);
TreeNode* create_directory_node(const char *name);

// Удаление
void free_tree_node(TreeNode *node);
void free_blob(Blob *blob);

// Поиск
Blob *find_blob_by_path(TreeNode *tree, const char *path);
int file_exists_in_tree(TreeNode *tree, const char *path);

// Structural sharing — основная функция
TreeNode *copy_tree_with_change(TreeNode *old_tree, const char *path, 
                                Blob *new_blob);

// Операции с деревьями (экспортируемые)
void append_child(TreeNode *dir, TreeNode *child);
TreeNode *clone_directory_shallow(TreeNode *src);

// Отладка
void print_tree(TreeNode *node, int level);
int is_shared_node(TreeNode *node1, TreeNode *node2);

// Создаёт глубокую копию дерева (для staging)
TreeNode *copy_tree_deep(const TreeNode *src);

#endif