#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>

#ifdef _WIN32
    #include <direct.h>
    #include <windows.h>
    #include <dirent.h>
#else
    #include <unistd.h>
#endif

#include "core/types.h"
#include "core/tree.h"
#include "core/commit.h"
#include "core/repo.h"
#include "core/staging.h"
#include "core/repo_storage.h"
#include "utils.h" 

// ================ ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ ================

static Repository *repo = NULL;
static Commit *current_commit = NULL;
static StagingArea *staging = NULL;

// ================ ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ================

static void print_help(void) {
    printf("\n");
    printf("===============================================================================\n");
    printf("                         COO.code Repository Manager                          \n");
    printf("===============================================================================\n");
    printf("\n   Commands:\n");
    printf("   init                          - Initialize repository in current directory\n");
    printf("   init-dir <path>               - Initialize repository in specified path\n");
    printf("   load [path]                   - Load existing repository\n");
    printf("   close                         - Close current repository\n");
    printf("   \n");
    printf("   add <file> <content>          - Add/update file in staging\n");
    printf("   rm <file>                     - Remove file from staging\n");
    printf("   commit <message>              - Create commit from staged changes\n");
    printf("   reset                         - Discard all staging changes\n");
    printf("   status                        - Show staging status\n");
    printf("   \n");
    printf("   cat <file>                    - Show file content\n");
    printf("   exists <file>                 - Check file existence\n");
    printf("   ls                            - List files in current commit\n");
    printf("   pwd                           - Show repository working directory\n");
    printf("   \n");
    printf("   log                           - Show commit history\n");
    printf("   branch <name>                 - Create new branch\n");
    printf("   branches                      - List all branches\n");
    printf("   checkout <branch>             - Switch to branch\n");
    printf("   merge <branch>                - Merge branch into current\n");
    printf("   \n");
    printf("   count                         - Count tree/blob objects\n");
    printf("   help, ?                       - Show this help\n");
    printf("   exit, q                       - Exit\n");
    printf("===============================================================================\n");
}

static void print_status_line(const char *label, const char *value) {
    printf("  %-15s: %s\n", label, value);
}

static char *find_existing_repo(void) {
    char cwd[1024];
    if (!getcwd(cwd, sizeof(cwd))) return NULL;
    
    // Нормализуем путь
    for (char *p = cwd; *p; p++) {
        if (*p == '\\') *p = '/';
    }
    
    char *path = strdup(cwd);
    while (1) {
        char test_path[1024];
        // Используем двойные обратные слеши для Windows или прямые
        snprintf(test_path, sizeof(test_path), "%s/.minigit", path);
        
        // Пробуем открыть как файл (проверяем существование директории)
        DIR *dir = opendir(test_path);
        if (dir) {
            closedir(dir);
            printf("DEBUG: Found repo at: %s\n", path);
            return path;
        }
        
        char *last_slash = strrchr(path, '/');
        if (!last_slash || last_slash == path) break;
        *last_slash = '\0';
    }
    
    free(path);
    return NULL;
}

static void auto_load_repo(void) {
    char *repo_path = repo_load_last_repo();
    if (repo_path && strlen(repo_path) > 0) {
        printf("\n[INFO] Loading saved repository from: %s\n", repo_path);
        repo = repo_load_from_dir(repo_path);
        if (repo) {
            current_commit = repo_get_head(repo);
            staging = staging_create(current_commit);
        } else {
            printf("[WARN] Failed to load saved repository\n");
        }
    } else {
        char *found = find_existing_repo();
        if (found) {
            printf("\n[INFO] Found repository in: %s\n", found);
            repo = repo_load_from_dir(found);
            if (repo) {
                current_commit = repo_get_head(repo);
                staging = staging_create(current_commit);
            }
            free(found);
        }
    }
}

// ================ КОМАНДЫ ================
static void cmd_add_file(char *filename) {
    if (!repo) {
        printf("[ERROR] Repository not initialized. Use 'init' command.\n");
        return;
    }
    
    if (!filename || strlen(filename) == 0) {
        printf("[ERROR] Usage: add <filename>\n");
        return;
    }
    
    // Read file content from disk
    size_t file_size;
    char *content = read_text_file(filename, &file_size);
    if (!content) {
        printf("[ERROR] File '%s' not found or cannot be read\n", filename);
        return;
    }
    
    staging_add_file(staging, filename, content);
    free(content);
    
    printf("[OK] File '%s' added to staging\n", filename);
}

