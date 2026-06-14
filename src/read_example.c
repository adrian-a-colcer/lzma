#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{   
    int is_ok = EXIT_FAILURE;

    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        return is_ok;
    }

    
    FILE *fptr = fopen(argv[1], "r");

    if (!fptr)
    {
        perror("File opening failed");
        return is_ok;
    }

    int c; // note: int, not char, required to handle EOF
    while ((c = fgetc(fptr)) != EOF) // standard C I/O file reading loop
    {
        putchar(c);
    }

    if (ferror(fptr))
    {
        puts("I/O error when reading");
    }
    else if (feof(fptr))
    {
        is_ok = EXIT_SUCCESS;
    }

    fclose(fptr);
    return is_ok;
}