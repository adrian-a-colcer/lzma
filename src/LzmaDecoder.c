#include <stdio.h>
#include <stdlib.h>

#include "LzmaDecoder.h"

#define RINOK(expr) { int res = (expr); if (res != 0) return res; }

#define HEADER_SIZE 13
#define BUFFER_SIZE 4096
#define kNumBitModelTotalBits 11

#define LZMA_DIC_MIN (1 << 12)

#define kTopValue (1 << 24)

#define PROB_INIT_VAL ((1 << k))

int range_decoder_init(range_decoder *rd, const uint8_t *buffer, size_t size) {
    if (rd == NULL || buffer == NULL || size < 5) {
        return 1; // Invalid parameters
    }

    rd->range = 0xFFFFFFFF;
    uint32_t code = 0;

    uint8_t start_byte = buffer[0];

    for (int i = 0; i < 4; i++) {
        code = (code << 8) | buffer[1 + i];
    }
    rd->code = code;

    if (start_byte != 0 || rd->code == rd->range) {
        rd->corrupted = 1;
    }
    return (start_byte == 0);
}

int is_finished(range_decoder *rd) {
    return rd->code == rd->range;
}

void range_decoder_normalize(range_decoder *rd) {
    while (rd->range < kTopValue) {
        rd->range <<= 8;
        rd->code = (rd->code << 8) | *(rd->buffer++);
    }
}

uint32_t range_decoder_direct(range_decoder *rd, uint32_t num_total_bits) {
    uint32_t result = 0;
    while (num_total_bits--) {
        rd->range >>= 1;
        rd->code -= rd->range;
        uint32_t t = 0 - (rd->code >> 31);
        rd->code += rd->range & t;

        range_decoder_normalize(rd);
        result <<= 1;
        result += t + 1;
    }
    return result;
}

int decode_properties(LzmaProps *props, const uint8_t *data)
{
    uint32_t dictSize = 0;
    uint8_t lc, lp, pb;
    uint8_t d = data[0];

    if (d >= 9 * 5 * 5) {
        fprintf(stderr, "Error: Invalid LZMA properties uint8_t (>= 225).\n");
        return 1;
    }

    for (int i = 0; i < 4; i++) {
        dictSize |= ((uint32_t)data[1 + i]) << (8 * i);
    }

    if (dictSize < LZMA_DIC_MIN) {
        dictSize = LZMA_DIC_MIN;
    }

    pb = d / (9 * 5);
    d -= pb * 9 * 5;
    lp = d / 9;
    lc = d - lp * 9;

    props->lc = lc;
    props->lp = lp;
    props->pb = pb;
    props->dictSize = dictSize;

    return 0;
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

    uint8_t header_buf[HEADER_SIZE];

    // Read the header (first 13 bytes)
    if (fread(header_buf, 1, HEADER_SIZE, fptr) < HEADER_SIZE) {
        fprintf(stderr, "Error: File is too small to contain a valid LZMA header.\n");
        fclose(fptr);
        return EXIT_FAILURE;
    }

    if (ferror(fptr)) {
        perror("I/O error when reading");
    }

    LzmaProps props;
    if (decode_properties(&props, header_buf)) {
        fprintf(stderr, "Error: Failed to decode LZMA properties.\n");
        fclose(fptr);
        return EXIT_FAILURE;
    }

    range_decoder rd;
    if (range_decoder_init(&rd, header_buf, HEADER_SIZE)) {

    fclose(fptr);
    return EXIT_SUCCESS;
}