#include "staging.h"
#include "commit.h"
#include "repo.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "repo_storage.h"

static void add_changed_file(StagingArea *staging, const char *path) {
    if (!staging || !path) return;

    for (int i = 0; i < staging->changed_files_count; i++) {
        if (strcmp(staging->changed_files[i], path) == 0) {
            return;
        }
    }

    if (staging->changed_files_count >= staging->changed_files_capacity) {
        int new_capacity = staging->changed_files_capacity == 0
            ? 4
            : staging->changed_files_capacity * 2;
        char **new_files = (char**)realloc(
            staging->changed_files,
            new_capacity * sizeof(char*)
        );
        if (!new_files) return;

        staging->changed_files = new_files;
        staging->changed_files_capacity = new_capacity;
    }

    staging->changed_files[staging->changed_files_count++] = strdup(path);
}

StagingArea *staging_create(Commit *base) {
    StagingArea *staging = (StagingArea*)malloc(sizeof(StagingArea));
    if (!staging) return NULL;

    staging->base_commit = base;
    staging->pending_tree = NULL;
    staging->changed_files = NULL;
    staging->changed_files_count = 0;
    staging->changed_files_capacity = 0;
    return staging;
}

void staging_destroy(StagingArea *staging) {
    if (!staging) return;

    if (staging->pending_tree) {
        free_tree_node(staging->pending_tree);
    }
    for (int i = 0; i < staging->changed_files_count; i++) {
        free(staging->changed_files[i]);
    }
    free(staging->changed_files);
    free(staging);
}

static TreeNode *staging_base_tree(StagingArea *staging) {
    if (staging->pending_tree) return staging->pending_tree;
    if (staging->base_commit && staging->base_commit->root) {
        return staging->base_commit->root;
    }
    return NULL;
}

void staging_add_file(StagingArea *staging, const char *path, const char *content) {
    if (!staging || !path || !content || strlen(path) == 0) return;

    Blob *new_blob = create_blob(content, strlen(content));
    if (!new_blob) return;

    TreeNode *old_tree = staging_base_tree(staging);
    TreeNode *new_tree = copy_tree_with_change(old_tree, path, new_blob);
    if (!new_tree) {
        free_blob(new_blob);
        return;
    }

    if (staging->pending_tree) {
        free_tree_node(staging->pending_tree);
    }
    staging->pending_tree = new_tree;
    add_changed_file(staging, path);
}

void staging_remove_file(StagingArea *staging, const char *path) {
    if (!staging || !path || strlen(path) == 0) return;

    TreeNode *old_tree = staging_base_tree(staging);
    if (!old_tree || !file_exists_in_tree(old_tree, path)) {
        printf("staging: file '%s' does not exist\n", path);
        return;
    }

    TreeNode *new_tree = copy_tree_with_change(old_tree, path, NULL);
    if (!new_tree) return;

    if (staging->pending_tree) {
        free_tree_node(staging->pending_tree);
    }
    staging->pending_tree = new_tree;
    add_changed_file(staging, path);
}

int staging_has_changes(const StagingArea *staging) {
    return staging && staging->changed_files_count > 0;
}

Commit *staging_commit(Repository *repo, StagingArea *staging, const char *message) {
    if (!staging || !staging_has_changes(staging)) {
        printf("commit: nothing to commit\n");
        return NULL;
    }

    TreeNode *tree_to_commit = staging->pending_tree;
    if (!tree_to_commit) {
        printf("commit: internal error - no tree but changes exist\n");
        return NULL;
    }

    Commit *new_commit = create_commit(
        staging->base_commit,
        tree_to_commit,
        message && strlen(message) > 0 ? message : "No message",
        staging->changed_files,
        staging->changed_files_count
    );
    if (!new_commit) return NULL;

    if (repo) {
        repo_add_commit(repo, new_commit);
        if (repo->current_branch_name) {
            repo_update_branch(repo, repo->current_branch_name, new_commit);
        }
        repo_set_head(repo, new_commit);
        
        repo_save_state(repo);
    }

    staging->pending_tree = NULL;
    for (int i = 0; i < staging->changed_files_count; i++) {
        free(staging->changed_files[i]);
        staging->changed_files[i] = NULL;
    }
    staging->changed_files_count = 0;
    staging->base_commit = new_commit;
    return new_commit;
}

Commit *commit(Repository *repo, StagingArea *staging, const char *message) {
    return staging_commit(repo, staging, message);
}

TreeNode *staging_get_tree(StagingArea *staging) {
    return staging_base_tree(staging);
}
