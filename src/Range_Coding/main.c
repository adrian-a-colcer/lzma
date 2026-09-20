#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#define HEADER_SIZE 13
#define PROPS_SIZE 5
#define LZMA_DIC_MIN (1 << 12)
#define IN_BUF_SIZE (64 * 1024)

#define kNumBitModelTotalBits 11
#define kBitModelTotal (1 << kNumBitModelTotalBits)
#define kNumMoveBits 5

#define kNumTopBits 24
#define kTopValue ((uint32_t)1 << kNumTopBits)

#define kNumPosBitsMax 4
#define kNumPosStatesMax (1 << kNumPosBitsMax)

#define kLenNumLowBits 3
#define kLenNumLowSymbols (1 << kLenNumLowBits)
#define kLenNumHighBits 8
#define kLenNumHighSymbols (1 << kLenNumHighBits)

#define LenLow 0
#define LenHigh (LenLow + 2 * (kNumPosStatesMax << kLenNumLowBits))
#define kNumLenProbs (LenHigh + kLenNumHighSymbols)

#define LenChoice LenLow
#define LenChoice2 (LenLow + (1 << kLenNumLowBits))

#define kNumStates 12
#define kNumStates2 16
#define kNumLitStates 7

#define kStartPosModelIndex 4
#define kEndPosModelIndex 14
#define kNumFullDistances (1 << (kEndPosModelIndex >> 1))

#define kNumPosSlotBits 6
#define kNumLenToPosStates 4

#define kNumAlignBits 4
#define kAlignTableSize (1 << kNumAlignBits)

#define kMatchMinLen 2
#define kMatchSpecLenStart (kMatchMinLen + kLenNumLowSymbols * 2 + kLenNumHighSymbols)

#define kStartOffset 1664
#define GET_PROBS p->probs_1664

#define SpecPos (-kStartOffset)
#define IsRep0Long (SpecPos + kNumFullDistances)
#define RepLenCoder (IsRep0Long + (kNumStates2 << kNumPosBitsMax))
#define LenCoder (RepLenCoder + kNumLenProbs)
#define Match (LenCoder + kNumLenProbs)
#define Align (Match + (kNumStates2 << kNumPosBitsMax))
#define IsRep (Align + kAlignTableSize)
#define IsRepG0 (IsRep + kNumStates)
#define IsRepG1 (IsRepG0 + kNumStates)
#define IsRepG2 (IsRepG1 + kNumStates)
#define PosSlot (IsRepG2 + kNumStates)
#define Literal (PosSlot + (kNumLenToPosStates << kNumPosSlotBits))
#define NUM_BASE_PROBS (Literal + kStartOffset)
// Markov chain states
/*
State   Types of previous sequences
   -----   ---------------------------------------------
   0       literal, literal, literal
   1       match, literal, literal
   2       rep or (!literal, shortrep), literal, literal
   3       literal, shortrep, literal, literal
   4       match, literal
   5       rep or (!literal, shortrep), literal
   6       literal, shortrep, literal
   7       literal, match
   8       literal, rep
   9       literal, shortrep
   10      !literal, match
   11      !literal, (rep or shortrep)
*/
// state (probabilities and MRUD)
// State, 12 values, markov chain

/*
* Literal context: track the high lc bits of previous byte
* Literal position: tracks an array of (1 << lp) byteProbs
* Position bits: how many low bits of the decoder position do we care about
*/

typedef struct {
  uint8_t lc; // Literal context bits, how many of the high bits of the previous uncompressed byte to use
  uint8_t lp; // Literal position bits, how many of the low bits of the current uncompressed position to use
  uint8_t pb; // Position bits, how many of the low bits of the current uncompressed position to use for the match distance
  uint8_t padding; // padding for alignment
  uint32_t dictSize; // Dictionary size
} lzma_props;

typedef struct {
  uint32_t range;
  uint32_t code;

  uint16_t *probs;
  uint32_t num_probs;
  uint32_t state;

  FILE *file;
  int eof;
  uint8_t in[IN_BUF_SIZE];
  size_t limit;
  size_t pos;
} range_decoder;

typedef struct {
  uint8_t *buf;
  uint32_t pos;
  uint32_t size;
  int is_full;
} out_window;

void range_decoder_init(range_decoder *rd, FILE *f) {
  rd->range = 0xFFFFFFFF;
  rd->code = 0;
  rd->file = f;
  rd->eof = 0;
  rd->pos = 0;
  rd->limit = 0;
}

int out_window_init(out_window *win, size_t size) {
  if (win == NULL || size == 0) {
    return -1;
  }

  win->size = size;
  win->buf = malloc(size);

  if (win->buf == NULL) {
    win->size = 0;
    return -1;
  }

  win->is_full = 0;
  win->pos = 0;
  return 0;
}

