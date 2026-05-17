#include "repo.h"
#include "commit.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static void ensure_commits_capacity(Repository *repo) {
    if (repo->commits_count >= repo->commits_capacity) {
        repo->commits_capacity = repo->commits_capacity == 0 ? 10 : repo->commits_capacity * 2;
        repo->all_commits = (Commit**)realloc(repo->all_commits, 
                                               repo->commits_capacity * sizeof(Commit*));
    }
}

static void ensure_branches_capacity(Repository *repo) {
    if (repo->branches_count >= repo->branches_capacity) {
        repo->branches_capacity = repo->branches_capacity == 0 ? 5 : repo->branches_capacity * 2;
        repo->branches = (Branch*)realloc(repo->branches,
                                          repo->branches_capacity * sizeof(Branch));
    }
}

Repository *repo_create(void) {
    Repository *repo = (Repository*)malloc(sizeof(Repository));
    if (!repo) return NULL;
    
    repo->head = NULL;
    repo->current_branch_name = strdup("master");
    repo->all_commits = NULL;
    repo->commits_count = 0;
    repo->commits_capacity = 0;
    repo->branches = NULL;
    repo->branches_count = 0;
    repo->branches_capacity = 0;
    
    repo_create_branch(repo, "master", NULL);
    
    return repo;
}

void repo_destroy(Repository *repo) {
    if (!repo) return;
    
    for (int i = 0; i < repo->commits_count; i++) {
        free_commit(repo->all_commits[i]);
    }
    free(repo->all_commits);
    
    for (int i = 0; i < repo->branches_count; i++) {
        free(repo->branches[i].name);
    }
    free(repo->branches);
    
    free(repo->current_branch_name);
    free(repo);
}

void repo_add_commit(Repository *repo, Commit *commit) {
    if (!repo || !commit) return;
    if (repo_find_commit_by_id(repo, commit->id)) return;
    
    ensure_commits_capacity(repo);
    repo->all_commits[repo->commits_count++] = commit;
}

Commit *repo_find_commit_by_id(Repository *repo, int id) {
    if (!repo) return NULL;
    return find_commit_by_id(repo->all_commits, repo->commits_count, id);
}

void repo_set_head(Repository *repo, Commit *commit) {
    if (repo) repo->head = commit;
}

Commit *repo_get_head(Repository *repo) {
    return repo ? repo->head : NULL;
}

void repo_create_branch(Repository *repo, const char *branch_name, Commit *commit) {
    if (!repo || !branch_name) return;
    
    if (repo_branch_exists(repo, branch_name)) {
        repo_update_branch(repo, branch_name, commit);
        return;
    }
    
    ensure_branches_capacity(repo);
    repo->branches[repo->branches_count].name = strdup(branch_name);
    repo->branches[repo->branches_count].commit = commit ? commit : repo->head;
    repo->branches_count++;
}

void repo_delete_branch(Repository *repo, const char *branch_name) {
    if (!repo || !branch_name) return;
    
    if (repo->current_branch_name && strcmp(repo->current_branch_name, branch_name) == 0) {
        printf("Cannot delete current branch '%s'.\n", branch_name);
        return;
    }
    
    for (int i = 0; i < repo->branches_count; i++) {
        if (strcmp(repo->branches[i].name, branch_name) == 0) {
            free(repo->branches[i].name);
            for (int j = i; j < repo->branches_count - 1; j++) {
                repo->branches[j] = repo->branches[j + 1];
            }
            repo->branches_count--;
            return;
        }
    }
}

void repo_checkout_branch(Repository *repo, const char *branch_name) {
    if (!repo || !branch_name) return;
    
    Commit *branch_head = repo_get_branch_head(repo, branch_name);
    if (!branch_head) {
        printf("Branch '%s' does not exist.\n", branch_name);
        return;
    }
    
    free(repo->current_branch_name);
    repo->current_branch_name = strdup(branch_name);
    repo->head = branch_head;
}

Commit *repo_get_branch_head(Repository *repo, const char *branch_name) {
    if (!repo || !branch_name) return NULL;
    
    for (int i = 0; i < repo->branches_count; i++) {
        if (strcmp(repo->branches[i].name, branch_name) == 0) {
            return repo->branches[i].commit;
        }
    }
    return NULL;
}

