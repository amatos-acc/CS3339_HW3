# CS3339 Homework 3 - Cache Simulator

This project is a single-file C++20 cache simulator based on the assignment
specification.

- Required behavior: set-associative cache hit/miss simulation
- Output file: `cache_sim_output`
- Input: command-line configuration + memory references read from a file
- Extra credit features implemented:
  - multi-word blocks
  - multi-level cache (L1 + optional L2)
  - miss classification (compulsory/capacity/conflict)

## Project Structure

- `main.cpp`: complete simulator implementation (all logic in one file)
- `data/sample_refs.txt`: sample memory reference file

## Build

```bash
make
```

## Run

```bash
./cache_sim 4 2 data/sample_refs.txt
```

### Command format

```bash
./cache_sim <num_entries> <associativity> <memory_reference_file> [options]
```

Optional extra-credit-oriented flags:

```bash
./cache_sim 8 2 data/sample_refs.txt --block-words 4 --l2-entries 16 --l2-assoc 4
```

Additional option:

```bash
--disable-miss-classification
```

## Output format

Each line corresponds to one input address:

- Hit: `ADDR : HIT (L1)` or `ADDR : HIT (L2)`
- Miss: `ADDR : MISS (COMPULSORY|CONFLICT|CAPACITY)` when miss classification is enabled
- Miss: `ADDR : MISS` when `--disable-miss-classification` is used

## Test

```bash
make run-tests
```

`make run-tests` currently runs the simulator with the sample input to verify
build + execution path and regenerate `cache_sim_output`.

## Known Limitations

- Miss type labels (`COMPULSORY`, `CONFLICT`, `CAPACITY`) are a best-effort
  estimate and may not match every textbook's exact method.
- The simulator only handles read-style memory references and expects addresses
  to be given as word addresses.
