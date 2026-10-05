#include <stdio.h>
#include <string.h>
#include "view.h"

void view_mp3(const char *filename)
{
    FILE *fp;

    unsigned char header[10];
    unsigned char size_bytes[4];

    char frame_id[5];
    char data[1025];

    unsigned int frame_size;
    unsigned int tag_size;
    unsigned int bytes_read;

    fp = fopen(filename, "rb");

    if (fp == NULL)
    {
        printf("ERROR: Unable to open file\n");
        return;
    }

    /* Read ID3 header */
    if (fread(header, 1, 10, fp) != 10)
    {
        printf("ERROR: Unable to read ID3 header\n");
        fclose(fp);
        return;
    }

    /* Check ID3 signature */
    if (header[0] != 'I' ||
        header[1] != 'D' ||
        header[2] != '3')
    {
        printf("ERROR: ID3 tag not found\n");
        fclose(fp);
        return;
    }

    /* Read ID3 tag size */
    tag_size = ((unsigned int)(header[6] & 0x7F) << 21) |
               ((unsigned int)(header[7] & 0x7F) << 14) |
               ((unsigned int)(header[8] & 0x7F) << 7)  |
               (header[9] & 0x7F);

    bytes_read = 0;

    printf("\n----- MP3 TAG INFORMATION -----\n");

    printf("ID3 Version : ID3v2.%d.%d\n",
           header[3],
           header[4]);

    /* Read frames */
    while (bytes_read + 10 <= tag_size)
    {
        /* Read frame ID */
        if (fread(frame_id, 1, 4, fp) != 4)
            break;

        frame_id[4] = '\0';

        /* Padding reached */
        if (frame_id[0] == '\0')
            break;

        /* Read frame size */
        if (fread(size_bytes, 1, 4, fp) != 4)
            break;

        frame_size = ((unsigned int)size_bytes[0] << 24) |
                     ((unsigned int)size_bytes[1] << 16) |
                     ((unsigned int)size_bytes[2] << 8) |
                     size_bytes[3];

        /* Skip frame flags */
        if (fseek(fp, 2, SEEK_CUR) != 0)
            break;

        bytes_read += 10;

        /* Invalid frame size */
        if (frame_size == 0)
            break;

        if (bytes_read + frame_size > tag_size)
            break;

        /*
         * Our buffer can hold 1024 bytes.
         * If frame is larger, skip it.
         */
        if (frame_size > sizeof(data) - 1)
        {
            fseek(fp, frame_size, SEEK_CUR);
            bytes_read += frame_size;
            continue;
        }

        /* Read frame data */
        if (fread(data, 1, frame_size, fp) != frame_size)
            break;

        data[frame_size] = '\0';

        /*
         * Text frames:
         * data[0] = encoding
         * data[1...] = actual text
         */

        if (strcmp(frame_id, "TIT2") == 0)
        {
            printf("Title    : %s\n", &data[1]);
        }
        else if (strcmp(frame_id, "TPE1") == 0)
        {
            printf("Artist   : %s\n", &data[1]);
        }
        else if (strcmp(frame_id, "TALB") == 0)
        {
            printf("Album    : %s\n", &data[1]);
        }
        else if (strcmp(frame_id, "TYER") == 0)
        {
            printf("Year     : %s\n", &data[1]);
        }
        else if (strcmp(frame_id, "TCON") == 0)
        {
            printf("Genre    : %s\n", &data[1]);
        }
        else if (strcmp(frame_id, "TCOM") == 0)
        {
            printf("Composer : %s\n", &data[1]);
        }
        else if (strcmp(frame_id, "TPUB") == 0)
        {
            printf("Publisher: %s\n", &data[1]);
        }
        else if (strcmp(frame_id, "TRCK") == 0)
        {
            printf("Track No.: %s\n", &data[1]);
        }
        else if (strcmp(frame_id, "TPOS") == 0)
        {
            printf("Disc No. : %s\n", &data[1]);
        }

        bytes_read += frame_size;
    }

    printf("--------------------------------\n");

    fclose(fp);
}