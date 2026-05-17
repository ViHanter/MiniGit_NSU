#ifndef TYPES_H
#define TYPES_H

#include <time.h>

//содержимое файла
typedef struct blob {
    char *hash;
    char *content;      // содержимое файла
    size_t size;        // размер
    int ref_count;
} Blob;

//файл или папка
typedef struct tree_node {
    char *name;                 // имя файла/папки
    int is_directory;           // 1 = папка, 0 = файл
    Blob *blob;                 // если файл
    struct tree_node **children; // если папка (массив)
    int children_count;         // для папок
    int ref_count;
} TreeNode;

// Коммит (персистентный) — ПОЛНАЯ ВЕРСИЯ
typedef struct commit {
    char *hash;                 
    int id;                     // числовой ID (для удобства)
    struct commit *parent;      // родительский коммит (NULL для первого)
    struct commit **children;   // дочерние коммиты (для ветвления)
    int children_count;         // количество дочерних коммитов
    int children_capacity;      // выделенная память под children
    TreeNode *root;             // корень дерева файлов
    char *message;              // сообщение коммита
    time_t timestamp;           // время создания
    char **changed_files;       // список изменённых/добавленных/удалённых
    int changed_files_count;    // количество изменённых файлов
} Commit;

typedef struct staging_area {
    Commit *base_commit;          // от какого коммита делаем изменения
    TreeNode *pending_tree;       // строится при добавлении файлов
    char **changed_files;         // список изменённых файлов
    int changed_files_count;
    int changed_files_capacity;
} StagingArea;


// Ветка
typedef struct branch {
    char *name;           // имя ветки
    Commit *commit;       // указатель на последний коммит ветки
} Branch;

// Репозиторий
typedef struct repository {
    Commit *head;                // текущий коммит (HEAD)
    char *current_branch_name;   // имя текущей ветки
    Commit **all_commits;        // массив всех коммитов
    int commits_count;           // количество коммитов
    int commits_capacity;        // выделенная память
    Branch *branches;            // массив всех веток
    int branches_count;          // количество веток
    int branches_capacity;       // выделенная память
} Repository;

#endif
