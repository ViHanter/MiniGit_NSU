#include "tree.h"
#include "types.h"
#include "../utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void retain_tree_node(TreeNode *node) {
    if (node) node->ref_count++;
}

TreeNode *create_empty_tree(void) {
    TreeNode *root = (TreeNode*)malloc(sizeof(TreeNode));
    if (!root) return NULL;

    root->name = strdup("");
    root->is_directory = 1;
    root->blob = NULL;
    root->children = NULL;
    root->children_count = 0;
    root->ref_count = 1;
    return root;
}

static TreeNode *create_directory_node(const char *name) {
    TreeNode *node = (TreeNode*)malloc(sizeof(TreeNode));
    if (!node) return NULL;

    node->name = strdup(name ? name : "");
    node->is_directory = 1;
    node->blob = NULL;
    node->children = NULL;
    node->children_count = 0;
    node->ref_count = 1;
    return node;
}

static TreeNode *create_file_node(const char *name, Blob *blob) {
    TreeNode *node = (TreeNode*)malloc(sizeof(TreeNode));
    if (!node) return NULL;

    node->name = strdup(name ? name : "");
    node->is_directory = 0;
    node->blob = blob;
    node->children = NULL;
    node->children_count = 0;
    node->ref_count = 1;
    return node;
}

Blob *create_blob(const char *content, size_t size) {
    if (!content) return NULL;

    Blob *blob = (Blob*)malloc(sizeof(Blob));
    if (!blob) return NULL;

    blob->content = (char*)malloc(size + 1);
    if (!blob->content) {
        free(blob);
        return NULL;
    }

    memcpy(blob->content, content, size);
    blob->content[size] = '\0';
    blob->size = size;
    blob->hash = compute_hash(content, size);
    blob->ref_count = 1;
    if (blob->hash) {
        store_blob_object(blob->hash, blob->content, blob->size);
    }
    return blob;
}

void free_blob(Blob *blob) {
    if (!blob) return;
    blob->ref_count--;
    if (blob->ref_count > 0) return;

    free(blob->content);
    free(blob->hash);
    free(blob);
}

void free_tree_node(TreeNode *node) {
    if (!node) return;
    node->ref_count--;
    if (node->ref_count > 0) return;

    free(node->name);
    if (node->is_directory) {
        for (int i = 0; i < node->children_count; i++) {
            free_tree_node(node->children[i]);
        }
        free(node->children);
    } else {
        free_blob(node->blob);
    }
    free(node);
}

