#ifndef PRINT_H
#define PRINT_H

#include "../core/types.h"

// Выводит информацию о коммите (хеш, родитель, сообщение, дату, изменённые файлы)
void print_commit(const Commit *commit);

// Выводит всю историю (рекурсивно от commit до корня)
void print_history(const Commit *commit);

// Выводит все файлы и их хеши (плоский список)
void print_files(const Commit *commit);

#endif