#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#define HEADER_SIZE 13
#define PROPS_SIZE 5
#define LZMA_DIC_MIN (1 << 12)
#define IN_BUF_SIZE (64 * 1024)

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
#define NUM_STATES 12
#define StartOffset 1664

#define SpecPos (-StartOffset)
#define IsRep0Long (SpecPos + kNumFullDistances)
#define RepLenCoder (IsRep0Long + (kNumStates2 << kNumPosBitsMax))
#define LenCoder (RepLenCoder + kNumLenProbs)
#define IsMatch (LenCoder + kNumLenProbs)
#define Align (IsMatch + (kNumStates2 << kNumPosBitsMax))
#define IsRep (Align + kAlignTableSize)
#define IsRepG0 (IsRep + NUM_STATES)
#define IsRepG1 (IsRepG0 + NUM_STATES)
#define IsRepG2 (IsRepG1 + NUM_STATES)
#define PosSlot (IsRepG2 + NUM_STATES)
#define Literal (PosSlot + (kNumLenToPosStates << kNumPosSlotBits))

typedef enum {false, true} bool;

// state (probabilities and MRUD)
// State, 12 values, markov chain

/*
* Literal context: track the high lc bits of previous byte
* Literal position: tracks an array of (1 << lp) byteProbs
* Position bits: how many low bits of the decoder position do we care about
*/
typedef struct {
  uint8_t lc; // Literal context bits
  uint8_t lp; // Literal position bits
  uint8_t pb; // Position bits
  uint8_t padding; // padding for alignment
  uint32_t dictSize; // Dictionary size
} lzma_props;

typedef struct {
  uint32_t range;
  uint32_t code;

  FILE *file;
  bool eof;
  uint8_t in[IN_BUF_SIZE];
  size_t limit;
  size_t pos;
} range_decoder;

typedef struct {
  uint8_t *buf;
  uint32_t pos;
  uint32_t size;
  bool is_full;
} out_window;

inline uint8_t rd_get_byte(range_decoder *rd) {
  if (rd->pos >= rd->limit) {
    if (rd->eof) return 0xFF;
    rd->limit = fread(rd->in, 1, IN_BUF_SIZE, rd->file);
    rd->pos = 0;
    if (rd->limit == 0) {
      rd->eof = true;
      return 0xFF;
    }
  }
  return rd->in[rd->pos++];
}

void range_decoder_init(range_decoder *rd, FILE *f) {
  rd->range = 0xFFFFFFFF;
  rd->code = 0;
  rd->file = f;
  rd->eof = false;
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

  win->is_full = false;
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

  lzma_props *props;
  if (decode_properties(props, header)) {
    fprintf(stderr, "Error: Failed to decode LZMA properties.\n");
    fclose(fptr);
    return EXIT_FAILURE;
  }

  printf("\nlc=%d, lp=%d, pb=%d", props->lc, props->lp, props->pb);
  printf("\nDictionary Size in properties = %u", props->dictSize);

  uint64_t uncompressed_size;
  for (int i = 0; i < 8; i++) {
      uncompressed_size|= ((uint64_t)header[PROPS_SIZE + i]) << (8 * i);
  }

  // All 0xFF means unknown unknown uncompressed size (at the moment)
  // EOS marker is mandatory.
  // If not all 0xFF, EOS is optional
  bool knownUncompSize = (uncompressed_size != (int64_t)-1);

  range_decoder *rd;
  range_decoder_init(rd, fptr);

    
  out_window *win;
  out_window_init(win, props->dictSize);

  size_t num_probs = (StartOffset + (768 << (props->lc + props->lp)));
  uint16_t *probs;

  fclose(fptr);
  free(win->buf);
  return 0;
}
