#include "commit.h"
#include "repo.h"
#include "../utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int next_commit_id = 1;

void add_child_commit(Commit *parent, Commit *child) {
    if (!parent || !child) return;

    if (parent->children_count >= parent->children_capacity) {
        int new_capacity = parent->children_capacity == 0
            ? 4
            : parent->children_capacity * 2;
        Commit **new_children = (Commit**)realloc(
            parent->children,
            new_capacity * sizeof(Commit*)
        );
        if (!new_children) return;

        parent->children = new_children;
        parent->children_capacity = new_capacity;
    }

    parent->children[parent->children_count++] = child;
}

Commit *init_repo(void) {
    Commit *initial = (Commit*)malloc(sizeof(Commit));
    if (!initial) return NULL;

    initial->hash = NULL;
    initial->id = next_commit_id++;
    initial->parent = NULL;
    initial->children = NULL;
    initial->children_count = 0;
    initial->children_capacity = 0;
    initial->root = create_empty_tree();
    initial->message = strdup("Initial commit");
    initial->timestamp = time(NULL);
    initial->changed_files = NULL;
    initial->changed_files_count = 0;
    initial->hash = compute_commit_hash(initial);
    return initial;
}

Commit *create_commit(Commit *parent, TreeNode *root, const char *message,
                      char **changed_files, int changed_files_count) {
    Commit *new_commit = (Commit*)malloc(sizeof(Commit));
    if (!new_commit) return NULL;

    new_commit->hash = NULL;
    new_commit->id = next_commit_id++;
    new_commit->parent = parent;
    new_commit->children = NULL;
    new_commit->children_count = 0;
    new_commit->children_capacity = 0;
    new_commit->root = root ? root : create_empty_tree();
    new_commit->message = strdup(message ? message : "");
    new_commit->timestamp = time(NULL);
    new_commit->changed_files = NULL;
    new_commit->changed_files_count = 0;

    if (changed_files && changed_files_count > 0) {
        new_commit->changed_files = (char**)malloc(changed_files_count * sizeof(char*));
        if (new_commit->changed_files) {
            new_commit->changed_files_count = changed_files_count;
            for (int i = 0; i < changed_files_count; i++) {
                new_commit->changed_files[i] = strdup(changed_files[i]);
            }
        }
    }

    new_commit->hash = compute_commit_hash(new_commit);
    add_child_commit(parent, new_commit);
    return new_commit;
}

void free_commit(Commit *commit) {
    if (!commit) return;

    free(commit->hash);
    free(commit->message);
    for (int i = 0; i < commit->changed_files_count; i++) {
        free(commit->changed_files[i]);
    }
    free(commit->changed_files);
    free(commit->children);
    free_tree_node(commit->root);
    free(commit);
}

Commit *add_file(Repository *repo, Commit *current_commit,
                 const char *path, const char *content) {
    if (!path || !content) return current_commit;

    if (!current_commit) {
        current_commit = init_repo();
        if (repo) repo_add_commit(repo, current_commit);
    }

    Blob *new_blob = create_blob(content, strlen(content));
    if (!new_blob) return NULL;

    TreeNode *new_tree = copy_tree_with_change(current_commit->root, path, new_blob);
    if (!new_tree) {
        free_blob(new_blob);
        return NULL;
    }

    char *changed_files[1] = {(char*)path};
    char message[1024];
    snprintf(message, sizeof(message), "Added/updated: %s", path);

    Commit *new_commit = create_commit(current_commit, new_tree, message, changed_files, 1);
    if (!new_commit) {
        free_tree_node(new_tree);
        return NULL;
    }

    if (repo) {
        repo_add_commit(repo, new_commit);
        if (repo->current_branch_name) {
            repo_update_branch(repo, repo->current_branch_name, new_commit);
        }
        repo_set_head(repo, new_commit);
    }
    return new_commit;
}

