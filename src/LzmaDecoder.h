#include <stdint.h>

typedef struct {
    uint8_t lc; // Literal context bits
    uint8_t lp; // Literal position bits
    uint8_t pb; // Position bits
    uint8_t padding; // padding for alignment
    uint32_t dictSize; // Dictionary size
} LzmaProps;

typedef struct {
    uint32_t range;
    uint32_t code;
    uint8_t *buffer;
    uint8_t corrupted;
} range_decoder;