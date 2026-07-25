#pragma once

#include <concepts>
#include <cstddef>
#include <format>
#include <iterator>
#include <menagerie/beavers>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "colors.hpp"
#include "layout.hpp"

namespace menagerie::chameleon {

    namespace detail {

        /// Rendering options shared by Table and TableFromRange.
        struct TableRenderOptions {
            const border::Glyphs* glyphs = &border::unicode;  ///< Border glyph set; must outlive the table.
            std::string_view header_style_prefix{};  ///< ANSI SGR prefix headers are colorized with, if any.
            std::size_t min_width    = 0;      ///< Minimum column width, before content is measured.
            bool terminate           = false;  ///< Whether render() ends with a trailing newline.
            Align default_cell_align = Align::Left;  ///< Alignment for columns with no column_aligns override.
            /// Sparse per-column alignment overrides: index c -> alignment. Indexes with
            /// no entry (or beyond size()) fall back to default_cell_align.
            std::vector<Align> column_aligns{};
        };

        /// Resolves column c's alignment: its column_aligns override if set, else default_cell_align.
        [[nodiscard]] constexpr Align cell_align_for(const TableRenderOptions& opts, const std::size_t c) noexcept {
            return c < opts.column_aligns.size() ? opts.column_aligns[c] : opts.default_cell_align;
        }

        /// Renders headers and column-major cells_cm into a bordered table string per opts.
        [[nodiscard]] constexpr std::string render_table(const std::vector<std::string>& headers,
                                                         const std::vector<std::vector<std::string>>& cells_cm,
                                                         const TableRenderOptions& opts) {
            if (headers.empty() && cells_cm.empty()) {
                return {};
            }

            const std::size_t col_count = headers.empty() ? cells_cm.size() : headers.size();
            const std::size_t row_count = cells_cm.empty() ? 0 : cells_cm[0].size();

            std::vector col_widths(col_count, opts.min_width);
            for (std::size_t c = 0; c < col_count; ++c) {
                if (c < headers.size()) {
                    col_widths[c] = std::max(col_widths[c], visible_width(headers[c]));
                }
                if (c < cells_cm.size()) {
                    for (const auto& cell : cells_cm[c]) {
                        for (const auto line : lines(cell)) {
                            col_widths[c] = std::max(col_widths[c], visible_width(line));
                        }
                    }
                }
            }

            std::vector<std::size_t> row_heights(row_count, 1);
            for (std::size_t r = 0; r < row_count; ++r) {
                for (std::size_t c = 0; c < col_count; ++c) {
                    if (c < cells_cm.size() && r < cells_cm[c].size()) {
                        row_heights[r] = std::max(row_heights[r], lines(cells_cm[c][r]).size());
                    }
                }
            }

            const auto& [horizontal,
                         vertical,
                         top_left,
                         top_right,
                         bottom_left,
                         bottom_right,
                         tee_top,
                         tee_bottom,
                         tee_left,
                         tee_right,
                         cross] = *opts.glyphs;

            auto emit_border = [&](std::string& out,
                                   const std::string_view left,
                                   const std::string_view junction,
                                   const std::string_view right) {
                out.append(left);
                for (std::size_t c = 0; c < col_count; ++c) {
                    for (std::size_t i = 0; i < col_widths[c] + 2; ++i) {
                        out.append(horizontal);
                    }
                    if (c + 1 < col_count) {
                        out.append(junction);
                    }
                }
                out.append(right);
                out.push_back('\n');
            };

            auto emit_cell_line =
                [&](std::string& out, const std::string_view text, const std::size_t width, const Align a) {
                    out.append(vertical);
                    out.push_back(' ');
                    out.append(pad(text, width, a));
                    out.push_back(' ');
                };

            std::string out;

            emit_border(out, top_left, tee_top, top_right);

            if (!headers.empty()) {
                for (std::size_t c = 0; c < col_count; ++c) {
                    const std::string_view header_text =
                        c < headers.size() ? std::string_view{headers[c]} : std::string_view{};
                    if (!opts.header_style_prefix.empty()) {
                        out.append(vertical);
                        out.push_back(' ');
                        out.append(colors::colorize(pad(header_text, col_widths[c]), opts.header_style_prefix));
                        out.push_back(' ');
                    } else {
                        emit_cell_line(out, header_text, col_widths[c], Align::Left);
                    }
                }
                out.append(vertical);
                out.push_back('\n');
                if (row_count > 0) {
                    emit_border(out, tee_left, cross, tee_right);
                }
            }

            for (std::size_t r = 0; r < row_count; ++r) {
                const std::size_t h = row_heights[r];
                std::vector<std::vector<std::string_view>> split_cells(col_count);
                for (std::size_t c = 0; c < col_count; ++c) {
                    if (c < cells_cm.size() && r < cells_cm[c].size()) {
                        split_cells[c] = lines(cells_cm[c][r]);
                    } else {
                        split_cells[c].emplace_back();
                    }
                }
                for (std::size_t line_idx = 0; line_idx < h; ++line_idx) {
                    for (std::size_t c = 0; c < col_count; ++c) {
                        const std::string_view line =
                            line_idx < split_cells[c].size() ? split_cells[c][line_idx] : std::string_view{};
                        emit_cell_line(out, line, col_widths[c], cell_align_for(opts, c));
                    }
                    out.append(vertical);
                    out.push_back('\n');
                }
                if (r + 1 < row_count) {
                    emit_border(out, tee_left, cross, tee_right);
                }
            }

            emit_border(out, bottom_left, tee_bottom, bottom_right);

            if (!opts.terminate && !out.empty() && out.back() == '\n') {
                out.pop_back();
            }
            return out;
        }

    }  // namespace detail

