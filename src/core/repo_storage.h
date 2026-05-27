#ifndef REPO_STORAGE_H
#define REPO_STORAGE_H

#include "repo.h"
#include "types.h"

// Инициализация репозитория в указанной директории
Repository *repo_init_in_dir(const char *path);

// Загрузка существующего репозитория из директории
Repository *repo_load_from_dir(const char *path);

// Сохранение состояния репозитория
int repo_save_state(Repository *repo);

// Получение текущей рабочей директории репозитория
const char *repo_get_workdir(Repository *repo);

// Установка рабочей директории
void repo_set_workdir(Repository *repo, const char *path);

// Сохранение последнего репозитория в конфиг
void repo_save_last_repo(const char *path);

// Загрузка последнего репозитория из конфига
char *repo_load_last_repo(void);

#endif