int repo_branch_exists(Repository *repo, const char *branch_name) {
    if (!repo || !branch_name) return 0;
    
    for (int i = 0; i < repo->branches_count; i++) {
        if (strcmp(repo->branches[i].name, branch_name) == 0) {
            return 1;
        }
    }
    return 0;
}

void repo_update_branch(Repository *repo, const char *branch_name, Commit *commit) {
    if (!repo || !branch_name) return;
    
    for (int i = 0; i < repo->branches_count; i++) {
        if (strcmp(repo->branches[i].name, branch_name) == 0) {
            repo->branches[i].commit = commit;
            return;
        }
    }
    repo_create_branch(repo, branch_name, commit);
}

char **repo_list_branches(Repository *repo, int *count) {
    if (!repo) {
        *count = 0;
        return NULL;
    }
    
    *count = repo->branches_count;
    char **names = (char**)malloc(repo->branches_count * sizeof(char*));
    for (int i = 0; i < repo->branches_count; i++) {
        names[i] = strdup(repo->branches[i].name);
    }
    return names;
}

static void add_changed_path(char ***paths, int *count, int *capacity, const char *path) {
    if (!paths || !count || !capacity || !path) return;

    if (*count >= *capacity) {
        int new_capacity = *capacity == 0 ? 8 : *capacity * 2;
        char **new_paths = (char**)realloc(*paths, new_capacity * sizeof(char*));
        if (!new_paths) return;

        *paths = new_paths;
        *capacity = new_capacity;
    }
    (*paths)[(*count)++] = strdup(path);
}

static void overlay_tree(TreeNode **target_tree, TreeNode *source_node, const char *prefix,
                         char ***changed, int *changed_count, int *changed_capacity) {
    if (!target_tree || !source_node) return;

    char path[1024];
    if (!source_node->name || strlen(source_node->name) == 0) {
        snprintf(path, sizeof(path), "%s", prefix ? prefix : "");
    } else if (prefix && strlen(prefix) > 0) {
        snprintf(path, sizeof(path), "%s/%s", prefix, source_node->name);
    } else {
        snprintf(path, sizeof(path), "%s", source_node->name);
    }

    if (source_node->is_directory) {
        for (int i = 0; i < source_node->children_count; i++) {
            overlay_tree(target_tree, source_node->children[i], path,
                         changed, changed_count, changed_capacity);
        }
        return;
    }

    if (path[0] == '\0' || !source_node->blob) return;

    Blob *blob = create_blob(source_node->blob->content, source_node->blob->size);
    TreeNode *new_tree = copy_tree_with_change(*target_tree, path, blob);
    if (!new_tree) {
        free_blob(blob);
        return;
    }

    free_tree_node(*target_tree);
    *target_tree = new_tree;
    add_changed_path(changed, changed_count, changed_capacity, path);
}

Commit *merge_simple(Repository *repo, Commit *base, Commit *other, const char *message) {
    if (!base || !other) return base;

    TreeNode *merged_tree = copy_tree_with_change(base->root, "", NULL);
    char **changed = NULL;
    int changed_count = 0;
    int changed_capacity = 0;

    overlay_tree(&merged_tree, other->root, "", &changed, &changed_count, &changed_capacity);

    Commit *new_commit = create_commit(
        base,
        merged_tree,
        message && strlen(message) > 0 ? message : "Merge",
        changed,
        changed_count
    );

    for (int i = 0; i < changed_count; i++) {
        free(changed[i]);
    }
    free(changed);

    if (!new_commit) {
        free_tree_node(merged_tree);
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

void repo_print_status(Repository *repo) {
    if (!repo) return;
    
    printf("=== Repository Status ===\n");
    printf("Current branch: %s\n", repo->current_branch_name ? repo->current_branch_name : "(detached)");
    printf("HEAD commit: %s\n", repo->head ? (repo->head->hash ? repo->head->hash : "no hash") : "none");
    printf("Total commits: %d\n", repo->commits_count);
    printf("Branches: ");
    for (int i = 0; i < repo->branches_count; i++) {
        printf("%s ", repo->branches[i].name);
    }
    printf("\n");
}

void free_branches(void) {
    // В нашей реализации ветки хранятся в структуре Repository,
    // которая освобождается через repo_destroy().
    // Эта функция оставлена для совместимости с main.c.
    // Можно оставить пустой или добавить логирование.
    #ifdef DEBUG
    printf("free_branches() called (branches are freed with repo_destroy)\n");
    #endif
}