static void cmd_close(void) {
    if (repo) {
        // Save state before closing
        repo_save_state(repo);
        
        // Clear staging
        if (staging) {
            staging_destroy(staging);
            staging = NULL;
        }
        
        // Clear repository
        free_branches();
        repo_destroy(repo);
        repo = NULL;
        current_commit = NULL;
        
        printf("[OK] Repository closed.\n");
    } else {
        printf("[WARN] No open repository.\n");
    }
}

static void cmd_load(char *args) {
    if (repo) {
        printf("[ERROR] Repository already loaded!\n");
        return;
    }
    
    char *dir = args ? args : ".";
    
    repo = repo_load_from_dir(dir);
    if (!repo) return;
    
    current_commit = repo_get_head(repo);
    staging = staging_create(current_commit);
}

static void cmd_init_in_dir(char *args) {
    if (repo) {
        printf("[ERROR] Repository already loaded!\n");
        printf("   First close it (close), then re-enter\n");
        return;
    }
    
    char *dir = args ? args : ".";
    if (dir[0] == '"' || dir[0] == '\'') {
        char quote = dir[0];
        dir++;
        int len = strlen(dir);
        if (len > 0 && dir[len-1] == quote) {
            dir[len-1] = '\0';
        }
    }
    
    repo = repo_init_in_dir(dir);
    if (!repo) return;
    
    current_commit = repo_get_head(repo);
    if (!current_commit) {
        current_commit = init_repo();
        repo_add_commit(repo, current_commit);
        repo_set_head(repo, current_commit);
        repo_update_branch(repo, "master", current_commit);
        
        repo_save_state(repo);
    }
    
    staging = staging_create(current_commit);
    
    printf("[OK] Working directory: %s\n", repo_get_workdir(repo));
}

static void cmd_debug(void) {
    if (!current_commit) {
        printf("[ERROR] No commits\n");
        return;
    }
    
    printf("\n[DEBUG] TREE STRUCTURE:\n");
    printf("Root node: name='%s', is_dir=%d, children_count=%d\n",
           current_commit->root->name,
           current_commit->root->is_directory,
           current_commit->root->children_count);
    
    for (int i = 0; i < current_commit->root->children_count; i++) {
        TreeNode *child = current_commit->root->children[i];
        printf("  Child %d: name='%s', is_dir=%d\n", 
               i, child->name, child->is_directory);
        if (!child->is_directory && child->blob) {
            printf("    blob: size=%zu, hash=%s\n", 
                   child->blob->size,
                   child->blob->hash ? child->blob->hash : "NULL");
        }
    }
}
static void cmd_init(void) {
    if (repo) {
        printf("[ERROR] Repository already initialized!\n");
        return;
    }
    
    repo = repo_create();
    current_commit = init_repo();
    repo_add_commit(repo, current_commit);
    repo_set_head(repo, current_commit);
    repo_update_branch(repo, "master", current_commit);
    
    // Create staging
    staging = staging_create(current_commit);
    
    printf("[OK] Repository initialized!\n");
    print_status_line("Initial commit ID", "1");
    print_status_line("Current branch", "master");
    print_status_line("Commit hash", current_commit->hash ? current_commit->hash : "calculating...");
}

static void cmd_add(char *args) {
    if (!repo) {
        printf("[ERROR] Repository not initialized. Use 'init' command.\n");
        return;
    }
    
    if (!args || strlen(args) == 0) {
        printf("[ERROR] Usage: add <filename> <content>\n");
        printf("   Example: add hello.txt \"Hello, World!\"\n");
        return;
    }
    
    // Parse: add <file> <content>
    char *file = strtok(args, " ");
    char *content = strtok(NULL, "");
    
    if (!file || !content) {
        printf("[ERROR] Usage: add <filename> <content>\n");
        return;
    }
    
    // Remove quotes if present
    if (content[0] == '"' || content[0] == '\'') {
        char quote = content[0];
        content++;
        int len = strlen(content);
        if (len > 0 && content[len-1] == quote) {
            content[len-1] = '\0';
        }
    }
    
    staging_add_file(staging, file, content);
    printf("[OK] File '%s' added to staging (not yet committed)\n", file);
}

