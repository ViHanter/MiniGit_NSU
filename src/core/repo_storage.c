#include "repo_storage.h"
#include "../utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#include <wchar.h>
#else
#include <sys/stat.h>
#include <dirent.h>
#endif

#ifdef _WIN32
static wchar_t *utf8_to_wide(const char *text) {
    if (!text) return NULL;

    int needed = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
    if (needed <= 0) return NULL;

    wchar_t *wide = (wchar_t*)malloc((size_t)needed * sizeof(wchar_t));
    if (!wide) return NULL;

    if (MultiByteToWideChar(CP_UTF8, 0, text, -1, wide, needed) <= 0) {
        free(wide);
        return NULL;
    }
    return wide;
}

static int storage_mkdir(const char *path) {
    wchar_t *wide_path = utf8_to_wide(path);
    if (!wide_path) return -1;

    int result = _wmkdir(wide_path);
    free(wide_path);
    return result;
}

static FILE *storage_fopen(const char *path, const char *mode) {
    wchar_t *wide_path = utf8_to_wide(path);
    wchar_t *wide_mode = utf8_to_wide(mode);
    if (!wide_path || !wide_mode) {
        free(wide_path);
        free(wide_mode);
        return NULL;
    }

    FILE *file = _wfopen(wide_path, wide_mode);
    free(wide_path);
    free(wide_mode);
    return file;
}

#define MKDIR(path) storage_mkdir(path)
#define FOPEN(path, mode) storage_fopen(path, mode)
#else
#define MKDIR(path) mkdir(path, 0777)
#define FOPEN(path, mode) fopen(path, mode)
#endif

static char *repo_workdir = NULL;

// Прототипы
static void save_tree_node(TreeNode *node, const char *base_path, const char *parent_path);
static TreeNode *load_tree_node(const char *base_path, const char *node_path);
static int save_commit_info(Commit *commit, const char *base_path);
static Commit *load_commit_info(int id, const char *base_path);
static int load_commit_tree(Commit *commit, const char *base_path);

// ================ СОХРАНЕНИЕ ДЕРЕВА ================

static void save_tree_node(TreeNode *node, const char *base_path, const char *parent_path) {
    if (!node) return;
    
    char node_path[1024];
    if (parent_path && strlen(parent_path) > 0) {
        snprintf(node_path, sizeof(node_path), "%s/%s", parent_path, node->name);
    } else {
        snprintf(node_path, sizeof(node_path), "%s", node->name);
    }
    
    // Создаём директорию для trees
    char trees_dir[1024];
    snprintf(trees_dir, sizeof(trees_dir), "%s/.minigit/trees", base_path ? base_path : ".");
    if (MKDIR(trees_dir) != 0 && errno != EEXIST) return;
    
    if (node->is_directory) {
        char dir_path[1024];
        snprintf(dir_path, sizeof(dir_path), "%s/.minigit/trees/%s.tree", 
                 base_path ? base_path : ".", node_path);
        
        FILE *f = FOPEN(dir_path, "w");
        if (f) {
            fprintf(f, "name=%s\n", node->name);
            fprintf(f, "is_directory=1\n");
            fprintf(f, "children_count=%d\n", node->children_count);
            for (int i = 0; i < node->children_count; i++) {
                fprintf(f, "child=%s\n", node->children[i]->name);
            }
            fclose(f);
        }
        
        for (int i = 0; i < node->children_count; i++) {
            save_tree_node(node->children[i], base_path, node_path);
        }
    } else {
        char file_path[1024];
        snprintf(file_path, sizeof(file_path), "%s/.minigit/trees/%s.file", 
                 base_path ? base_path : ".", node_path);
        
        FILE *f = FOPEN(file_path, "w");
        if (f) {
            fprintf(f, "name=%s\n", node->name);
            fprintf(f, "is_directory=0\n");
            fprintf(f, "blob_hash=%s\n", node->blob && node->blob->hash ? node->blob->hash : "");
            fclose(f);
        }
    }
}

// ================ ЗАГРУЗКА ДЕРЕВА ================

