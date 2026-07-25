# Math Library

The math library (`common/math/`) is a small random-value toolkit built on `std::mt19937`: a shared base that
owns and seeds the generator, a `NumberGenerator` for random integers, a `RandomTimeGenerator` for random
durations and dates, and a set of free functions for picking from or subsetting a collection. It has no
dependencies on any other Menagerie library. Everything is reached through `#include <menagerie/math>`
(`export/menagerie/math`).

## Key types

- **`BaseRandomGenerator`** -- owns the shared
  `std::mt19937` state, seeded from `std::random_device` by default or from a caller-supplied generator; base
  class for the two generator types below.
- **`NumberGenerator`** --
  `generate_random_uint32(min, max)` and a `generate_random_t<T>()` template for any integral `T`, uniform over
  `T`'s full range.
- **`RandomTimeGenerator`** --
  `generate_milliseconds(target_ms, deviation = 15)` returns a duration within `deviation` percent of
  `target_ms`; `generate_date(from, to)`, `generate_date_to(to)`, `generate_date_from_to_now(from)`, and
  `generate_date_last_century()` (anchored at 1925-07-25) generate a `std::chrono::year_month_day`;
  `generate_time_period(...)` variants return a `(start, end)` pair.
- **`random_utils.hpp`** free functions -- `pick(array)`,
  `pick(span)`, and `pick(begin, end)` return a random element (the span/iterator overloads return
  `std::optional`, empty on an empty range); `generate_subset(array)` and `generate_based_on_subset(array, fn)`
  keep each element with independent 50% probability; `generate_random_uuid_v4()` builds a random UUID string.

## Usage

```cpp
#include <menagerie/math>

#include <thread>

using namespace menagerie::math::random;

RandomTimeGenerator rnd;
const auto delay = rnd.generate_milliseconds(50, 30);  // ~50ms +/- 30%
std::this_thread::sleep_for(delay);

NumberGenerator numbers;
const auto n = numbers.generate_random_uint32(1, 100);
```

## Design notes

`BaseRandomGenerator::generator_` is `mutable`, and every `generate_*` member on `NumberGenerator` and
`RandomTimeGenerator` is declared `const` -- a generator can be held by `const&` or stored in a `const` object
while each call still advances the underlying `std::mt19937` state.
