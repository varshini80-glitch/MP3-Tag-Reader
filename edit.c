#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "edit.h"

#define HEADER_SIZE 10
#define FRAME_HEADER_SIZE 10
#define EXTRA_PADDING 1024

/* ---------------------------------------------------------
   Get frame ID and field name
   --------------------------------------------------------- */
int get_frame_info(const char *tag, char *frame_id, char *field_name)
{
    if (strcmp(tag, "-t") == 0)
    {
        strcpy(frame_id, "TIT2");
        strcpy(field_name, "Title");
    }
    else if (strcmp(tag, "-a") == 0)
    {
        strcpy(frame_id, "TPE1");
        strcpy(field_name, "Artist");
    }
    else if (strcmp(tag, "-A") == 0)
    {
        strcpy(frame_id, "TALB");
        strcpy(field_name, "Album");
    }
    else if (strcmp(tag, "-y") == 0)
    {
        strcpy(frame_id, "TYER");
        strcpy(field_name, "Year");
    }
    else if (strcmp(tag, "-g") == 0)
    {
        strcpy(frame_id, "TCON");
        strcpy(field_name, "Genre");
    }
    else if (strcmp(tag, "-c") == 0)
    {
        strcpy(frame_id, "TCOM");
        strcpy(field_name, "Composer");
    }
    else if (strcmp(tag, "-p") == 0)
    {
        strcpy(frame_id, "TPUB");
        strcpy(field_name, "Publisher");
    }
    else if (strcmp(tag, "-n") == 0)
    {
        strcpy(frame_id, "TRCK");
        strcpy(field_name, "Track Number");
    }
    else if (strcmp(tag, "-D") == 0)
    {
        strcpy(frame_id, "TPOS");
        strcpy(field_name, "Disc Number");
    }
    else
    {
        return 0;
    }

    return 1;
}

/* ---------------------------------------------------------
   Read normal 4-byte frame size
   ID3v2.3 uses big-endian frame sizes
   --------------------------------------------------------- */
unsigned int read_frame_size(unsigned char b[4])
{
    return ((unsigned int)b[0] << 24) |
           ((unsigned int)b[1] << 16) |
           ((unsigned int)b[2] << 8) |
           b[3];
}

/* ---------------------------------------------------------
   Write normal 4-byte frame size
   --------------------------------------------------------- */
void write_frame_size(unsigned char b[4], unsigned int size)
{
    b[0] = (size >> 24) & 0xFF;
    b[1] = (size >> 16) & 0xFF;
    b[2] = (size >> 8) & 0xFF;
    b[3] = size & 0xFF;
}

/* ---------------------------------------------------------
   Read ID3 tag size
   ID3v2 uses sync-safe integer
   --------------------------------------------------------- */
unsigned int read_tag_size(unsigned char b[4])
{
    return ((unsigned int)(b[0] & 0x7F) << 21) |
           ((unsigned int)(b[1] & 0x7F) << 14) |
           ((unsigned int)(b[2] & 0x7F) << 7) |
           (b[3] & 0x7F);
}

/* ---------------------------------------------------------
   Write ID3 tag size
   --------------------------------------------------------- */
void write_tag_size(unsigned char b[4], unsigned int size)
{
    b[0] = (size >> 21) & 0x7F;
    b[1] = (size >> 14) & 0x7F;
    b[2] = (size >> 7) & 0x7F;
    b[3] = size & 0x7F;
}

/* ---------------------------------------------------------
   Copy all remaining bytes
   --------------------------------------------------------- */
int copy_remaining(FILE *src, FILE *dst)
{
    unsigned char buffer[4096];
    size_t n;

    while ((n = fread(buffer, 1, sizeof(buffer), src)) > 0)
    {
        if (fwrite(buffer, 1, n, dst) != n)
            return 0;
    }

    return 1;
}

/* ---------------------------------------------------------
   Write a new ID3 text frame
   --------------------------------------------------------- */