static TreeNode *load_tree_node(const char *base_path, const char *node_path) {
    char meta_path[1024];
    snprintf(meta_path, sizeof(meta_path), "%s/.minigit/trees/%s.tree", 
             base_path ? base_path : ".", node_path);
    
    FILE *f = FOPEN(meta_path, "r");
    if (!f) {
        // Пробуем как файл
        snprintf(meta_path, sizeof(meta_path), "%s/.minigit/trees/%s.file", 
                 base_path ? base_path : ".", node_path);
        f = FOPEN(meta_path, "r");
        if (!f) return NULL;
        
        // Загружаем файл
        char name[256] = "";
        char blob_hash[256] = "";
        char line[1024];
        
        while (fgets(line, sizeof(line), f)) {
            char *key = strtok(line, "=");
            char *value = strtok(NULL, "\n");
            if (!key || !value) continue;
            
            if (strcmp(key, "name") == 0) {
                strncpy(name, value, 255);
            } else if (strcmp(key, "blob_hash") == 0) {
                strncpy(blob_hash, value, 255);
            }
        }
        fclose(f);
        
        if (strlen(blob_hash) == 0) return NULL;
        
        // Загружаем blob из хранилища
        char blob_path[1024];
        snprintf(blob_path, sizeof(blob_path), "%s/.minigit/objects/%s.blob", 
                 base_path ? base_path : ".", blob_hash);
        
        size_t blob_size;
        char *blob_content = read_text_file(blob_path, &blob_size);
        if (!blob_content) return NULL;
        
        Blob *blob = create_blob(blob_content, blob_size);
        free(blob_content);
        
        TreeNode *node = (TreeNode*)malloc(sizeof(TreeNode));
        if (!node) {
            free_blob(blob);
            return NULL;
        }
        
        node->name = strdup(name);
        node->is_directory = 0;
        node->blob = blob;
        node->children = NULL;
        node->children_count = 0;
        node->ref_count = 1;
        
        return node;
    }
    
    // Загружаем директорию
    char name[256] = "";
    int children_count = 0;
    char **children_names = NULL;
    char line[1024];
    
    while (fgets(line, sizeof(line), f)) {
        char *key = strtok(line, "=");
        char *value = strtok(NULL, "\n");
        if (!key || !value) continue;
        
        if (strcmp(key, "name") == 0) {
            strncpy(name, value, 255);
        } else if (strcmp(key, "children_count") == 0) {
            children_count = atoi(value);
            if (children_count > 0) {
                children_names = (char**)malloc(children_count * sizeof(char*));
                for (int i = 0; i < children_count; i++) children_names[i] = NULL;
            }
        } else if (strcmp(key, "child") == 0 && children_names) {
            for (int i = 0; i < children_count; i++) {
                if (!children_names[i]) {
                    children_names[i] = strdup(value);
                    break;
                }
            }
        }
    }
    fclose(f);
    
    TreeNode *node = create_directory_node(name);
    if (!node) {
        if (children_names) {
            for (int i = 0; i < children_count; i++) free(children_names[i]);
            free(children_names);
        }
        return NULL;
    }
    
    // Загружаем детей
    for (int i = 0; i < children_count; i++) {
        if (!children_names[i]) continue;
        
        char child_path[1024];
        if (strlen(node_path) > 0 && strcmp(node_path, "") != 0) {
            snprintf(child_path, sizeof(child_path), "%s/%s", node_path, children_names[i]);
        } else {
            snprintf(child_path, sizeof(child_path), "%s", children_names[i]);
        }
        
        TreeNode *child = load_tree_node(base_path, child_path);
        if (child) {
            append_child(node, child);
        }
        free(children_names[i]);
    }
    free(children_names);
    
    return node;
}

// ================ СОХРАНЕНИЕ КОММИТА ================

static int save_commit_info(Commit *commit, const char *base_path) {
    if (!commit) return 0;
    
    char path[1024];
    snprintf(path, sizeof(path), "%s/.minigit/commits/%d.commit", 
             base_path ? base_path : ".", commit->id);
    
    // Создаём директорию если нужно
    char dir[1024];
    snprintf(dir, sizeof(dir), "%s/.minigit/commits", base_path ? base_path : ".");
    if (MKDIR(dir) != 0 && errno != EEXIST) return 0;
    
    FILE *f = FOPEN(path, "w");
    if (!f) return 0;
    
    fprintf(f, "id=%d\n", commit->id);
    fprintf(f, "message=%s\n", commit->message ? commit->message : "");
    fprintf(f, "timestamp=%ld\n", (long)commit->timestamp);
    fprintf(f, "parent_id=%d\n", commit->parent ? commit->parent->id : 0);
    fprintf(f, "hash=%s\n", commit->hash ? commit->hash : "");
    fprintf(f, "changed_files_count=%d\n", commit->changed_files_count);
    for (int i = 0; i < commit->changed_files_count; i++) {
        fprintf(f, "changed_file=%s\n", commit->changed_files[i]);
    }
    
    fclose(f);
    
    // Сохраняем дерево
    save_tree_node(commit->root, base_path, "");
    
    return 1;
}