    /**
     * @brief Table built row-by-row: `.headers(...).add_row(...)`.
     *
     * Chained setters each return an rvalue/lvalue-forwarded reference to self.
     * add_row's argument count must match the column count set by headers(...).
     */
    class Table {
    public:
        constexpr Table() noexcept = default;

        /// Sets the column headers, resetting any previously added rows.
        template <typename Self, beavers::IsStringLike... Args>
        [[nodiscard]] constexpr auto&& headers(this Self&& self, Args&&... names) {
            self.headers_.clear();
            self.headers_.reserve(sizeof...(Args));
            (self.headers_.emplace_back(std::forward<Args>(names)), ...);
            self.cells_.resize(self.headers_.size());
            return std::forward<Self>(self);
        }

        /// Appends a row; values are formatted with std::format.
        /// @throw std::invalid_argument if the argument count does not match headers(...).
        template <typename Self, typename... Args>
        [[nodiscard]] auto&& add_row(this Self&& self, Args&&... values) {
            static_assert((std::formattable<std::remove_cvref_t<Args>, char> && ...),
                          "chameleon::Table::add_row — every argument must satisfy std::formattable<T, char>");
            constexpr std::size_t n = sizeof...(Args);
            if (self.cells_.size() != n) {
                throw std::invalid_argument("chameleon::Table::add_row — arity must match headers(...) count");
            }
            std::size_t c = 0;
            ((self.cells_[c++].emplace_back(std::format("{}", std::forward<Args>(values)))), ...);
            return std::forward<Self>(self);
        }

        /// Sets the glyph set the border is drawn with; g must outlive the Table.
        template <typename Self>
        [[nodiscard]] constexpr auto&& border(this Self&& self, const border::Glyphs& g) noexcept {
            self.opts_.glyphs = &g;
            return std::forward<Self>(self);
        }

        /// Sets an ANSI SGR prefix headers are colorized with.
        template <typename Self>
        [[nodiscard]] constexpr auto&& header_style(this Self&& self, const std::string_view ansi_prefix) noexcept {
            self.opts_.header_style_prefix = ansi_prefix;
            return std::forward<Self>(self);
        }

        /// Sets the minimum column width, before content is measured.
        template <typename Self>
        [[nodiscard]] constexpr auto&& min_width(this Self&& self, const std::size_t w) noexcept {
            self.opts_.min_width = w;
            return std::forward<Self>(self);
        }

        /// Sets whether render() ends with a trailing newline.
        template <typename Self>
        [[nodiscard]] constexpr auto&& terminate(this Self&& self, const bool on = true) noexcept {
            self.opts_.terminate = on;
            return std::forward<Self>(self);
        }

        /// Sets the default alignment applied to every data cell (headers always render left-aligned).
        template <typename Self>
        [[nodiscard]] constexpr auto&& align(this Self&& self, const Align a) noexcept {
            self.opts_.default_cell_align = a;
            return std::forward<Self>(self);
        }

        /// Overrides alignment for one column. index is 0-based over the columns declared via headers(...).
        template <typename Self>
        [[nodiscard]] constexpr auto&& column_align(this Self&& self, const std::size_t index, const Align a) {
            if (self.opts_.column_aligns.size() <= index) {
                self.opts_.column_aligns.resize(index + 1, self.opts_.default_cell_align);
            }
            self.opts_.column_aligns[index] = a;
            return std::forward<Self>(self);
        }

        /// Renders the configured table to a string.
        [[nodiscard]] std::string render() const {
            return render_table(headers_, cells_, opts_);
        }

    private:
        std::vector<std::string> headers_{};
        std::vector<std::vector<std::string>> cells_{};  // column-major
        detail::TableRenderOptions opts_{};
    };