int decode_properties(lzma_props *props, const uint8_t *data)
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



static inline unsigned dec_bit(range_decoder *rd, uint16_t *prob) {
  // Normalize - range becomes too small
  if (rd->range < kTopValue) {
    rd->range <<= 8; 
    rd->code = (rd->code << 8) | rd->in[rd->pos++];
  }

  /*
    portion the range into two parts based on probability of 0/1
    shift range left by 11 bits (since probabilities are stored as 11-bit values) to get unit range,
    then multiply by the probability of 0 (prob[0]) to get the bound for 0
  */
  uint32_t bound = (rd->range >> 11) * (uint32_t)(*(prob));

  if (rd->code < bound) {
    rd->range = bound;
    *(prob) = (uint16_t)(*(prob) + ((kBitModelTotal - *(prob)) >> kNumMoveBits));

    return 0;

  } else {
    rd->range -= bound;
    rd->code -= bound;

    *(prob) = (uint16_t)(*(prob) - (*(prob) >> kNumMoveBits));

    return 1;
  }
}

int decode(range_decoder *rd, out_window *win, lzma_props *props, uint64_t uncompressed_size, int knownUncompSize) {
  unsigned pbMask = ((unsigned)1 << (props->pb)) - 1;
  unsigned lpMask = ((unsigned)0x100 << props->lp) - ((unsigned)0x100 >> props->lc);
  do {
    uint16_t *prob;
    uint32_t bound;
    unsigned positionState = (((rd->pos) & (pbMask)) << 4);
    unsigned temp;

    // literal or match?
    prob = rd->probs + Match + positionState + rd->state;

    // first bit 0/1 : literal or match
    unsigned isMatch = dec_bit(rd, prob);

    if (isMatch == 0) {
      // 
      uint32_t symbol;

      // literal probabilities
      prob = rd->probs + Literal;
      if (rd->pos != 0) {
        prob += (uint32_t)3 * ((((rd->pos << 8) + win->buf[(win->pos == 0 ? win->size: win->pos) - 1]) & lpMask) << props->lc);
      }
      rd->pos++;

      // Last decoded byte was a literal
      if (rd->state < kNumLitStates) {
        rd->state = (rd->state < 4) ? rd->state : (rd->state - 3); 

        do { symbol = (symbol << 1) | dec_bit(rd, prob + symbol); }while (symbol < 0x100);

        // state > 6: last byte was a dictionary match
        // this is still literal decoding, 
      } else {
        unsigned matchByte = win->buf[win->pos - 
      }

      win->buf[win->pos++] = (uint8_t)symbol;
    } else {
      // first bit one
      rd->range -= bound;
      rd->code -= bound;
      *(prob) = (uint16_t)(*(prob) - (*(prob) >> kNumMoveBits));
    }

  } while (win->pos < win->size && rd->in < rd->limit);
  return 0;

}

int main(int argc, char *argv[]) {

  if (argc != 2) {
    fprintf(stderr, "Usage: %s <lzma_file>\n", argv[0]);
    return EXIT_FAILURE;
  }

  FILE *fptr = fopen(argv[1], "rb");
  if (!fptr) {
    perror("file opening failed");
    return EXIT_FAILURE;
  }

  uint8_t header[HEADER_SIZE];

  // Read the header (first 13 bytes)
  if (fread(header, 1, HEADER_SIZE, fptr) < HEADER_SIZE) {
    fprintf(stderr, "Error: File is too small to contain a valid LZMA header.\n");
    fclose(fptr);
    return EXIT_FAILURE;
  }

  if (ferror(fptr)) {
    perror("I/O error when reading");
  }

  lzma_props props;

  if (decode_properties(&props, header) != 0) {
    fprintf(stderr, "Error: Failed to decode LZMA properties.\n");
    fclose(fptr);
    return EXIT_FAILURE;
  }

  printf("\nlc=%d, lp=%d, pb=%d", props.lc, props.lp, props.pb);
  printf("\nDictionary Size in properties = %u", props.dictSize);

  uint64_t uncompressed_size;
  for (int i = 0; i < 8; i++) {
      uncompressed_size|= ((uint64_t)header[PROPS_SIZE + i]) << (8 * i);
  }

  // All 0xFF means unknown unknown uncompressed size (at the moment)
  // EOS marker is mandatory.
  // If not all 0xFF, EOS is optional
  int knownUncompSize = (uncompressed_size != (int64_t)-1);

  range_decoder rd;
  range_decoder_init(&rd, fptr);

  out_window win;
  out_window_init(&win, props.dictSize);

  size_t num_probs = (kStartOffset + (768 << (props.lc + props.lp)));

  fclose(fptr);
  free(win.buf);
  return 0;
}
