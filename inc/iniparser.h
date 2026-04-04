#ifndef INIPARSER_H
#define INIPARSER_H

#include <stdio.h>

typedef struct _dictionary {
    void* data;
} dictionary;

#ifdef __cplusplus
extern "C" {
#endif

void iniparser_freedict(dictionary* d);
dictionary* iniparser_load(const char* ini);
int iniparser_set(dictionary* d, const char* entry, const char* val);
void iniparser_dump_ini(dictionary* d, FILE* f);
int iniparser_getint(dictionary* d, const char* key, int def);
int iniparser_getboolean(dictionary* d, const char* key, int def);

#ifdef __cplusplus
}
#endif

#endif