static int find_child_index(TreeNode *dir, const char *name) {
    if (!dir || !dir->is_directory || !name) return -1;

    for (int i = 0; i < dir->children_count; i++) {
        if (dir->children[i] && strcmp(dir->children[i]->name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static void append_child(TreeNode *dir, TreeNode *child) {
    if (!dir || !child) return;

    TreeNode **new_children = (TreeNode**)realloc(
        dir->children,
        (dir->children_count + 1) * sizeof(TreeNode*)
    );
    if (!new_children) return;

    dir->children = new_children;
    dir->children[dir->children_count++] = child;
}

static TreeNode *clone_directory_shallow(TreeNode *src) {
    TreeNode *copy = create_directory_node(src && src->name ? src->name : "");
    if (!copy || !src || !src->is_directory || src->children_count == 0) {
        return copy;
    }

    copy->children_count = src->children_count;
    copy->children = (TreeNode**)malloc(copy->children_count * sizeof(TreeNode*));
    if (!copy->children) {
        copy->children_count = 0;
        return copy;
    }

    for (int i = 0; i < src->children_count; i++) {
        copy->children[i] = src->children[i];
        retain_tree_node(copy->children[i]);
    }
    return copy;
}

Blob *find_blob_by_path(TreeNode *tree, const char *path) {
    if (!tree || !path || strlen(path) == 0) return NULL;

    int parts_count = 0;
    char **parts = split_path(path, &parts_count);
    if (!parts || parts_count == 0) {
        free_split_path(parts, parts_count);
        return NULL;
    }

    TreeNode *current = tree;
    for (int i = 0; i < parts_count; i++) {
        if (!current || !current->is_directory) {
            free_split_path(parts, parts_count);
            return NULL;
        }

        int index = find_child_index(current, parts[i]);
        if (index < 0) {
            free_split_path(parts, parts_count);
            return NULL;
        }
        current = current->children[index];
    }

    free_split_path(parts, parts_count);
    return current && !current->is_directory ? current->blob : NULL;
}

int file_exists_in_tree(TreeNode *tree, const char *path) {
    return find_blob_by_path(tree, path) != NULL;
}

TreeNode *copy_tree_deep(const TreeNode *src) {
    if (!src) return NULL;

    TreeNode *dst = (TreeNode*)malloc(sizeof(TreeNode));
    if (!dst) return NULL;

    dst->name = strdup(src->name ? src->name : "");
    dst->is_directory = src->is_directory;
    dst->blob = NULL;
    dst->children = NULL;
    dst->children_count = 0;
    dst->ref_count = 1;

    if (src->is_directory) {
        dst->children_count = src->children_count;
        if (dst->children_count > 0) {
            dst->children = (TreeNode**)malloc(dst->children_count * sizeof(TreeNode*));
            if (!dst->children) {
                free(dst->name);
                free(dst);
                return NULL;
            }
            for (int i = 0; i < dst->children_count; i++) {
                dst->children[i] = copy_tree_deep(src->children[i]);
            }
        }
    } else if (src->blob) {
        dst->blob = create_blob(src->blob->content, src->blob->size);
    }

    return dst;
}

static TreeNode *copy_tree_replace(TreeNode *src, char **parts, int parts_count,
                                   int depth, Blob *new_blob) {
    TreeNode *copy = src && src->is_directory
        ? clone_directory_shallow(src)
        : create_directory_node(depth == 0 ? "" : parts[depth - 1]);
    if (!copy || depth >= parts_count) return copy;

    const char *name = parts[depth];
    int child_index = find_child_index(copy, name);
    int is_leaf = (depth == parts_count - 1);

    if (is_leaf) {
        if (new_blob) {
            TreeNode *file = create_file_node(name, new_blob);
            if (child_index >= 0) {
                free_tree_node(copy->children[child_index]);
                copy->children[child_index] = file;
            } else {
                append_child(copy, file);
            }
        } else if (child_index >= 0) {
            free_tree_node(copy->children[child_index]);
            for (int i = child_index; i < copy->children_count - 1; i++) {
                copy->children[i] = copy->children[i + 1];
            }
            copy->children_count--;
            if (copy->children_count == 0) {
                free(copy->children);
                copy->children = NULL;
            } else {
                copy->children = (TreeNode**)realloc(
                    copy->children,
                    copy->children_count * sizeof(TreeNode*)
                );
            }
        }
        return copy;
    }

    TreeNode *old_child = NULL;
    if (child_index >= 0 && copy->children[child_index]->is_directory) {
        old_child = copy->children[child_index];
    }

    TreeNode *updated_child = copy_tree_replace(old_child, parts, parts_count, depth + 1, new_blob);
    if (!updated_child) return copy;

    if (child_index >= 0) {
        free_tree_node(copy->children[child_index]);
        copy->children[child_index] = updated_child;
    } else {
        append_child(copy, updated_child);
    }
    return copy;
}

TreeNode *copy_tree_with_change(TreeNode *old_tree, const char *path, Blob *new_blob) {
    if (!path || strlen(path) == 0) {
        free_blob(new_blob);
        if (old_tree) {
            retain_tree_node(old_tree);
            return old_tree;
        }
        return create_empty_tree();
    }

    int parts_count = 0;
    char **parts = split_path(path, &parts_count);
    if (!parts || parts_count == 0) {
        free_blob(new_blob);
        free_split_path(parts, parts_count);
        if (old_tree) {
            retain_tree_node(old_tree);
            return old_tree;
        }
        return create_empty_tree();
    }

    TreeNode *new_root = copy_tree_replace(old_tree, parts, parts_count, 0, new_blob);
    free_split_path(parts, parts_count);
    return new_root;
}

void print_tree(TreeNode *node, int level) {
    if (!node) return;

    for (int i = 0; i < level; i++) printf("  ");

    if (node->is_directory) {
        printf("%s/\n", node->name && strlen(node->name) > 0 ? node->name : ".");
        for (int i = 0; i < node->children_count; i++) {
            print_tree(node->children[i], level + 1);
        }
    } else {
        printf("%s", node->name);
        if (node->blob && node->blob->hash) {
            printf(" [%s]", node->blob->hash);
        }
        printf("\n");
    }
}

int is_shared_node(TreeNode *node1, TreeNode *node2) {
    return node1 != NULL && node1 == node2;
}
