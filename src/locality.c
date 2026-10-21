/*
 * locality.c - demonstrate the cache cost of access order.
 *
 * Sums the elements of a large N x N matrix twice: once in row-major order
 * (stride 1, cache-friendly) and once in column-major order (stride N, one cache
 * miss per access for large N). Prints the elapsed time for each so the locality
 * difference is visible, and reports the speedup.
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define N 4096

static int *matrix;

static double sum_row_major(void) {
    long long sum = 0;
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            sum += matrix[i * N + j]; /* contiguous: stride 1 */
    return (double)sum;
}

static double sum_col_major(void) {
    long long sum = 0;
    for (int j = 0; j < N; j++)
        for (int i = 0; i < N; i++)
            sum += matrix[i * N + j]; /* jumps by a full row each step */
    return (double)sum;
}

static double seconds_since(struct timespec start) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec - start.tv_sec) + (now.tv_nsec - start.tv_nsec) / 1e9;
}

int main(void) {
    matrix = malloc((size_t)N * N * sizeof(int));
    if (!matrix) {
        fprintf(stderr, "locality: out of memory\n");
        return 1;
    }
    for (int i = 0; i < N * N; i++) matrix[i] = i & 0xFF;

    struct timespec t0;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    volatile double r = sum_row_major();
    double row_time = seconds_since(t0);

    clock_gettime(CLOCK_MONOTONIC, &t0);
    volatile double c = sum_col_major();
    double col_time = seconds_since(t0);

    (void)r; (void)c;
    printf("matrix: %d x %d ints (%.1f MB)\n", N, N,
           (double)N * N * sizeof(int) / (1024 * 1024));
    printf("row-major sum:    %.4f s\n", row_time);
    printf("column-major sum: %.4f s\n", col_time);
    if (row_time > 0)
        printf("row-major is %.1fx faster\n", col_time / row_time);

    free(matrix);
    return 0;
}