static void cmd_rm(char *file) {
    if (!repo) {
        printf("[ERROR] Repository not initialized. Use 'init' command.\n");
        return;
    }
    
    if (!file || strlen(file) == 0) {
        printf("[ERROR] Usage: rm <filename>\n");
        return;
    }
    
    staging_remove_file(staging, file);
}

static void cmd_commit(char *message) {
    if (!repo) {
        printf("[ERROR] Repository not initialized. Use 'init' command.\n");
        return;
    }
    
    if (!message || strlen(message) == 0) {
        printf("[ERROR] Usage: commit <message>\n");
        return;
    }
    
    if (!staging_has_changes(staging)) {
        printf("[ERROR] No changes to commit. Use 'add' to add files\n");
        return;
    }
    
    current_commit = staging_commit(repo, staging, message);
    if (current_commit) {
        printf("[OK] Commit %d created: '%s'\n", current_commit->id, message);
        printf("   Hash: %s\n", current_commit->hash ? current_commit->hash : "calculating...");
    }
}

static void cmd_status(void) {
    if (!repo) {
        printf("[ERROR] Repository not initialized\n");
        return;
    }
    
    printf("\n[STATUS] Current state\n");
    print_status_line("Current branch", repo->current_branch_name ? repo->current_branch_name : "(detached)");
    print_status_line("Current commit", current_commit ? (current_commit->hash ? current_commit->hash : "none") : "none");
    char id_buf[32];
    if (current_commit) {
        snprintf(id_buf, sizeof(id_buf), "%d", current_commit->id);
        print_status_line("Commit ID", id_buf);
    } else {
        print_status_line("Commit ID", "none");
    }
    
    printf("\n[STAGING] Changes (%d):\n", staging ? staging->changed_files_count : 0);
    if (staging && staging->changed_files_count > 0) {
        for (int i = 0; i < staging->changed_files_count; i++) {
            printf("   - %s\n", staging->changed_files[i]);
        }
    } else {
        printf("   (no changes)\n");
    }
    
    printf("\n[BRANCHES] Available: ");
    int count;
    char **branches = repo_list_branches(repo, &count);
    for (int i = 0; i < count; i++) {
        printf("%s ", branches[i]);
        free(branches[i]);
    }
    free(branches);
    printf("\n");
}

static void cmd_cat(char *file) {
    if (!current_commit) {
        printf("[ERROR] No commits\n");
        return;
    }
    
    if (!file || strlen(file) == 0) {
        printf("[ERROR] Usage: cat <filename>\n");
        return;
    }
    
    char *content = get_file_content(current_commit, file);
    if (content) {
        printf("------------------------------------------------------------\n");
        printf("%s\n", content);
        printf("------------------------------------------------------------\n");
        free(content);
    } else {
        printf("[ERROR] File '%s' not found in current commit\n", file);
    }
}

static void cmd_exists(char *file) {
    if (!current_commit) {
        printf("[ERROR] No commits\n");
        return;
    }
    
    if (!file || strlen(file) == 0) {
        printf("[ERROR] Usage: exists <filename>\n");
        return;
    }
    
    int exists = get_file_exists(current_commit, file);
    printf("[INFO] File '%s': %s\n", file, exists ? "[FOUND]" : "[NOT FOUND]");
}

static void cmd_ls(void) {
    if (!current_commit) {
        printf("[ERROR] No commits\n");
        return;
    }
    
    print_files(current_commit);
}

static void cmd_log(void) {
    if (!current_commit) {
        printf("[ERROR] No commits\n");
        return;
    }
    
    print_history(current_commit);
}

