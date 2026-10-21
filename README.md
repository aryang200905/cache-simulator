# Cache Simulator

A configurable set-associative **cache simulator** in C that replays
Valgrind-style memory-access traces and reports hits, misses, and evictions using
a **least-recently-used (LRU)** replacement policy. Includes a small benchmark
that demonstrates the real-world cost of poor spatial locality.

## Cache simulator (`csim`)

The cache geometry is fully configurable:

| Flag | Meaning |
|---|---|
| `-s <s>` | number of set-index bits → `2^s` sets |
| `-E <E>` | associativity (lines per set) |
| `-b <b>` | block-offset bits → `2^b` bytes per block |
| `-t <t>` | path to the trace file |
| `-v` | verbose: print the result of each access |

For each data access the simulator splits the address into tag / set-index /
block-offset, looks up the set, and on a miss fills an empty line or **evicts the
least-recently-used** line. `L` (load) and `S` (store) are one access each; `M`
(modify) is a load immediately followed by a store (so it always ends in a hit);
`I` (instruction fetch) lines are ignored.

```bash
make
./csim -s 4 -E 2 -b 4 -t traces/sample.trace       # 16 sets, 2-way, 16B blocks
./csim -v -s 1 -E 1 -b 2 -t traces/sample.trace     # verbose, direct-mapped
```

Example output:

```
hits:3 misses:4 evictions:2
```

### Trace format

Traces use Valgrind's `lackey` format, one access per line:

```
I 0400d7d4,8     # instruction fetch (ignored)
 L 0,1           # data load
 S 4,1           # data store
 M 4,1           # data modify = load + store
```

You can generate your own with:
`valgrind --log-fd=1 --tool=lackey -v --trace-mem=yes ./a.out`.

## Locality benchmark (`locality`)

`locality` sums a large matrix in **row-major** (stride 1) versus **column-major**
(stride N) order and times both, making the cost of cache-unfriendly access
patterns concrete:

```bash
make
./locality
```

```
matrix: 4096 x 4096 ints (64.0 MB)
row-major sum:    0.0096 s
column-major sum: 0.0708 s
row-major is 7.4x faster
```

## Building and testing

Requires a C compiler (tested with `cc`/clang and GCC) on Linux or macOS.

```bash
make        # build csim and locality
make check  # replay the sample trace and verify the counts
```

## License

Released under the [MIT License](LICENSE).
