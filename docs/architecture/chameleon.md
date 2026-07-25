# Chameleon Library

The chameleon library (`common/chameleon/`) is Menagerie's terminal text-formatting toolkit: ANSI color
helpers, box-drawing glyph sets, alignment/padding primitives, and three composable renderers -- `Box`,
`Section`, and `Table` -- for turning structured data into readable console or log output. Crow's `ConsoleSink`
uses it for level-based colorization; anywhere else that needs a bordered block, a label/value summary, or a
tabular dump reaches for it directly. Everything is reached through `#include <menagerie/chameleon>`
(`export/menagerie/chameleon`).

## Key types

- **`colors::make_red` / `make_green` / `make_bold_red` / ... and `colors::colorize(text, code)`**
  -- wrap text in an ANSI SGR code and a trailing reset; every standard color/style/background combination has a
  `make_*` helper.
- **`border::Glyphs`, `border::ascii`, `border::unicode`** -- the
  eleven box-drawing characters (corners, tees, cross, horizontal/vertical) a renderer draws with; `unicode` is
  the default, `ascii` is a plain `-`/`|`/`+` fallback.
- **`Align`, `pad(text, width, align)`, `indent(...)`, `hr(...)`, `newline(...)`**
  -- layout primitives shared by every renderer, including a `visible_width(...)` that skips ANSI escapes and
  UTF-8 continuation bytes when computing column width.
- **`Box` / `box(body)`** -- a bordered block around one body
  string, with an optional centered `title(...)`, configurable `border(...)` glyph set, `border_style(...)`
  color, and `padding(...)`.
- **`Section` / `section(title)`** -- a title plus a
  list of `row(label, value)` pairs rendered as an aligned `label:  value` block, with multi-line values
  indented to the value column.
- **`Table` / `table()`** -- a row-by-row table builder:
  `headers(...)` then repeated `add_row(...)`, with per-column `column_align(...)` and a configurable
  `header_style(...)`.
- **`TableFromRange<RangeT>` / `table(range)`** -- a table
  built from a range of structs by declaring `column(name, fn)` accessors instead of building rows by hand.

## Usage

```cpp
#include <menagerie/chameleon>

namespace chameleon = menagerie::chameleon;

std::string summary = chameleon::box("Startup complete").title("Server").render();

std::string status = chameleon::section("Summary")
                          .row("Users", 42)
                          .row("Latency p99", "12ms")
                          .render();

std::string report = chameleon::table()
                          .headers("name", "p99")
                          .add_row("insert", 1.2)
                          .add_row("select_by_pk", 0.3)
                          .render();
```

## Design notes

`table(range)` picks its storage mode from the argument's value category: an lvalue range is held by reference
(`TableFromRange<RangeT&>`, zero-copy, caller keeps ownership), while an rvalue range is held by value
(`TableFromRange<std::remove_cvref_t<RangeT>>`, owned by the table, no dangling risk). Both instantiate the same
`TableFromRange<Held>` template, so `.column(...).render()` reads identically either way; only the deduced
`Held` type controls whether the range is copied.