Commit *remove_file(Repository *repo, Commit *current_commit, const char *path) {
    if (!current_commit || !path) return current_commit;
    if (!file_exists_in_tree(current_commit->root, path)) return current_commit;

    TreeNode *new_tree = copy_tree_with_change(current_commit->root, path, NULL);
    if (!new_tree) return NULL;

    char *changed_files[1] = {(char*)path};
    char message[1024];
    snprintf(message, sizeof(message), "Removed: %s", path);

    Commit *new_commit = create_commit(current_commit, new_tree, message, changed_files, 1);
    if (!new_commit) {
        free_tree_node(new_tree);
        return NULL;
    }

    if (repo) {
        repo_add_commit(repo, new_commit);
        if (repo->current_branch_name) {
            repo_update_branch(repo, repo->current_branch_name, new_commit);
        }
        repo_set_head(repo, new_commit);
    }
    return new_commit;
}

char *get_file_content(Commit *commit, const char *path) {
    if (!commit || !path) return NULL;

    Blob *blob = find_blob_by_path(commit->root, path);
    if (!blob || !blob->content) return NULL;
    return strdup(blob->content);
}

int get_file_exists(Commit *commit, const char *path) {
    if (!commit || !path) return 0;
    return file_exists_in_tree(commit->root, path);
}

int file_exists(Commit *commit, const char *path) {
    return get_file_exists(commit, path);
}

void print_commit(Commit *commit) {
    if (!commit) {
        printf("NULL commit\n");
        return;
    }

    char timebuf[64];
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", localtime(&commit->timestamp));

    printf("Commit %d\n", commit->id);
    printf("  Hash: %s\n", commit->hash ? commit->hash : "none");
    printf("  Parent: %s\n",
           commit->parent && commit->parent->hash ? commit->parent->hash : "none");
    printf("  Message: %s\n", commit->message ? commit->message : "");
    printf("  Date: %s\n", timebuf);
    printf("  Changed files (%d):", commit->changed_files_count);
    for (int i = 0; i < commit->changed_files_count; i++) {
        printf(" %s", commit->changed_files[i]);
    }
    printf("\n");
}

void print_history(Commit *commit) {
    if (!commit) {
        printf("No commits in history\n");
        return;
    }

    printf("\n=== Commit History ===\n");
    for (Commit *current = commit; current; current = current->parent) {
        printf("\n");
        print_commit(current);
    }
}

static void add_file_path(char ***files, int *count, int *capacity, const char *path) {
    if (*count >= *capacity) {
        int new_capacity = *capacity == 0 ? 8 : *capacity * 2;
        char **new_files = (char**)realloc(*files, new_capacity * sizeof(char*));
        if (!new_files) return;

        *files = new_files;
        *capacity = new_capacity;
    }
    (*files)[(*count)++] = strdup(path);
}

static void traverse_tree(TreeNode *node, const char *prefix,
                          char ***files, int *count, int *capacity) {
    if (!node) return;

    char current_path[1024];
    if (!node->name || strlen(node->name) == 0) {
        snprintf(current_path, sizeof(current_path), "%s", prefix ? prefix : "");
    } else if (prefix && strlen(prefix) > 0) {
        snprintf(current_path, sizeof(current_path), "%s/%s", prefix, node->name);
    } else {
        snprintf(current_path, sizeof(current_path), "%s", node->name);
    }

    if (node->is_directory) {
        for (int i = 0; i < node->children_count; i++) {
            traverse_tree(node->children[i], current_path, files, count, capacity);
        }
    } else if (current_path[0] != '\0') {
        add_file_path(files, count, capacity, current_path);
    }
}

void print_files(Commit *commit) {
    if (!commit || !commit->root) {
        printf("No files in this commit\n");
        return;
    }

    char **files = NULL;
    int file_count = 0;
    int capacity = 0;
    traverse_tree(commit->root, "", &files, &file_count, &capacity);

    if (file_count == 0) {
        printf("No files in commit %d\n", commit->id);
        return;
    }

    printf("\nFiles in commit %d:\n", commit->id);
    for (int i = 0; i < file_count; i++) {
        Blob *blob = find_blob_by_path(commit->root, files[i]);
        printf("  %-40s [%s]\n", files[i], blob && blob->hash ? blob->hash : "no-hash");
        free(files[i]);
    }
    printf("Total: %d file(s)\n", file_count);
    free(files);
}

