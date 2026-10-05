#include <stdio.h>
#include <string.h>
#include "view.h"
#include "edit.h"

void help_menu()
{
    printf("\n");
    printf("============================================\n");
    printf("             MP3 TAG READER\n");
    printf("============================================\n");

    printf("\nVIEW:\n");
    printf("  ./a.out -v Song.mp3\n");

    printf("\nEDIT:\n");
    printf("  -t  Song Title\n");
    printf("  -a  Artist\n");
    printf("  -A  Album\n");
    printf("  -y  Year\n");
    printf("  -g  Genre\n");
    printf("  -c  Composer\n");
    printf("  -p  Publisher\n");
    printf("  -n  Track Number\n");
    printf("  -D  Disc Number\n");

    printf("\nEDIT EXAMPLES:\n");
    printf("  ./a.out -e Song.mp3 -t \"My New Song\"\n");
    printf("  ./a.out -e Song.mp3 -a \"New Artist\"\n");
    printf("  ./a.out -e Song.mp3 -A \"New Album Name\"\n");
    printf("  ./a.out -e Song.mp3 -y 2025\n");
    printf("  ./a.out -e Song.mp3 -g \"Rock\"\n");
    printf("  ./a.out -e Song.mp3 -c \"New Composer\"\n");
    printf("  ./a.out -e Song.mp3 -p \"New Publisher\"\n");
    printf("  ./a.out -e Song.mp3 -n 5\n");
    printf("  ./a.out -e Song.mp3 -D 2\n");

    printf("\nDELETE:\n");
    printf("  ./a.out -d Song.mp3 -t\n");
    printf("  ./a.out -d Song.mp3 -a\n");
    printf("  ./a.out -d Song.mp3 -A\n");
    printf("  ./a.out -d Song.mp3 -y\n");
    printf("  ./a.out -d Song.mp3 -g\n");
    printf("  ./a.out -d Song.mp3 -c\n");
    printf("  ./a.out -d Song.mp3 -p\n");
    printf("  ./a.out -d Song.mp3 -n\n");
    printf("  ./a.out -d Song.mp3 -D\n");

    printf("\nHELP:\n");
    printf("  ./a.out -h\n");

    printf("\n============================================\n");
}


int main(int argc, char *argv[])
{
    /* Help */
    if (argc == 2 && strcmp(argv[1], "-h") == 0)
    {
        help_menu();
        return 0;
    }

    if (argc < 2)
    {
        printf("ERROR: Missing arguments\n");
        printf("Use './a.out -h' for help\n");
        return 1;
    }

    /* View */
    if (strcmp(argv[1], "-v") == 0)
    {
        if (argc < 3)
        {
            printf("ERROR: Missing filename\n");
            return 1;
        }

        view_mp3(argv[2]);
    }

    /* Edit */
    else if (strcmp(argv[1], "-e") == 0)
    {
        if (argc < 5)
        {
            printf("ERROR: Missing edit arguments\n");
            printf("Use './a.out -h' for help\n");
            return 1;
        }

        edit_mp3(argv[2], argv[3], argv[4]);
    }

    /* Delete */
    else if (strcmp(argv[1], "-d") == 0)
    {
        if (argc < 4)
        {
            printf("ERROR: Missing delete option\n");
            printf("Use './a.out -h' for help\n");
            return 1;
        }

        delete_tag(argv[2], argv[3]);
    }

    else
    {
        printf("ERROR: Invalid operation '%s'\n", argv[1]);
        printf("Use './a.out -h' for help\n");
        return 1;
    }

    return 0;
}