void write_text_frame(unsigned char *buffer,
                      unsigned int *position,
                      const char *frame_id,
                      const char *value)
{
    unsigned int value_len;
    unsigned int frame_size;
    unsigned char size_bytes[4];

    value_len = strlen(value);

    /*
     * Text frame data:
     * 1 byte encoding + text
     */
    frame_size = value_len + 1;

    memcpy(buffer + *position,
           frame_id,
           4);

    *position += 4;

    write_frame_size(size_bytes, frame_size);

    memcpy(buffer + *position,
           size_bytes,
           4);

    *position += 4;

    /* Flags */
    buffer[*position] = 0;
    buffer[*position + 1] = 0;

    *position += 2;

    /* Encoding: ISO-8859-1 */
    buffer[*position] = 0;

    *position += 1;

    /* Text */
    memcpy(buffer + *position,
           value,
           value_len);

    *position += value_len;
}

/* ---------------------------------------------------------
   EDIT / ADD TAG
   --------------------------------------------------------- */
void edit_mp3(const char *filename,
              const char *tag,
              const char *new_value)
{
    FILE *src;
    FILE *dst;

    unsigned char header[HEADER_SIZE];

    char frame_id[5];
    char field_name[50];
    char temp_name[512];

    unsigned int old_tag_size;
    unsigned int new_tag_size;

    unsigned char *old_tag;
    unsigned char *new_tag;

    unsigned int old_pos;
    unsigned int new_pos;

    int found = 0;

    if (!get_frame_info(tag, frame_id, field_name))
    {
        printf("ERROR: Invalid tag option\n");
        return;
    }

    src = fopen(filename, "rb");

    if (src == NULL)
    {
        printf("ERROR: Unable to open file\n");
        return;
    }

    /* Read ID3 header */
    if (fread(header, 1, HEADER_SIZE, src) != HEADER_SIZE)
    {
        printf("ERROR: Invalid MP3 file\n");
        fclose(src);
        return;
    }

    /* Check ID3 */
    if (header[0] != 'I' ||
        header[1] != 'D' ||
        header[2] != '3')
    {
        printf("ERROR: ID3 tag not found\n");
        fclose(src);
        return;
    }

    old_tag_size = read_tag_size(&header[6]);

    /*
     * Read complete old ID3 tag.
     */
    old_tag = malloc(old_tag_size);

    if (old_tag == NULL)
    {
        printf("ERROR: Memory allocation failed\n");
        fclose(src);
        return;
    }

    if (fread(old_tag, 1, old_tag_size, src)
        != old_tag_size)
    {
        printf("ERROR: Unable to read ID3 tag\n");

        free(old_tag);
        fclose(src);

        return;
    }

    /*
     * Allocate enough memory for:
     *
     * old tag
     * + new frame
     * + extra padding
     */
    new_tag = malloc(old_tag_size +
                     strlen(new_value) +
                     FRAME_HEADER_SIZE +
                     EXTRA_PADDING + 10);

    if (new_tag == NULL)
    {
        printf("ERROR: Memory allocation failed\n");

        free(old_tag);
        fclose(src);

        return;
    }

    old_pos = 0;
    new_pos = 0;

    /*
     * Process existing frames.
     */
    while (old_pos + FRAME_HEADER_SIZE <= old_tag_size)
    {
        char current_id[5];
        unsigned char size_bytes[4];

        unsigned int frame_size;

        memcpy(current_id,
               old_tag + old_pos,
               4);

        current_id[4] = '\0';

        /*
         * Zero ID means padding.
         */
        if (current_id[0] == '\0')
            break;

        memcpy(size_bytes,
               old_tag + old_pos + 4,
               4);

        frame_size = read_frame_size(size_bytes);

        /*
         * Invalid frame.
         */
        if (frame_size == 0 ||
            old_pos + FRAME_HEADER_SIZE + frame_size >
            old_tag_size)
        {
            break;
        }

        /*
         * Target frame found.
         */
        if (strcmp(current_id, frame_id) == 0)
        {
            write_text_frame(new_tag,
                             &new_pos,
                             frame_id,
                             new_value);

            found = 1;
        }
        else
        {
            /*
             * Copy original frame.
             */
            memcpy(new_tag + new_pos,
                   old_tag + old_pos,
                   FRAME_HEADER_SIZE + frame_size);

            new_pos += FRAME_HEADER_SIZE + frame_size;
        }

        old_pos += FRAME_HEADER_SIZE + frame_size;
    }

    /*
     * If frame doesn't exist,
     * add a new frame inside the ID3 tag.
     */
    if (!found)
    {
        write_text_frame(new_tag,
                         &new_pos,
                         frame_id,
                         new_value);
    }

    /*
     * If the rebuilt tag is larger than old tag,
     * enlarge it and add padding.
     */
    if (new_pos > old_tag_size)
    {
        new_tag_size = new_pos + EXTRA_PADDING;
    }
    else
    {
        /*
         * Keep original tag size.
         */
        new_tag_size = old_tag_size;
    }

    /*
     * Create temporary file.
     */
    snprintf(temp_name,
             sizeof(temp_name),
             "%s.tmp",
             filename);

    dst = fopen(temp_name, "wb");

    if (dst == NULL)
    {
        printf("ERROR: Unable to create temporary file\n");

        free(old_tag);
        free(new_tag);
        fclose(src);

        return;
    }

    /*
     * Update tag size in header.
     */
    write_tag_size(&header[6], new_tag_size);

    /*
     * Write header.
     */
    fwrite(header, 1, HEADER_SIZE, dst);

    /*
     * Write rebuilt tag.
     */
    fwrite(new_tag, 1, new_pos, dst);

    /*
     * Fill remaining tag space with zeros.
     */
    {
        unsigned int padding;
        unsigned char zero_buffer[4096] = {0};

        padding = new_tag_size - new_pos;

        while (padding > 0)
        {
            unsigned int chunk;

            if (padding > sizeof(zero_buffer))
                chunk = sizeof(zero_buffer);
            else
                chunk = padding;

            fwrite(zero_buffer, 1, chunk, dst);

            padding -= chunk;
        }
    }

    /*
     * IMPORTANT:
     *
     * Source is currently positioned immediately after
     * the ORIGINAL ID3 tag.
     *
     * Therefore the MP3 audio can now be copied directly.
     */
    if (!copy_remaining(src, dst))
    {
        printf("ERROR: Unable to copy MP3 audio\n");

        fclose(src);
        fclose(dst);

        free(old_tag);
        free(new_tag);

        remove(temp_name);

        return;
    }

    fclose(src);
    fclose(dst);

    free(old_tag);
    free(new_tag);

    /*
     * Replace original file.
     */
    if (remove(filename) != 0)
    {
        printf("ERROR: Unable to remove original file\n");
        remove(temp_name);
        return;
    }

    if (rename(temp_name, filename) != 0)
    {
        printf("ERROR: Unable to replace original file\n");
        return;
    }

    printf("%s updated successfully!\n", field_name);
}

