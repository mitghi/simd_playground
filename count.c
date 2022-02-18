#include <stdio.h>
#include <assert.h>
#include <strings.h>
#include <stdlib.h>
#include <sys/time.h>
#include <arm_neon.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>

typedef struct arg_s {
    char *payload;
    size_t start;
    size_t end;
    bool readUntilEnd;
} arg_s;

atomic_ulong GCOUNTER = 0;

// count_chars_8 is copied from Josh Weinstein's Blog
size_t
count_chars_8(const char* data, size_t size, const char ch)
{
	size_t total = 0;
	while (size) {
		if (*data == ch)
			total += 1;
		data += 1;
		size -= 1;
	}
	return total;
};

// from sse2neon repo
static inline __attribute__((always_inline)) int32_t _mm_movemask_epi8_neon(uint8x16_t input)
{
    const int8_t __attribute__ ((aligned (16))) xr[8] = {-7,-6,-5,-4,-3,-2,-1,0};
    uint8x8_t mask_and = vdup_n_u8(0x80);
    int8x8_t mask_shift = vld1_s8(xr);

    uint8x8_t lo = vget_low_u8(input);
    uint8x8_t hi = vget_high_u8(input);

    lo = vand_u8(lo, mask_and);
    lo = vshl_u8(lo, mask_shift);

    hi = vand_u8(hi, mask_and);
    hi = vshl_u8(hi, mask_shift);

    lo = vpadd_u8(lo,lo);
    lo = vpadd_u8(lo,lo);
    lo = vpadd_u8(lo,lo);

    hi = vpadd_u8(hi,hi);
    hi = vpadd_u8(hi,hi);
    hi = vpadd_u8(hi,hi);

    return ((hi[0] << 8) | (lo[0] & 0xFF));
}

unsigned long
neon_count_chars_128(const char* data, size_t size, const char ch)
{
	unsigned long total = 0;
	assert(size % 16 == 0);
	uint8x16_t tocmp =  vdupq_n_u8((char)(ch));
	while (size) {
		int mask = 0;
		uint8x16_t chunk =  *(const uint8x16_t*)&data[0];
		uint8x16_t results =  vceqq_u8(chunk, tocmp);
		mask = _mm_movemask_epi8_neon(results);
		total += __builtin_popcount(mask);
		data += 16;
		size -= 16;
	}
	return total;
};

unsigned long
neon_thread(const char* data, size_t size, const char ch, bool readUntilEnd)
{
	unsigned long total = 0;
	assert(size % 16 == 0);
	uint8x16_t tocmp =  vdupq_n_u8((char)(ch));
	while (size) {
		int mask = 0;
		uint8x16_t chunk =  *(const uint8x16_t*)&data[0];
		uint8x16_t results =  vceqq_u8(chunk, tocmp);
		mask = _mm_movemask_epi8_neon(results);
		total += __builtin_popcount(mask);
		data += 16;
		size -= 16;
	}
	return total;
};


void
neon_simd(char *string, size_t ln)
{
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    unsigned long result = neon_count_chars_128(string, ln, ',');
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    uint64_t delta_us = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000;
    printf("[x] Neon_SIMD           took %llu u/s  count: %lu commas\n", delta_us, result);
};

void
normal(char *string)
{
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    unsigned long result = count_chars_8(string, strlen(string), ',');
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    uint64_t delta_us = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000;
    printf("[x] NORMAL              took %llu u/s  count: %lu commas\n", delta_us, result);
};

void* procThread(void *input)
{
    arg_s *s = (arg_s*)input;

    unsigned long cnt = neon_thread(&s->payload[0], s->end, ',', s->readUntilEnd);
    GCOUNTER += cnt;

    return NULL;
};

void
neon_with_threads(char *string, size_t fsize)
{
    pthread_t threads[2];

    arg_s arg1 = {.payload=string, .start=0, .end=fsize/2, .readUntilEnd=false};
    arg_s arg2 = {.payload=string, .start=fsize/2, .end=fsize - (fsize/2), .readUntilEnd=false};

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    pthread_create(&threads[0], NULL, procThread, (void*)&arg1);
    pthread_create(&threads[1], NULL, procThread, (void*)&arg2);

    for (int t = 0; t < 2; t++) {
        pthread_join(threads[t], NULL);
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    uint64_t delta_us = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000;
    printf("[x] Neon_SIMD_THREAD(S) took %llu u/s  count: %zu commas\n", delta_us, GCOUNTER);
};

int
main(int argc, char *argv[])
{
    char *fpath = NULL;

    if(argc <2) {
        puts("first argument filepath is missing");
        exit(1);
    }
    fpath = argv[1];

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    FILE *f = fopen(fpath, "rb");
    if (f == NULL) {
        puts("file not found");
        exit(1);
    }
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *string = malloc(fsize + 1);
    fread(string, fsize, 1, f);
    fclose(f);
    string[fsize] = 0;

    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    uint64_t delta_us = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000;

    printf("[+] File loaded ( took %llu u/s )\n", delta_us);

    normal(string);
    neon_simd(string, fsize);
    neon_with_threads(string, fsize);

    free(string);

    return 0;
};
