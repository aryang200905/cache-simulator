/*
 * csim.c - a configurable set-associative cache simulator.
 *
 * Replays a Valgrind-style memory-access trace against a cache of the requested
 * geometry and reports the number of hits, misses, and evictions. Eviction uses
 * a least-recently-used (LRU) policy.
 *
 * Usage:
 *   csim [-v] -s <s> -E <E> -b <b> -t <tracefile>
 *     -s <s>  number of set-index bits  (S = 2^s sets)
 *     -E <E>  lines (ways) per set      (associativity)
 *     -b <b>  number of block-offset bits (B = 2^b bytes per block)
 *     -t <t>  path to the trace file
 *     -v      verbose: print per-access results
 *     -h      print this help
 *
 * Trace lines follow Valgrind's format, one access per line:
 *     I <addr>,<size>   instruction fetch (ignored)
 *    " L <addr>,<size>" data load
 *    " S <addr>,<size>" data store
 *    " M <addr>,<size>" data modify (a load followed by a store)
 */

#define _GNU_SOURCE
#include <getopt.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int valid;
    uint64_t tag;
    uint64_t last_used; /* LRU timestamp */
} cache_line_t;

typedef struct {
    int s, E, b;          /* geometry: set bits, ways, block bits */
    uint64_t S;           /* number of sets = 2^s */
    cache_line_t *lines;  /* S * E lines, row-major by set */
    uint64_t tick;        /* monotonic clock for LRU */
    int hits, misses, evictions;
} cache_t;

static int verbose = 0;

static cache_t *cache_create(int s, int E, int b) {
    cache_t *c = calloc(1, sizeof(*c));
    if (!c) return NULL;
    c->s = s;
    c->E = E;
    c->b = b;
    c->S = (uint64_t)1 << s;
    c->lines = calloc(c->S * (uint64_t)E, sizeof(cache_line_t));
    if (!c->lines) {
        free(c);
        return NULL;
    }
    return c;
}

static void cache_free(cache_t *c) {
    if (c) {
        free(c->lines);
        free(c);
    }
}

/* Simulate a single data access to the given address. */
static void access_cache(cache_t *c, uint64_t addr) {
    uint64_t set_index = (addr >> c->b) & (c->S - 1);
    uint64_t tag = addr >> (c->b + c->s);
    cache_line_t *set = &c->lines[set_index * (uint64_t)c->E];

    c->tick++;

    /* Hit? */
    for (int i = 0; i < c->E; i++) {
        if (set[i].valid && set[i].tag == tag) {
            set[i].last_used = c->tick;
            c->hits++;
            if (verbose) printf(" hit");
            return;
        }
    }

    /* Miss. Find an empty line, else evict the LRU line. */
    c->misses++;
    if (verbose) printf(" miss");

    int victim = -1;
    for (int i = 0; i < c->E; i++) {
        if (!set[i].valid) { victim = i; break; }
    }
    if (victim < 0) {
        victim = 0;
        for (int i = 1; i < c->E; i++) {
            if (set[i].last_used < set[victim].last_used) victim = i;
        }
        c->evictions++;
        if (verbose) printf(" eviction");
    }

    set[victim].valid = 1;
    set[victim].tag = tag;
    set[victim].last_used = c->tick;
}

static int run_trace(cache_t *c, const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "csim: cannot open trace file '%s'\n", path);
        return -1;
    }

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        char op;
        uint64_t addr;
        int size;
        /* The leading space in the format skips indentation and reads the op as
         * the first non-space character; this handles both " L .." and "I .." */
        if (sscanf(line, " %c %" SCNx64 ",%d", &op, &addr, &size) != 3) continue;
        if (op == 'I') continue; /* instruction fetches are not simulated */

        if (verbose) printf("%c %" PRIx64 ",%d", op, addr, size);
        access_cache(c, addr);        /* L, S, and the load half of M */
        if (op == 'M') access_cache(c, addr); /* the store half of M (a hit) */
        if (verbose) printf("\n");
    }
    fclose(fp);
    return 0;
}

static void usage(const char *prog) {
    fprintf(stderr,
            "Usage: %s [-v] -s <s> -E <E> -b <b> -t <tracefile>\n"
            "  -s <s>  number of set-index bits (S = 2^s sets)\n"
            "  -E <E>  associativity (lines per set)\n"
            "  -b <b>  number of block-offset bits (B = 2^b bytes)\n"
            "  -t <t>  path to the Valgrind trace file\n"
            "  -v      verbose output\n"
            "  -h      show this help\n",
            prog);
}

int main(int argc, char **argv) {
    int s = -1, E = -1, b = -1;
    const char *trace = NULL;
    int opt;

    while ((opt = getopt(argc, argv, "s:E:b:t:vh")) != -1) {
        switch (opt) {
        case 's': s = atoi(optarg); break;
        case 'E': E = atoi(optarg); break;
        case 'b': b = atoi(optarg); break;
        case 't': trace = optarg; break;
        case 'v': verbose = 1; break;
        case 'h': usage(argv[0]); return 0;
        default: usage(argv[0]); return 1;
        }
    }

    if (s < 0 || E <= 0 || b < 0 || !trace) {
        usage(argv[0]);
        return 1;
    }

    cache_t *c = cache_create(s, E, b);
    if (!c) {
        fprintf(stderr, "csim: out of memory\n");
        return 1;
    }

    if (run_trace(c, trace) != 0) {
        cache_free(c);
        return 1;
    }

    printf("hits:%d misses:%d evictions:%d\n", c->hits, c->misses, c->evictions);
    cache_free(c);
    return 0;
}