/* ---------------------------------------------------------
   DELETE TAG
   --------------------------------------------------------- */
void delete_tag(const char *filename,
                const char *tag)
{
    FILE *src;
    FILE *dst;

    unsigned char header[HEADER_SIZE];

    char frame_id[5];
    char field_name[50];
    char temp_name[512];

    unsigned int tag_size;

    unsigned char *old_tag;
    unsigned char *new_tag;

    unsigned int old_pos;
    unsigned int new_pos;

    int found = 0;

    if (!get_frame_info(tag, frame_id, field_name))
    {
        printf("ERROR: Invalid tag option\n");
        return;
    }

    src = fopen(filename, "rb");

    if (src == NULL)
    {
        printf("ERROR: Unable to open file\n");
        return;
    }

    if (fread(header, 1, HEADER_SIZE, src) != HEADER_SIZE)
    {
        printf("ERROR: Invalid MP3 file\n");
        fclose(src);
        return;
    }

    if (header[0] != 'I' ||
        header[1] != 'D' ||
        header[2] != '3')
    {
        printf("ERROR: ID3 tag not found\n");
        fclose(src);
        return;
    }

    tag_size = read_tag_size(&header[6]);

    old_tag = malloc(tag_size);
    new_tag = malloc(tag_size);

    if (old_tag == NULL || new_tag == NULL)
    {
        printf("ERROR: Memory allocation failed\n");

        free(old_tag);
        free(new_tag);

        fclose(src);

        return;
    }

    if (fread(old_tag, 1, tag_size, src) != tag_size)
    {
        printf("ERROR: Unable to read ID3 tag\n");

        free(old_tag);
        free(new_tag);

        fclose(src);

        return;
    }

    old_pos = 0;
    new_pos = 0;

    /*
     * Rebuild tag without target frame.
     */
    while (old_pos + FRAME_HEADER_SIZE <= tag_size)
    {
        char current_id[5];
        unsigned char size_bytes[4];

        unsigned int frame_size;

        memcpy(current_id,
               old_tag + old_pos,
               4);

        current_id[4] = '\0';

        if (current_id[0] == '\0')
            break;

        memcpy(size_bytes,
               old_tag + old_pos + 4,
               4);

        frame_size = read_frame_size(size_bytes);

        if (frame_size == 0 ||
            old_pos + FRAME_HEADER_SIZE + frame_size >
            tag_size)
        {
            break;
        }

        if (strcmp(current_id, frame_id) == 0)
        {
            /*
             * Do not copy target frame.
             */
            found = 1;
        }
        else
        {
            memcpy(new_tag + new_pos,
                   old_tag + old_pos,
                   FRAME_HEADER_SIZE + frame_size);

            new_pos += FRAME_HEADER_SIZE + frame_size;
        }

        old_pos += FRAME_HEADER_SIZE + frame_size;
    }

    /*
     * Keep same ID3 tag size.
     */
    {
        unsigned int padding;
        unsigned char zero_buffer[4096] = {0};

        padding = tag_size - new_pos;

        while (padding > 0)
        {
            unsigned int chunk;

            if (padding > sizeof(zero_buffer))
                chunk = sizeof(zero_buffer);
            else
                chunk = padding;

            /*
             * We don't need to store padding in new_tag
             * separately. It will be written directly later.
             */
            padding -= chunk;
        }
    }

    snprintf(temp_name,
             sizeof(temp_name),
             "%s.tmp",
             filename);

    dst = fopen(temp_name, "wb");

    if (dst == NULL)
    {
        printf("ERROR: Unable to create temporary file\n");

        free(old_tag);
        free(new_tag);

        fclose(src);

        return;
    }

    /*
     * Keep same tag size.
     */
    write_tag_size(&header[6], tag_size);

    fwrite(header, 1, HEADER_SIZE, dst);

    /*
     * Write remaining frames.
     */
    fwrite(new_tag, 1, new_pos, dst);

    /*
     * Write zero padding.
     */
    {
        unsigned int padding;
        unsigned char zero_buffer[4096] = {0};

        padding = tag_size - new_pos;

        while (padding > 0)
        {
            unsigned int chunk;

            if (padding > sizeof(zero_buffer))
                chunk = sizeof(zero_buffer);
            else
                chunk = padding;

            fwrite(zero_buffer, 1, chunk, dst);

            padding -= chunk;
        }
    }

    /*
     * Source is already positioned after old ID3 tag.
     * Copy MP3 audio unchanged.
     */
    if (!copy_remaining(src, dst))
    {
        printf("ERROR: Unable to copy MP3 audio\n");

        fclose(src);
        fclose(dst);

        free(old_tag);
        free(new_tag);

        remove(temp_name);

        return;
    }

    fclose(src);
    fclose(dst);

    free(old_tag);
    free(new_tag);

    if (!found)
    {
        remove(temp_name);

        printf("ERROR: %s frame not found\n",
               field_name);

        return;
    }

    if (remove(filename) != 0)
    {
        printf("ERROR: Unable to remove original file\n");
        remove(temp_name);
        return;
    }

    if (rename(temp_name, filename) != 0)
    {
        printf("ERROR: Unable to replace original file\n");
        return;
    }

    printf("%s deleted successfully!\n", field_name);
}