// ================ ЗАГРУЗКА КОММИТА ================

static int load_commit_tree(Commit *commit, const char *base_path) {
    if (!commit) return 0;
    
    TreeNode *root = load_tree_node(base_path, "");
    if (root) {
        if (commit->root) {
            free_tree_node(commit->root);
        }
        commit->root = root;
        return 1;
    }
    
    commit->root = create_empty_tree();
    return 0;
}

static Commit *load_commit_info(int id, const char *base_path) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/.minigit/commits/%d.commit", 
             base_path ? base_path : ".", id);
    
    FILE *f = FOPEN(path, "r");
    if (!f) return NULL;
    
    Commit *commit = (Commit*)calloc(1, sizeof(Commit));
    if (!commit) {
        fclose(f);
        return NULL;
    }
    
    commit->children = NULL;
    commit->children_count = 0;
    commit->children_capacity = 0;
    commit->root = create_empty_tree();
    
    char line[1024];
    int changed_count = 0;
    char **changed_files_temp = NULL;
    
    while (fgets(line, sizeof(line), f)) {
        char *key = strtok(line, "=");
        char *value = strtok(NULL, "\n");
        if (!key || !value) continue;
        
        if (strcmp(key, "id") == 0) {
            commit->id = atoi(value);
            commit_note_loaded_id(commit->id);
        } else if (strcmp(key, "message") == 0) {
            commit->message = strdup(value);
        } else if (strcmp(key, "timestamp") == 0) {
            commit->timestamp = (time_t)atol(value);
        } else if (strcmp(key, "parent_id") == 0) {
            int parent_id = atoi(value);
            if (parent_id > 0) {
                commit->parent = (Commit*)(intptr_t)parent_id;
            }
        } else if (strcmp(key, "hash") == 0) {
            commit->hash = strdup(value);
        } else if (strcmp(key, "changed_files_count") == 0) {
            commit->changed_files_count = atoi(value);
            if (commit->changed_files_count > 0) {
                changed_files_temp = (char**)malloc(commit->changed_files_count * sizeof(char*));
            }
        } else if (strcmp(key, "changed_file") == 0 && changed_files_temp && changed_count < commit->changed_files_count) {
            changed_files_temp[changed_count++] = strdup(value);
        }
    }
    fclose(f);
    
    if (changed_files_temp) {
        commit->changed_files = changed_files_temp;
        commit->changed_files_count = changed_count;
    }
    
    // Восстанавливаем дерево
    load_commit_tree(commit, base_path);
    
    return commit;
}

// ================ ПУБЛИЧНЫЕ ФУНКЦИИ ================

const char *repo_get_workdir(Repository *repo) {
    (void)repo;
    return repo_workdir ? repo_workdir : ".";
}

void repo_set_workdir(Repository *repo, const char *path) {
    (void)repo;
    if (repo_workdir) {
        free(repo_workdir);
    }
    repo_workdir = path ? strdup(path) : NULL;
    set_minigit_storage_base(repo_workdir);
}

static int ensure_minigit_dir(const char *base_path) {
    char path[1024];
    const char *base = base_path && strlen(base_path) > 0 ? base_path : ".";

    if (strcmp(base, ".") != 0) {
        char base_copy[1024];
        snprintf(base_copy, sizeof(base_copy), "%s", base);
        normalize_path(base_copy);

        for (char *p = base_copy; *p; p++) {
            if (*p != '/') continue;
            if (p == base_copy || (p == base_copy + 2 && base_copy[1] == ':')) continue;

            *p = '\0';
            if (MKDIR(base_copy) != 0 && errno != EEXIST) {
                return 0;
            }
            *p = '/';
        }

        if (MKDIR(base_copy) != 0 && errno != EEXIST) {
            return 0;
        }
    }
    
    snprintf(path, sizeof(path), "%s/.minigit", base);
    if (MKDIR(path) != 0 && errno != EEXIST) return 0;
    
    snprintf(path, sizeof(path), "%s/.minigit/objects", base);
    if (MKDIR(path) != 0 && errno != EEXIST) return 0;
    
    snprintf(path, sizeof(path), "%s/.minigit/refs", base);
    if (MKDIR(path) != 0 && errno != EEXIST) return 0;
    
    return 1;
}