static int path_in_list(char **files, int count, const char *path) {
    for (int i = 0; i < count; i++) {
        if (strcmp(files[i], path) == 0) return 1;
    }
    return 0;
}

static void free_file_list(char **files, int count) {
    if (!files) return;
    for (int i = 0; i < count; i++) {
        free(files[i]);
    }
    free(files);
}

static void collect_commit_files(Commit *commit, char ***files, int *count, int *capacity) {
    *files = NULL;
    *count = 0;
    *capacity = 0;
    if (commit && commit->root) {
        traverse_tree(commit->root, "", files, count, capacity);
    }
}

int restore_commit_files(Commit *previous_commit, Commit *target_commit) {
    if (!target_commit || !target_commit->root) return 0;

    char **old_files = NULL;
    char **new_files = NULL;
    int old_count = 0, new_count = 0;
    int old_capacity = 0, new_capacity = 0;
    int ok = 1;

    collect_commit_files(previous_commit, &old_files, &old_count, &old_capacity);
    collect_commit_files(target_commit, &new_files, &new_count, &new_capacity);

    for (int i = 0; i < old_count; i++) {
        if (!path_in_list(new_files, new_count, old_files[i])) {
            if (!remove_text_file(old_files[i])) {
                ok = 0;
            }
        }
    }

    for (int i = 0; i < new_count; i++) {
        Blob *blob = find_blob_by_path(target_commit->root, new_files[i]);
        if (!blob || !write_text_file(new_files[i], blob->content, blob->size)) {
            ok = 0;
        }
    }

    free_file_list(old_files, old_count);
    free_file_list(new_files, new_count);
    return ok;
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

static int pointer_seen(void **items, int count, void *ptr) {
    for (int i = 0; i < count; i++) {
        if (items[i] == ptr) return 1;
    }
    return 0;
}

static void count_tree_objects(TreeNode *node, void ***trees, int *tree_count,
                               int *tree_capacity, void ***blobs, int *blob_count,
                               int *blob_capacity) {
    if (!node) return;

    if (!pointer_seen(*trees, *tree_count, node)) {
        if (*tree_count >= *tree_capacity) {
            *tree_capacity = *tree_capacity == 0 ? 16 : *tree_capacity * 2;
            *trees = (void**)realloc(*trees, *tree_capacity * sizeof(void*));
        }
        (*trees)[(*tree_count)++] = node;
    }

    if (node->is_directory) {
        for (int i = 0; i < node->children_count; i++) {
            count_tree_objects(node->children[i], trees, tree_count, tree_capacity,
                               blobs, blob_count, blob_capacity);
        }
    } else if (node->blob && !pointer_seen(*blobs, *blob_count, node->blob)) {
        if (*blob_count >= *blob_capacity) {
            *blob_capacity = *blob_capacity == 0 ? 16 : *blob_capacity * 2;
            *blobs = (void**)realloc(*blobs, *blob_capacity * sizeof(void*));
        }
        (*blobs)[(*blob_count)++] = node->blob;
    }
}

int count_objects(Commit *commit, int *tree_count, int *blob_count) {
    void **trees = NULL;
    void **blobs = NULL;
    int tc = 0, bc = 0, tree_capacity = 0, blob_capacity = 0;

    for (Commit *current = commit; current; current = current->parent) {
        count_tree_objects(current->root, &trees, &tc, &tree_capacity,
                           &blobs, &bc, &blob_capacity);
    }

    free(trees);
    free(blobs);
    if (tree_count) *tree_count = tc;
    if (blob_count) *blob_count = bc;
    return tc + bc;
}
