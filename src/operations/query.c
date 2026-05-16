#include "query.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

char *get_file_content(Commit *commit, const char *path) {
    if (!commit || !path) return NULL;
    
    Blob *blob = find_blob_by_path(commit->root, path);
    if (!blob || !blob->content) return NULL;
    
    // Возвращаем КОПИЮ строки (чтобы caller мог её свободно менять)
    return strdup(blob->content);
}

int get_file_exists(Commit *commit, const char *path) {
    if (!commit || !path) return 0;
    return file_exists_in_tree(commit->root, path);
}

size_t get_file_size(Commit *commit, const char *path) {
    if (!commit || !path) return 0;
    
    Blob *blob = find_blob_by_path(commit->root, path);
    if (!blob) return 0;
    return blob->size;
}

char *get_file_hash(Commit *commit, const char *path) {
    if (!commit || !path) return NULL;
    
    Blob *blob = find_blob_by_path(commit->root, path);
    if (!blob || !blob->hash) return NULL;
    
    return strdup(blob->hash);
}

int extract_file_to_disk(Commit *commit, const char *path, const char *output_path) {
    if (!commit || !path || !output_path) return -1;
    
    char *content = get_file_content(commit, path);
    if (!content) {
        printf("extract_file_to_disk: file '%s' not found in commit %d\n", 
               path, commit->id);
        return -1;
    }
    
    FILE *f = fopen(output_path, "w");
    if (!f) {
        printf("extract_file_to_disk: cannot open '%s' for writing\n", output_path);
        free(content);
        return -1;
    }
    
    fputs(content, f);
    fclose(f);
    free(content);
    
    printf("extract_file_to_disk: extracted '%s' to '%s'\n", path, output_path);
    return 0;
}