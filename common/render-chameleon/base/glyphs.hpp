#pragma once
#include <string_view>

namespace menagerie::chameleon::border {

    /// One box-drawing glyph set: the eleven characters a renderer (Box, Table, ...)
    /// draws borders with. Field values are raw UTF-8 byte sequences (or ASCII
    /// fallbacks), not display width -- pair with layout::visible_width for column math.
    struct Glyphs {
        std::string_view horizontal;    ///< Horizontal edge segment.
        std::string_view vertical;      ///< Vertical edge segment.
        std::string_view top_left;      ///< Top-left corner.
        std::string_view top_right;     ///< Top-right corner.
        std::string_view bottom_left;   ///< Bottom-left corner.
        std::string_view bottom_right;  ///< Bottom-right corner.
        std::string_view tee_top;       ///< T-junction opening downward.
        std::string_view tee_bottom;    ///< T-junction opening upward.
        std::string_view tee_left;      ///< T-junction opening rightward.
        std::string_view tee_right;     ///< T-junction opening leftward.
        std::string_view cross;         ///< Four-way junction.
    };

    /// Plain `-`/`|`/`+` fallback glyph set, for terminals without box-drawing support.
    static constexpr Glyphs ascii{
        .horizontal   = "-",
        .vertical     = "|",
        .top_left     = "+",
        .top_right    = "+",
        .bottom_left  = "+",
        .bottom_right = "+",
        .tee_top      = "+",
        .tee_bottom   = "+",
        .tee_left     = "+",
        .tee_right    = "+",
        .cross        = "+",
    };

    /// Default glyph set, using the Unicode box-drawing block (U+2500-U+257F).
    static constexpr Glyphs unicode{
        .horizontal   = "\xE2\x94\x80",  // U+2500 BOX DRAWINGS LIGHT HORIZONTAL
        .vertical     = "\xE2\x94\x82",  // U+2502 BOX DRAWINGS LIGHT VERTICAL
        .top_left     = "\xE2\x94\x8C",  // U+250C BOX DRAWINGS LIGHT DOWN AND RIGHT
        .top_right    = "\xE2\x94\x90",  // U+2510 BOX DRAWINGS LIGHT DOWN AND LEFT
        .bottom_left  = "\xE2\x94\x94",  // U+2514 BOX DRAWINGS LIGHT UP AND RIGHT
        .bottom_right = "\xE2\x94\x98",  // U+2518 BOX DRAWINGS LIGHT UP AND LEFT
        .tee_top      = "\xE2\x94\xAC",  // U+252C BOX DRAWINGS LIGHT DOWN AND HORIZONTAL
        .tee_bottom   = "\xE2\x94\xB4",  // U+2534 BOX DRAWINGS LIGHT UP AND HORIZONTAL
        .tee_left     = "\xE2\x94\x9C",  // U+251C BOX DRAWINGS LIGHT VERTICAL AND RIGHT
        .tee_right    = "\xE2\x94\xA4",  // U+2524 BOX DRAWINGS LIGHT VERTICAL AND LEFT
        .cross        = "\xE2\x94\xBC",  // U+253C BOX DRAWINGS LIGHT VERTICAL AND HORIZONTAL
    };

}  // namespace menagerie::chameleon::border