static void cmd_branch(char *name) {
    if (!repo) {
        printf("[ERROR] Repository not initialized\n");
        return;
    }
    
    if (!name || strlen(name) == 0) {
        printf("[ERROR] Usage: branch <branch_name>\n");
        return;
    }
    
    repo_create_branch(repo, name, current_commit);
    repo_save_state(repo);
    
    printf("[OK] Branch '%s' created (points to commit %d)\n", name, current_commit->id);
}

static void cmd_checkout(char *name) {
    if (!repo) {
        printf("[ERROR] Repository not initialized\n");
        return;
    }
    
    if (!name || strlen(name) == 0) {
        printf("[ERROR] Usage: checkout <branch_name>\n");
        return;
    }
    
    if (staging_has_changes(staging)) {
        printf("[WARN] You have unsaved changes in staging.\n");
        printf("   First commit or reset changes.\n");
        return;
    }
    
    if (!repo_branch_exists(repo, name)) {
        printf("[ERROR] Branch '%s' does not exist.\n", name);
        return;
    }

    repo_checkout_branch(repo, name);
    current_commit = repo_get_head(repo);
    
    staging_destroy(staging);
    staging = staging_create(current_commit);
    
    repo_save_state(repo);
    
    printf("[OK] Switched to branch '%s', commit %d\n", name, current_commit->id);
}

static void cmd_branches(void) {
    if (!repo) {
        printf("[ERROR] Repository not initialized\n");
        return;
    }
    
    int count;
    char **branches = repo_list_branches(repo, &count);
    
    printf("\n[BRANCHES] Repository branches:\n");
    for (int i = 0; i < count; i++) {
        printf("   - %s %s\n", branches[i],
               (repo->current_branch_name && strcmp(repo->current_branch_name, branches[i]) == 0) ? "[CURRENT]" : "");
        free(branches[i]);
    }
    free(branches);
    printf("\n");
}

static void cmd_count_objects(void) {
    if (!current_commit) {
        printf("No commits\n");
        return;
    }

    int tree_count = 0;
    int blob_count = 0;
    int total = count_objects(current_commit, &tree_count, &blob_count);
    printf("Objects: %d total (%d tree, %d blob)\n", total, tree_count, blob_count);
}

static void cmd_merge(char *branch_name) {
    if (!repo || !current_commit) {
        printf("Repository is not initialized\n");
        return;
    }

    if (!branch_name || strlen(branch_name) == 0) {
        printf("Usage: merge <branch>\n");
        return;
    }

    Commit *other = repo_get_branch_head(repo, branch_name);
    if (!other) {
        printf("Branch '%s' does not exist.\n", branch_name);
        return;
    }

    char message[512];
    snprintf(message, sizeof(message), "Merge branch %s", branch_name);
    current_commit = merge_simple(repo, current_commit, other, message);

    staging_destroy(staging);
    staging = staging_create(current_commit);

    printf("Merged branch '%s' into '%s' as commit %d\n",
           branch_name,
           repo->current_branch_name ? repo->current_branch_name : "(detached)",
           current_commit ? current_commit->id : 0);
}

static void cmd_reset(void) {
    if (!repo) {
        printf("[ERROR] Repository not initialized\n");
        return;
    }
    
    if (!staging_has_changes(staging)) {
        printf("[INFO] No changes to reset\n");
        return;
    }
    
    // Destroy old staging and create new one
    staging_destroy(staging);
    staging = staging_create(current_commit);
    
    printf("[OK] All staging changes discarded\n");
}

// ================ MAIN FUNCTION ================

