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

// ================ ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ ================

static Repository *repo = NULL;
static Commit *current_commit = NULL;
static StagingArea *staging = NULL;

// ================ ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ================

static void print_help(void) {
    printf("\n");
    printf("╔═════════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                           MiniGit Shell                                         ║\n");
    printf("╠═════════════════════════════════════════════════════════════════════════════════╣\n");
    printf("║   Команды:                                                                      ║\n");
    printf("║   init                          - инициализировать репозиторий                  ║\n");
    printf("║   add <файл> <содержимое>       - добавить файл в staging                       ║\n");
    printf("║   rm <файл>                     - удалить файл из staging                       ║\n");
    printf("║   commit <сообщение>            - создать коммит из staging                     ║\n");
    printf("║   status                        - показать статус staging                       ║\n");
    printf("║   cat <файл>                    - показать содержимое файла                     ║\n");
    printf("║   exists <файл>                 - проверить существование файла                 ║\n");
    printf("║   ls                            - список файлов в текущем коммите               ║\n");
    printf("║   log                           - история коммитов                              ║\n");
    printf("║   branch <имя>                  - создать ветку                                 ║\n");
    printf("║   checkout <ветка>              - переключиться на ветку                        ║\n");
    printf("║   branches                      - показать все ветки                            ║\n");
    printf("║   merge <ветка>                 - простое слияние ветки                         ║\n");
    printf("║   count                         - посчитать tree/blob объекты                   ║\n");
    printf("║   reset                         - отменить все изменения в staging              ║\n");
    printf("║   init                          - инициализировать репозиторий в текущей папке  ║\n");
    printf("║   init-dir <путь>               - инициализировать в указанной папке            ║\n");
    printf("║   load [путь]                   - загрузить существующий репозиторий            ║\n");
    printf("║   pwd                           - показать текущую директорию репозитория       ║\n");
    printf("║   help, ?                       - эта справка                                   ║\n");
    printf("║   exit, q                       - выход                                         ║\n");
    printf("╚═════════════════════════════════════════════════════════════════════════════════╝\n");
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
    char *repo_path = find_existing_repo();
    if (repo_path) {
        printf("\n📂 Found existing repository in: %s\n", repo_path);
        printf("   Loading...\n");
        repo = repo_load_from_dir(repo_path);
        if (repo) {
            current_commit = repo_get_head(repo);
            staging = staging_create(current_commit);
        }
        free(repo_path);
    }
}

// ================ КОМАНДЫ ================