    /**
     * @brief Table built from a range of structs by declaring column(name, fn) accessors
     *        instead of building rows by hand.
     *
     * RangeT controls ownership: an lvalue range is held by reference (zero-copy), an
     * rvalue range by value (owned, no dangling risk) -- see the free function table(range).
     */
    template <typename RangeT>
        requires std::ranges::range<RangeT>
    class TableFromRange {
    public:
        /// The element type column(name, fn)'s fn receives, decayed from *begin(range).
        using value_type = std::remove_cvref_t<decltype(*std::begin(std::declval<const RangeT&>()))>;

        /// Constructs from range; lvalues are held by reference, rvalues by value (see table(range)).
        template <typename RangeTp>
            requires std::is_same_v<std::remove_cvref_t<RangeT>, std::remove_cvref_t<RangeTp>> &&
                     (!std::is_same_v<std::remove_cvref_t<RangeTp>, TableFromRange>)
        constexpr explicit TableFromRange(RangeTp&& range) noexcept
            : range_{std::forward<RangeTp>(range)} {
        }

        /// Adds a column: name is the header, fn(row) computes each cell (formatted with std::format).
        template <typename Self, beavers::IsStringLike StringTp, typename Fn>
            requires std::invocable<Fn&, const value_type&>
        [[nodiscard]] constexpr auto&& column(this Self&& self, StringTp&& name, Fn fn) {
            using Result = std::invoke_result_t<Fn&, const value_type&>;
            static_assert(std::formattable<std::remove_cvref_t<Result>, char>,
                          "chameleon::Table::column — lambda's return type must satisfy std::formattable<T, char>");
            self.headers_.emplace_back(std::forward<StringTp>(name));
            std::vector<std::string> column_cells;
            column_cells.reserve(
                static_cast<std::size_t>(std::distance(std::begin(self.range_), std::end(self.range_))));
            for (const auto& row : self.range_) {
                column_cells.emplace_back(std::format("{}", fn(row)));
            }
            self.cells_.emplace_back(std::move(column_cells));
            return std::forward<Self>(self);
        }

        /// Sets the glyph set the border is drawn with; g must outlive the TableFromRange.
        template <typename Self>
        [[nodiscard]] constexpr auto&& border(this Self&& self, const border::Glyphs& g) noexcept {
            self.opts_.glyphs = &g;
            return std::forward<Self>(self);
        }

        /// Sets an ANSI SGR prefix headers are colorized with.
        template <typename Self>
        [[nodiscard]] constexpr auto&& header_style(this Self&& self, const std::string_view ansi_prefix) noexcept {
            self.opts_.header_style_prefix = ansi_prefix;
            return std::forward<Self>(self);
        }

        /// Sets the minimum column width, before content is measured.
        template <typename Self>
        [[nodiscard]] constexpr auto&& min_width(this Self&& self, const std::size_t w) noexcept {
            self.opts_.min_width = w;
            return std::forward<Self>(self);
        }

        /// Sets whether render() ends with a trailing newline.
        template <typename Self>
        [[nodiscard]] constexpr auto&& terminate(this Self&& self, const bool on = true) noexcept {
            self.opts_.terminate = on;
            return std::forward<Self>(self);
        }

        /// Sets the default alignment applied to every data cell (headers always render left-aligned).
        template <typename Self>
        [[nodiscard]] constexpr auto&& align(this Self&& self, const Align a) noexcept {
            self.opts_.default_cell_align = a;
            return std::forward<Self>(self);
        }

        /// Overrides alignment for one column. index is 0-based over the columns declared via column(...).
        template <typename Self>
        [[nodiscard]] constexpr auto&& column_align(this Self&& self, const std::size_t index, const Align a) {
            if (self.opts_.column_aligns.size() <= index) {
                self.opts_.column_aligns.resize(index + 1, self.opts_.default_cell_align);
            }
            self.opts_.column_aligns[index] = a;
            return std::forward<Self>(self);
        }

        /// Renders the configured table to a string.
        [[nodiscard]] std::string render() const {
            return render_table(headers_, cells_, opts_);
        }

    private:
        RangeT range_;
        std::vector<std::string> headers_{};
        std::vector<std::vector<std::string>> cells_{};
        detail::TableRenderOptions opts_{};
    };


    /// Starts an empty Table builder.
    [[nodiscard]] constexpr Table table() noexcept {
        return {};
    }

    /// Starts a TableFromRange builder over range: lvalues are held by reference
    /// (zero-copy, caller owns lifetime), rvalues are held by value (owned, no dangling risk).
    template <typename RangeTp>
        requires std::ranges::range<RangeTp>
    [[nodiscard]] constexpr auto table(RangeTp&& range) noexcept {
        using Held = std::conditional_t<std::is_lvalue_reference_v<RangeTp>, RangeTp, std::remove_cvref_t<RangeTp>>;
        return TableFromRange<Held>{std::forward<RangeTp>(range)};
    }


}  // namespace menagerie::chameleon
