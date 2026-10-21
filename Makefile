CC = cc
CFLAGS = -Wall -Wextra -Werror -O2 -g -std=c11

all: csim locality

csim: src/csim.c
	$(CC) $(CFLAGS) src/csim.c -o $@

locality: src/locality.c
	$(CC) $(CFLAGS) src/locality.c -o $@

# Replay the sample trace through a 2-set, direct-mapped, 4-byte-block cache.
# Hand-computed expected result: hits:3 misses:4 evictions:2
check: csim
	@out=$$(./csim -s 1 -E 1 -b 2 -t traces/sample.trace); \
	echo "$$out"; \
	[ "$$out" = "hits:3 misses:4 evictions:2" ] && echo "check: PASS" || { echo "check: FAIL"; exit 1; }

clean:
	rm -f csim locality
	rm -rf *.dSYM

.PHONY: all check clean