static void cmd_load(char *args) {
    if (repo) {
        printf("❌ Репозиторий уже загружен!\n");
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
        printf("❌ Репозиторий уже загружен!\n");
        printf("   Сначала выйдите (exit), затем войдите заново\n");
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
    
    printf("✅ Working directory: %s\n", repo_get_workdir(repo));
}

static void cmd_debug(void) {
    if (!current_commit) {
        printf("❌ Нет коммитов\n");
        return;
    }
    
    printf("\n🔍 ОТЛАДКА ДЕРЕВА:\n");
    printf("Корневой узел: name='%s', is_dir=%d, children_count=%d\n",
           current_commit->root->name,
           current_commit->root->is_directory,
           current_commit->root->children_count);
    
    for (int i = 0; i < current_commit->root->children_count; i++) {
        TreeNode *child = current_commit->root->children[i];
        printf("  Ребёнок %d: name='%s', is_dir=%d\n", 
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
        printf("❌ Репозиторий уже инициализирован!\n");
        return;
    }
    
    repo = repo_create();
    current_commit = init_repo();
    repo_add_commit(repo, current_commit);
    repo_set_head(repo, current_commit);
    repo_update_branch(repo, "master", current_commit);
    
    // Создаём staging
    staging = staging_create(current_commit);
    
    printf("✅ Репозиторий инициализирован!\n");
    print_status_line("Начальный коммит ID", "1");
    print_status_line("Текущая ветка", "master");
    print_status_line("Хеш коммита", current_commit->hash ? current_commit->hash : "вычисляется...");
}

static void cmd_add(char *args) {
    if (!repo) {
        printf("❌ Репозиторий не инициализирован. Введите 'init'\n");
        return;
    }
    
    if (!args || strlen(args) == 0) {
        printf("❌ Использование: add <имя_файла> <содержимое>\n");
        printf("   📝 Пример: add hello.txt \"Hello, World!\"\n");
        return;
    }
    
    // Парсим: add <файл> <содержимое>
    char *file = strtok(args, " ");
    char *content = strtok(NULL, "");
    
    if (!file || !content) {
        printf("❌ Использование: add <имя_файла> <содержимое>\n");
        return;
    }
    
    // Убираем кавычки, если есть
    if (content[0] == '"' || content[0] == '\'') {
        char quote = content[0];
        content++;
        int len = strlen(content);
        if (len > 0 && content[len-1] == quote) {
            content[len-1] = '\0';
        }
    }
    
    staging_add_file(staging, file, content);
    printf("✅ Файл '%s' добавлен в staging (ещё не закоммичен)\n", file);
}

static void cmd_rm(char *file) {
    if (!repo) {
        printf("❌ Репозиторий не инициализирован. Введите 'init'\n");
        return;
    }
    
    if (!file || strlen(file) == 0) {
        printf("❌ Использование: rm <имя_файла>\n");
        return;
    }
    
    staging_remove_file(staging, file);
}

static void cmd_commit(char *message) {
    if (!repo) {
        printf("❌ Репозиторий не инициализирован. Введите 'init'\n");
        return;
    }
    
    if (!message || strlen(message) == 0) {
        printf("❌ Использование: commit <сообщение>\n");
        return;
    }
    
    if (!staging_has_changes(staging)) {
        printf("❌ Нет изменений для коммита. Используйте 'add' для добавления файлов\n");
        return;
    }
    
    current_commit = staging_commit(repo, staging, message);
    if (current_commit) {
        printf("✅ Коммит %d создан: '%s'\n", current_commit->id, message);
        printf("   Хеш: %s\n", current_commit->hash ? current_commit->hash : "вычисляется...");
    }
}

static void cmd_status(void) {
    if (!repo) {
        printf("❌ Репозиторий не инициализирован\n");
        return;
    }
    
    printf("\n📊 === СТАТУС ===\n");
    print_status_line("Текущая ветка", repo->current_branch_name ? repo->current_branch_name : "(detached)");
    print_status_line("Текущий коммит", current_commit ? (current_commit->hash ? current_commit->hash : "none") : "none");
    char id_buf[32];
    if (current_commit) {
        snprintf(id_buf, sizeof(id_buf), "%d", current_commit->id);
        print_status_line("ID коммита", id_buf);
    } else {
        print_status_line("ID коммита", "none");
    }
    
    printf("\n📁 Изменения в staging (%d):\n", staging ? staging->changed_files_count : 0);
    if (staging && staging->changed_files_count > 0) {
        for (int i = 0; i < staging->changed_files_count; i++) {
            printf("   • %s\n", staging->changed_files[i]);
        }
    } else {
        printf("   (нет изменений)\n");
    }
    
    printf("\n🌿 Ветки: ");
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
        printf("❌ Нет коммитов\n");
        return;
    }
    
    if (!file || strlen(file) == 0) {
        printf("❌ Использование: cat <имя_файла>\n");
        return;
    }
    
    char *content = get_file_content(current_commit, file);
    if (content) {
        printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
        printf("%s\n", content);
        printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
        free(content);
    } else {
        printf("❌ Файл '%s' не найден в текущем коммите\n", file);
    }
}

static void cmd_exists(char *file) {
    if (!current_commit) {
        printf("❌ Нет коммитов\n");
        return;
    }
    
    if (!file || strlen(file) == 0) {
        printf("❌ Использование: exists <имя_файла>\n");
        return;
    }
    
    int exists = get_file_exists(current_commit, file);
    printf("📄 Файл '%s': %s\n", file, exists ? "✅ существует" : "❌ не существует");
}

static void cmd_ls(void) {
    if (!current_commit) {
        printf("❌ Нет коммитов\n");
        return;
    }
    
    print_files(current_commit);
}

static void cmd_log(void) {
    if (!current_commit) {
        printf("❌ Нет коммитов\n");
        return;
    }
    
    print_history(current_commit);
}

static void cmd_branch(char *name) {
    if (!repo) {
        printf("❌ Репозиторий не инициализирован\n");
        return;
    }
    
    if (!name || strlen(name) == 0) {
        printf("❌ Использование: branch <имя_ветки>\n");
        return;
    }
    
    repo_create_branch(repo, name, current_commit);
    repo_save_state(repo);
    
    printf("✅ Ветка '%s' создана (указывает на коммит %d)\n", name, current_commit->id);
}

static void cmd_checkout(char *name) {
    if (!repo) {
        printf("❌ Репозиторий не инициализирован\n");
        return;
    }
    
    if (!name || strlen(name) == 0) {
        printf("❌ Использование: checkout <имя_ветки>\n");
        return;
    }
    
    if (staging_has_changes(staging)) {
        printf("⚠️  У вас есть несохранённые изменения в staging.\n");
        printf("   Сначала сделайте commit или reset.\n");
        return;
    }
    
    if (!repo_branch_exists(repo, name)) {
        printf("Branch '%s' does not exist.\n", name);
        return;
    }

    repo_checkout_branch(repo, name);
    current_commit = repo_get_head(repo);
    
    staging_destroy(staging);
    staging = staging_create(current_commit);
    
    repo_save_state(repo);
    
    printf("✅ Переключились на ветку '%s', коммит %d\n", name, current_commit->id);
}

static void cmd_branches(void) {
    if (!repo) {
        printf("❌ Репозиторий не инициализирован\n");
        return;
    }
    
    int count;
    char **branches = repo_list_branches(repo, &count);
    
    printf("\n🌿 Ветки в репозитории:\n");
    for (int i = 0; i < count; i++) {
        printf("   • %s %s\n", branches[i],
               (repo->current_branch_name && strcmp(repo->current_branch_name, branches[i]) == 0) ? "← текущая" : "");
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
        printf("❌ Репозиторий не инициализирован\n");
        return;
    }
    
    if (!staging_has_changes(staging)) {
        printf("ℹ️  Нет изменений для сброса\n");
        return;
    }
    
    // Уничтожаем старый staging и создаём новый
    staging_destroy(staging);
    staging = staging_create(current_commit);
    
    printf("✅ Все изменения в staging отменены\n");
}

// ================ ГЛАВНАЯ ФУНКЦИЯ ================

int main(void) {
    auto_load_repo();
    // Настройка русской локали
    #ifdef _WIN32
        SetConsoleOutputCP(65001);
        SetConsoleCP(65001);
        system("chcp 65001 > nul");
    #else
        setlocale(LC_ALL, "ru_RU.UTF-8");
    #endif
    
    printf("\n");
    printf("╔═════════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                                                                                 ║\n");
    printf("║       MiniGit - Персистентная система контроля версий                           ║\n");
    printf("║                                                                                 ║\n");
    printf("║       Работает как настоящий Git: add → staging → commit                        ║\n");
    printf("║       Поддерживаются: ветки, история, персистентность                           ║\n");
    printf("║                                                                                 ║\n");
    printf("╚═════════════════════════════════════════════════════════════════════════════════╝\n");
    
    print_help();
    
    char input[8192];
    
    while (1) {
        printf("\n🔧 minigit> ");
        fflush(stdout);
        
        if (!fgets(input, sizeof(input), stdin)) {
            break;
        }
        
        // Убираем символ новой строки
        input[strcspn(input, "\n")] = 0;
        if ((unsigned char)input[0] == 0xEF &&
            (unsigned char)input[1] == 0xBB &&
            (unsigned char)input[2] == 0xBF) {
            memmove(input, input + 3, strlen(input + 3) + 1);
        }
        
        if (strlen(input) == 0) {
            continue;
        }
        
        // Парсим команду
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
            printf("До свидания!\n");
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
        else if (strcmp(cmd, "pwd") == 0) {
            if (repo) {
                printf("📁 Repository directory: %s\n", repo_get_workdir(repo));
            } else {
                char cwd_buf[1024];
                if (getcwd(cwd_buf, sizeof(cwd_buf))) {
                    printf("📁 Current directory: %s\n", cwd_buf);
                } else {
                    printf("📁 Current directory: (unknown)\n");
                }
            }
        }
        else {
            printf(" Неизвестная команда: '%s'\n", cmd);
            printf("   Введите 'help' для списка команд\n");
        }
    }
    
    // Очистка
    if (staging) {
        staging_destroy(staging);
    }
    if (repo) {
        free_branches();
        repo_destroy(repo);
    }
    
    return 0;
}
