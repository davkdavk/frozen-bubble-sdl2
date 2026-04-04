#include <cstdlib>
#include "iniparser.h"

extern "C" {

void iniparser_freedict(dictionary* d) {
    if (d) {
        free(d);
    }
}

dictionary* iniparser_load(const char* ini) {
    dictionary* d = (dictionary*)calloc(1, sizeof(dictionary));
    return d;
}

int iniparser_set(dictionary* d, const char* entry, const char* val) {
    return 0;
}

void iniparser_dump_ini(dictionary* d, FILE* f) {
}

int iniparser_getint(dictionary* d, const char* key, int def) {
    return def;
}

int iniparser_getboolean(dictionary* d, const char* key, int def) {
    return def;
}

}