int main(void) {
    // Setup Russian locale
    #ifdef _WIN32
        SetConsoleOutputCP(65001);
        SetConsoleCP(65001);
        system("chcp 65001 > nul");
    #else
        setlocale(LC_ALL, "ru_RU.UTF-8");
    #endif
    
    auto_load_repo();
    
    // ANSI color codes
    const char *BOLD = "\x1b[1m";
    const char *CYAN = "\x1b[36m";
    const char *GREEN = "\x1b[32m";
    const char *YELLOW = "\x1b[33m";
    const char *RESET = "\x1b[0m";
    
    printf("\n");
    printf("%s===============================================================================%s\n", CYAN, RESET);
    printf("%s                  %sCOO.code%s %s- Repository Version Control System%s\n", GREEN, BOLD, RESET, CYAN, RESET);
    printf("%s===============================================================================%s\n", CYAN, RESET);
    printf("\n   %sPersistent version control:%s add -> staging -> commit\n", YELLOW, RESET);
    printf("   %sSupports:%s branches, history, persistence across sessions\n\n", YELLOW, RESET);
    
    print_help();
    
    char input[8192];
    
    while (1) {
        printf("\n%scoo>%s ", GREEN, RESET);
        fflush(stdout);
        
        if (!fgets(input, sizeof(input), stdin)) {
            break;
        }
        
        // Remove newline
        input[strcspn(input, "\n")] = 0;
        if ((unsigned char)input[0] == 0xEF &&
            (unsigned char)input[1] == 0xBB &&
            (unsigned char)input[2] == 0xBF) {
            memmove(input, input + 3, strlen(input + 3) + 1);
        }
        
        if (strlen(input) == 0) {
            continue;
        }
        
        // Parse command
        char cmd[256];
        char *args;
        
        strcpy(cmd, input);
        args = strchr(cmd, ' ');
        if (args) {
            *args = '\0';
            args++;
        }
        
        if (strcmp(cmd, "init") == 0) {
            cmd_init();
        }
        else if (strcmp(cmd, "add") == 0) {
            cmd_add(args);
        }
        else if (strcmp(cmd, "rm") == 0) {
            cmd_rm(args);
        }
        else if (strcmp(cmd, "commit") == 0) {
            cmd_commit(args);
        }
        else if (strcmp(cmd, "status") == 0) {
            cmd_status();
        }
        else if (strcmp(cmd, "cat") == 0) {
            cmd_cat(args);
        }
        else if (strcmp(cmd, "exists") == 0) {
            cmd_exists(args);
        }
        else if (strcmp(cmd, "ls") == 0) {
            cmd_ls();
        }
        else if (strcmp(cmd, "log") == 0) {
            cmd_log();
        }
        else if (strcmp(cmd, "branch") == 0) {
            cmd_branch(args);
        }
        else if (strcmp(cmd, "checkout") == 0) {
            cmd_checkout(args);
        }
        else if (strcmp(cmd, "branches") == 0) {
            cmd_branches();
        }
        else if (strcmp(cmd, "merge") == 0) {
            cmd_merge(args);
        }
        else if (strcmp(cmd, "count") == 0 || strcmp(cmd, "objects") == 0) {
            cmd_count_objects();
        }
        else if (strcmp(cmd, "reset") == 0) {
            cmd_reset();
        }
        else if (strcmp(cmd, "help") == 0 || strcmp(cmd, "?") == 0) {
            print_help();
        }
        else if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "q") == 0) {
            if (repo) {
                repo_save_state(repo);
            }
            printf("[INFO] Goodbye!\n");
            break;
        }
        else if (strcmp(cmd, "debug") == 0) {
            cmd_debug();
        }
        else if (strcmp(cmd, "init-dir") == 0) {
            cmd_init_in_dir(args);
        }
        else if (strcmp(cmd, "load") == 0) {
            cmd_load(args);
        }
        else if (strcmp(cmd, "close") == 0) {
            cmd_close();
        }
        else if (strcmp(cmd, "add-file") == 0) {
            cmd_add_file(args);
        }
        else if (strcmp(cmd, "pwd") == 0) {
            if (repo) {
                printf("[INFO] Repository directory: %s\n", repo_get_workdir(repo));
            } else {
                char cwd_buf[1024];
                if (getcwd(cwd_buf, sizeof(cwd_buf))) {
                    printf("[INFO] Current directory: %s\n", cwd_buf);
                } else {
                    printf("[INFO] Current directory: (unknown)\n");
                }
            }
        }
        else {
            printf("[ERROR] Unknown command: '%s'\n", cmd);
            printf("   Type 'help' for list of commands\n");
        }
    }
    
    // Cleanup
    if (staging) {
        staging_destroy(staging);
    }
    if (repo) {
        free_branches();
        repo_destroy(repo);
    }
    
    return 0;
}