static void repo_link_commits(Repository *repo) {
    if (!repo) return;
    
    for (int i = 0; i < repo->commits_count; i++) {
        Commit *commit = repo->all_commits[i];
        if (commit->parent && (intptr_t)commit->parent > 0) {
            int parent_id = (int)(intptr_t)commit->parent;
            Commit *parent = repo_find_commit_by_id(repo, parent_id);
            commit->parent = parent;
            if (parent) {
                add_child_commit(parent, commit);
            }
        }
    }
}

Repository *repo_init_in_dir(const char *path) {
    if (!ensure_minigit_dir(path)) {
        printf("[ERROR] Cannot initialize repository in '%s'\n", path ? path : ".");
        return NULL;
    }
    
    Repository *repo = repo_create();
    if (!repo) return NULL;
    
    repo_set_workdir(repo, path);
    repo_save_state(repo);
    repo_save_last_repo(path);
    
    printf("[OK] Repository initialized in '%s'\n", path ? path : ".");
    return repo;
}

Repository *repo_load_from_dir(const char *path) {
    char minigit_path[1024];
    snprintf(minigit_path, sizeof(minigit_path), "%s/.minigit", path ? path : ".");

    // Проверяем существование директории .minigit
    #ifdef _WIN32
        wchar_t *wide_minigit_path = utf8_to_wide(minigit_path);
        DWORD attrs = wide_minigit_path ? GetFileAttributesW(wide_minigit_path) : INVALID_FILE_ATTRIBUTES;
        free(wide_minigit_path);
        if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            printf("[ERROR] Not a minigit repository: %s\n", path ? path : ".");
            return NULL;
        }
    #else
        struct stat st;
        if (stat(minigit_path, &st) != 0 || !S_ISDIR(st.st_mode)) {
            printf("[ERROR] Not a minigit repository: %s\n", path ? path : ".");
            return NULL;
        }
    #endif
    
    // Загружаем HEAD
    char head_path[1024];
    snprintf(head_path, sizeof(head_path), "%s/.minigit/HEAD", path ? path : ".");
    
    FILE *f = FOPEN(head_path, "r");
    if (!f) {
        printf("[WARN] Repository exists but appears corrupted\n");
        return NULL;
    }
    
    int head_id = 0;
    char branch_name[256] = "master";
    if (fscanf(f, "head_id=%d\nbranch=%255s", &head_id, branch_name) != 2) {
        rewind(f);
        char line[512];
        while (fgets(line, sizeof(line), f)) {
            if (sscanf(line, "head_id=%d", &head_id) == 1) continue;
            if (sscanf(line, "branch=%255s", branch_name) == 1) continue;
        }
    }
    fclose(f);
    
    Repository *repo = repo_create();
    if (!repo) return NULL;
    
    repo_set_workdir(repo, path);
    
    // Загружаем все коммиты
    char commits_dir[1024];
    snprintf(commits_dir, sizeof(commits_dir), "%s/.minigit/commits", path ? path : ".");
    
    #ifdef _WIN32
    WIN32_FIND_DATAW findData;
    char search_path[1024];
    snprintf(search_path, sizeof(search_path), "%s\\*.commit", commits_dir);
    wchar_t *wide_search_path = utf8_to_wide(search_path);
    HANDLE hFind = wide_search_path ? FindFirstFileW(wide_search_path, &findData) : INVALID_HANDLE_VALUE;
    free(wide_search_path);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            int id = 0;
            if (swscanf(findData.cFileName, L"%d.commit", &id) == 1) {
                Commit *commit = load_commit_info(id, path);
                if (commit) {
                    repo_add_commit(repo, commit);
                }
            }
        } while (FindNextFileW(hFind, &findData));
        FindClose(hFind);
    }
    #else
    DIR *dir = opendir(commits_dir);
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            int id = 0;
            if (sscanf(entry->d_name, "%d.commit", &id) == 1) {
                Commit *commit = load_commit_info(id, path);
                if (commit) {
                    repo_add_commit(repo, commit);
                }
            }
        }
        closedir(dir);
    }
    #endif
    
    // Связываем parent/children
    repo_link_commits(repo);
    
    // Загружаем HEAD
    Commit *head_commit = repo_find_commit_by_id(repo, head_id);
    if (head_commit) {
        repo_set_head(repo, head_commit);
    }
    
    // Загружаем ветки
    char branches_path[1024];
    snprintf(branches_path, sizeof(branches_path), "%s/.minigit/branches", path ? path : ".");
    f = FOPEN(branches_path, "r");
    if (f) {
        char line[512];
        while (fgets(line, sizeof(line), f)) {
            char bname[256];
            int bid;
            if (sscanf(line, "%255s %d", bname, &bid) == 2) {
                Commit *bcommit = repo_find_commit_by_id(repo, bid);
                if (bcommit) {
                    repo_create_branch(repo, bname, bcommit);
                }
            }
        }
        fclose(f);
    }
    
    free(repo->current_branch_name);
    repo->current_branch_name = strdup(branch_name);
    repo_save_last_repo(path);
    
    printf("[OK] Repository loaded from '%s'\n", path ? path : ".");
    printf("   HEAD: commit %d on branch '%s'\n", head_id, branch_name);
    printf("   Total commits: %d\n", repo->commits_count);
    
    return repo;
}

