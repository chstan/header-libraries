#ifndef CSYS_H
#define CSYS_H

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

char *read_file(const char *file_name) {
    char *file_contents;
    size_t input_file_size;

    FILE *input_file = fopen(file_name, "rb");
    fseek(input_file, 0, SEEK_END);
    input_file_size = ftell(input_file);
    rewind(input_file);

    file_contents = (char *) malloc((input_file_size + 1) * (sizeof(char)));
    fread(file_contents, sizeof(char), input_file_size, input_file);
    fclose(input_file);
    file_contents[input_file_size] = '\0';
    return file_contents;
}

#endif
