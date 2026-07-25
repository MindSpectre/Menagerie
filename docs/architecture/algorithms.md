# Algorithms Library

The algorithms library (`common/algorithms/`) is a single header-only algorithm: `SlidingWindowSorter<T>`, a
bounded-memory streaming sort that keeps a running sorted window over data arriving in batches and flushes the
oldest entries to a consumer callback once the window fills up. It has no dependencies on any other Menagerie
library. Everything is reached through the umbrella `#include <menagerie/algorithms>`
(`export/menagerie/algorithms`).

## Key types

- **`SlidingWindowConfig<T>`** --
  tunables for a sorter instance: `window_size` (entries retained before flushing), `batch_size` (entries
  buffered before a sort/merge pass runs), `enable_sorting`, a `comparator`, and merge-strategy knobs
  `use_inplace_merge` / `merge_threshold`.
- **`DefaultComparator<T>`** -- the
  default `comparator`: calls `T::comp(a, b)` if the type provides a static `comp`, otherwise falls back to
  `operator<`.
- **`SlidingWindowSorter<T>`** -- the
  sorter itself: `add_entry(T)` / `add_entries(std::vector<T>)` feed data in, `flush()` forces processing of
  everything still buffered, `reconfigure(SlidingWindowConfig<T>)` flushes and swaps in a new config, and
  `get_statistics()` returns a `Statistics` snapshot (`total_processed`, `merge_operations`, `sort_operations`,
  `avg_merge_efficiency`).

## Usage

```cpp
#include <menagerie/algorithms>

using namespace menagerie::algorithms;

SlidingWindowConfig<int> config;
config.window_size = 1024;
config.batch_size  = 512;

SlidingWindowSorter<int> sorter(config, [](const std::vector<int>& sorted_batch) {
    // consume the oldest entries once the window overflows
});

sorter.add_entries({5, 1, 3, 2, 4});
sorter.flush();  // process everything still buffered
```

## Design notes

`add_entries` only triggers a sort/merge pass once `new_entries_` reaches `batch_size`; overflowing `window_size`
does not flush the excess immediately when that pass runs -- `calculate_output_count()` flushes
`max(excess, batch_size)` entries, so a single overflowing batch can emit more than the amount by which the
window overflowed. The merge step picks between `std::inplace_merge` (reusing already-reserved capacity, gated
by `can_use_inplace_merge()`) and a `std::merge` into a scratch buffer, purely as a memory/perf trade-off -- both
paths produce the same sorted window.
