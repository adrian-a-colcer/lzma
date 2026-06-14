#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define HEADER_SIZE 13
#define BUFFER_SIZE 4096

typedef struct {
    unsigned int lc;
    unsigned int lp;
    unsigned int pb;
} Properties;

typedef struct {
    unsigned char *buf;
    uint32_t pos;
    uint32_t size;
    uint8_t isFull;
} COutWindow;

void init_out_window(COutWindow *out_window, uint32_t dict_size) {
    out_window->buf = (unsigned char *)malloc(dict_size);
    if (!out_window->buf) {
        fprintf(stderr, "Memory allocation failed for output window.\n");
        exit(EXIT_FAILURE);
    }
    out_window->pos = 0;
    out_window->size = dict_size;
    out_window->isFull = 0;
}   

uint8_t isEmpty(const COutWindow *out_window) {
    return out_window->pos == 0;
}


// print first
void print_hex_dump(const unsigned char *buffer, size_t bytes_read, size_t offset, size_t size) {

    for (size_t i = 0; i < size; i++) {
        if (i + offset < bytes_read) {
            printf("%02x ", buffer[offset + i]);
        }else {
            printf("   "); // padding for incomplete
        }
    }
    printf("\n");
}

int main(int argc, char *argv[]) {  

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <lzma_file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    FILE *fptr = fopen(argv[1], "rb");
    if (!fptr) {
        perror("File opening failed");
        return EXIT_FAILURE;
    }

    unsigned char header_buf[HEADER_SIZE];
    
    // Read the header (first 13 bytes)
    size_t header_bytes_read = fread(header_buf, 1, HEADER_SIZE, fptr);
    if (header_bytes_read < HEADER_SIZE) {
        fprintf(stderr, "Error: File is too small to contain a valid LZMA header.\n");
        fclose(fptr);
        return EXIT_FAILURE;
    }

    Properties props;
    unsigned char prop_byte = header_buf[0];
    if (prop_byte >= 225) {
        fprintf(stderr, "Error: Invalid LZMA properties byte (>= 225).\n");
        fclose(fptr);
        return EXIT_FAILURE;
    }

    props.lc = header_buf[0] % 9;
    props.lp = (header_buf[0] / 9) % 5;
    props.pb = header_buf[0] / 45;
    
    // LZMA 2 properties 
    if (props.lc + props.lp > 4) {
        fprintf(stderr, "Error: Invalid LZMA properties (lc + lp must be <= 4).\n");
        fclose(fptr);
        return EXIT_FAILURE;
    }

    // Dictionary size
    unsigned int dict_size = header_buf[1] | (header_buf[2] << 8) | (header_buf[3] << 16) | (header_buf[4] << 24);
    // Uncompressed size
    unsigned long long uncompressed_size = 0;
    for (int i = 0; i < 8; i++) {
        uncompressed_size |= ((unsigned long long)header_buf[5 + i]) << (8 * i);
    }
    // Compressed data from offset 13
    printf("Properties: lc=%u, lp=%u, pb=%u\n", props.lc, props.lp, props.pb);
    printf("Dictionary size: %u\n", dict_size);
    printf("Uncompressed size: %llu\n", uncompressed_size);

    unsigned char buffer[BUFFER_SIZE];
    size_t bytes_read;
    size_t current_offset = HEADER_SIZE;

    size_t size_of_prob_arrays = 1846 + 768 * (1 << (props.lp + props.lc));

    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, fptr)) > 0) {
        // Print the hex dump of the current buffer
        print_hex_dump(buffer, bytes_read, 0, bytes_read);
        
        current_offset += bytes_read;
    }

    if (ferror(fptr)) {
        perror("I/O error when reading");
    }

    fclose(fptr);
    return EXIT_SUCCESS;
}