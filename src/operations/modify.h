#ifndef MODIFY_H
#define MODIFY_H

#include "../core/types.h"
#include "../core/tree.h"
#include "../core/commit.h"

// ================ ПО ТРЕБОВАНИЯМ ТЗ ================

// add_file: принимает текущий коммит, путь к файлу и новое содержимое.
// Возвращает НОВЫЙ коммит, в котором этот файл добавлен или обновлён.
// Старый коммит остаётся без изменений.
Commit *add_file(Repository *repo, Commit *current_commit, 
                 const char *path, const char *content);

// remove_file: принимает текущий коммит и путь к файлу.
// Возвращает НОВЫЙ коммит, в котором указанный файл удалён.
Commit *remove_file(Repository *repo, Commit *current_commit, 
                    const char *path);

// commit: принимает текущее состояние (промежуточный коммит/staging) и сообщение.
// Создаёт финальный коммит с вычисленным хешем и привязкой к родителю.
// ВНИМАНИЕ: в нашей модели add_file/remove_file уже создают коммит.
// Эта функция нужна, если у тебя есть staging area (отложенное сохранение).
// Если staging нет — можно использовать add_file как commit.
Commit *commit_changes(Repository *repo, Commit *staging, const char *message);

// ================ ВСПОМОГАТЕЛЬНЫЕ ================

// Обновляет ветку и HEAD после создания нового коммита
void update_repo_after_commit(Repository *repo, Commit *new_commit);

#endif