// ================ CONFIG FUNCTIONS ================

static char *config_file_path = NULL;

static const char *get_config_file(void) {
    if (config_file_path) return config_file_path;
    
    #ifdef _WIN32
    const char *home = getenv("USERPROFILE");
    #else
    const char *home = getenv("HOME");
    #endif
    
    if (!home) home = ".";
    
    static char path[1024];
    snprintf(path, sizeof(path), "%s/.coo_code_config", home);
    
    config_file_path = path;
    return path;
}

void repo_save_last_repo(const char *path) {
    if (!path) return;
    
    const char *config = get_config_file();
    FILE *f = FOPEN(config, "w");
    if (f) {
        fprintf(f, "%s\n", path);
        fclose(f);
    }
}

char *repo_load_last_repo(void) {
    const char *config = get_config_file();
    FILE *f = FOPEN(config, "r");
    if (!f) return NULL;
    
    static char path[1024];
    if (fgets(path, sizeof(path), f)) {
        path[strcspn(path, "\n")] = 0;
        fclose(f);
        if (strlen(path) > 0) {
            return path;
        }
    }
    fclose(f);
    return NULL;
}

int repo_save_state(Repository *repo) {
    if (!repo) return 0;
    
    const char *base_path = repo_get_workdir(repo);
    
    // Сохраняем HEAD
    char head_path[1024];
    snprintf(head_path, sizeof(head_path), "%s/.minigit/HEAD", base_path);
    FILE *f = FOPEN(head_path, "w");
    if (!f) return 0;
    
    fprintf(f, "head_id=%d\nbranch=%s\n", 
            repo->head ? repo->head->id : 0,
            repo->current_branch_name ? repo->current_branch_name : "master");
    fclose(f);
    
    // Создаём директории
    char trees_dir[1024];
    snprintf(trees_dir, sizeof(trees_dir), "%s/.minigit/trees", base_path);
    if (MKDIR(trees_dir) != 0 && errno != EEXIST) return 0;
    
    // Сохраняем все коммиты
    for (int i = 0; i < repo->commits_count; i++) {
        save_commit_info(repo->all_commits[i], base_path);
    }
    
    // Сохраняем ветки
    char branches_path[1024];
    snprintf(branches_path, sizeof(branches_path), "%s/.minigit/branches", base_path);
    f = FOPEN(branches_path, "w");
    if (f) {
        for (int i = 0; i < repo->branches_count; i++) {
            fprintf(f, "%s %d\n", 
                    repo->branches[i].name,
                    repo->branches[i].commit ? repo->branches[i].commit->id : 0);
        }
        fclose(f);
    }
    
    return 1;
}
