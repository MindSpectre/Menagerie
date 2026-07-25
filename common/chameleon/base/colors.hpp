#pragma once

#include <string>
#include <string_view>


namespace menagerie::chameleon::colors {

    constexpr std::string_view reset = "\033[0m";  ///< SGR reset code appended after every colorized string.

    // -------- Foreground --------
    constexpr std::string_view black   = "\033[0;30m";  ///< Black foreground.
    constexpr std::string_view red     = "\033[0;31m";  ///< Red foreground.
    constexpr std::string_view green   = "\033[0;32m";  ///< Green foreground.
    constexpr std::string_view yellow  = "\033[0;33m";  ///< Yellow foreground.
    constexpr std::string_view blue    = "\033[0;34m";  ///< Blue foreground.
    constexpr std::string_view magenta = "\033[0;35m";  ///< Magenta foreground.
    constexpr std::string_view cyan    = "\033[0;36m";  ///< Cyan foreground.
    constexpr std::string_view white   = "\033[0;37m";  ///< White foreground.

    // -------- Bold foreground --------
    constexpr std::string_view bold_black   = "\033[1;30m";  ///< Bold black foreground.
    constexpr std::string_view bold_red     = "\033[1;31m";  ///< Bold red foreground.
    constexpr std::string_view bold_green   = "\033[1;32m";  ///< Bold green foreground.
    constexpr std::string_view bold_yellow  = "\033[1;33m";  ///< Bold yellow foreground.
    constexpr std::string_view bold_blue    = "\033[1;34m";  ///< Bold blue foreground.
    constexpr std::string_view bold_magenta = "\033[1;35m";  ///< Bold magenta foreground.
    constexpr std::string_view bold_cyan    = "\033[1;36m";  ///< Bold cyan foreground.
    constexpr std::string_view bold_white   = "\033[1;37m";  ///< Bold white foreground.

    // -------- Underlined foreground --------
    constexpr std::string_view underline_black   = "\033[4;30m";  ///< Underlined black foreground.
    constexpr std::string_view underline_red     = "\033[4;31m";  ///< Underlined red foreground.
    constexpr std::string_view underline_green   = "\033[4;32m";  ///< Underlined green foreground.
    constexpr std::string_view underline_yellow  = "\033[4;33m";  ///< Underlined yellow foreground.
    constexpr std::string_view underline_blue    = "\033[4;34m";  ///< Underlined blue foreground.
    constexpr std::string_view underline_magenta = "\033[4;35m";  ///< Underlined magenta foreground.
    constexpr std::string_view underline_cyan    = "\033[4;36m";  ///< Underlined cyan foreground.
    constexpr std::string_view underline_white   = "\033[4;37m";  ///< Underlined white foreground.

    // -------- Background --------
    constexpr std::string_view background_black   = "\033[40m";  ///< Black background.
    constexpr std::string_view background_red     = "\033[41m";  ///< Red background.
    constexpr std::string_view background_green   = "\033[42m";  ///< Green background.
    constexpr std::string_view background_yellow  = "\033[43m";  ///< Yellow background.
    constexpr std::string_view background_blue    = "\033[44m";  ///< Blue background.
    constexpr std::string_view background_magenta = "\033[45m";  ///< Magenta background.
    constexpr std::string_view background_cyan    = "\033[46m";  ///< Cyan background.
    constexpr std::string_view background_white   = "\033[47m";  ///< White background.

    // -------- High-intensity (bright) foreground --------
    constexpr std::string_view hi_black   = "\033[0;90m";  ///< Bright black foreground.
    constexpr std::string_view hi_red     = "\033[0;91m";  ///< Bright red foreground.
    constexpr std::string_view hi_green   = "\033[0;92m";  ///< Bright green foreground.
    constexpr std::string_view hi_yellow  = "\033[0;93m";  ///< Bright yellow foreground.
    constexpr std::string_view hi_blue    = "\033[0;94m";  ///< Bright blue foreground.
    constexpr std::string_view hi_magenta = "\033[0;95m";  ///< Bright magenta foreground.
    constexpr std::string_view hi_cyan    = "\033[0;96m";  ///< Bright cyan foreground.
    constexpr std::string_view hi_white   = "\033[0;97m";  ///< Bright white foreground.

    // -------- Bold high-intensity foreground --------
    constexpr std::string_view bold_hi_black   = "\033[1;90m";  ///< Bold bright black foreground.
    constexpr std::string_view bold_hi_red     = "\033[1;91m";  ///< Bold bright red foreground.
    constexpr std::string_view bold_hi_green   = "\033[1;92m";  ///< Bold bright green foreground.
    constexpr std::string_view bold_hi_yellow  = "\033[1;93m";  ///< Bold bright yellow foreground.
    constexpr std::string_view bold_hi_blue    = "\033[1;94m";  ///< Bold bright blue foreground.
    constexpr std::string_view bold_hi_magenta = "\033[1;95m";  ///< Bold bright magenta foreground.
    constexpr std::string_view bold_hi_cyan    = "\033[1;96m";  ///< Bold bright cyan foreground.
    constexpr std::string_view bold_hi_white   = "\033[1;97m";  ///< Bold bright white foreground.

    // -------- High-intensity background --------
    constexpr std::string_view hi_background_black   = "\033[0;100m";  ///< Bright black background.
    constexpr std::string_view hi_background_red     = "\033[0;101m";  ///< Bright red background.
    constexpr std::string_view hi_background_green   = "\033[0;102m";  ///< Bright green background.
    constexpr std::string_view hi_background_yellow  = "\033[0;103m";  ///< Bright yellow background.
    constexpr std::string_view hi_background_blue    = "\033[0;104m";  ///< Bright blue background.
    constexpr std::string_view hi_background_magenta = "\033[0;105m";  ///< Bright magenta background.
    constexpr std::string_view hi_background_cyan    = "\033[0;106m";  ///< Bright cyan background.
    constexpr std::string_view hi_background_white   = "\033[0;107m";  ///< Bright white background.


    /// Wraps text in code and appends colors::reset so the color reverts after text.
    [[nodiscard]] constexpr std::string colorize(const std::string_view text, const std::string_view code) {
        std::string result;
        result.reserve(code.size() + text.size() + reset.size());
        result.append(code).append(text).append(reset);
        return result;
    }

    // -------- Foreground helpers --------
    /// Wraps text in colors::black (see colorize()).
    [[nodiscard]] constexpr std::string make_black(const std::string_view text) {
        return colorize(text, black);
    }
    /// Wraps text in colors::red (see colorize()).
    [[nodiscard]] constexpr std::string make_red(const std::string_view text) {
        return colorize(text, red);
    }
    /// Wraps text in colors::green (see colorize()).
    [[nodiscard]] constexpr std::string make_green(const std::string_view text) {
        return colorize(text, green);
    }
    /// Wraps text in colors::yellow (see colorize()).
    [[nodiscard]] constexpr std::string make_yellow(const std::string_view text) {
        return colorize(text, yellow);
    }
    /// Wraps text in colors::blue (see colorize()).
    [[nodiscard]] constexpr std::string make_blue(const std::string_view text) {
        return colorize(text, blue);
    }
    /// Wraps text in colors::magenta (see colorize()).
    [[nodiscard]] constexpr std::string make_magenta(const std::string_view text) {
        return colorize(text, magenta);
    }
    /// Wraps text in colors::cyan (see colorize()).
    [[nodiscard]] constexpr std::string make_cyan(const std::string_view text) {
        return colorize(text, cyan);
    }
    /// Wraps text in colors::white (see colorize()).
    [[nodiscard]] constexpr std::string make_white(const std::string_view text) {
        return colorize(text, white);
    }

    // -------- Background helpers --------
    /// Wraps text in colors::background_black (see colorize()).
    [[nodiscard]] constexpr std::string make_background_black(const std::string_view text) {
        return colorize(text, background_black);
    }
    /// Wraps text in colors::background_red (see colorize()).
    [[nodiscard]] constexpr std::string make_background_red(const std::string_view text) {
        return colorize(text, background_red);
    }
    /// Wraps text in colors::background_green (see colorize()).
    [[nodiscard]] constexpr std::string make_background_green(const std::string_view text) {
        return colorize(text, background_green);
    }
    /// Wraps text in colors::background_yellow (see colorize()).
    [[nodiscard]] constexpr std::string make_background_yellow(const std::string_view text) {
        return colorize(text, background_yellow);
    }
    /// Wraps text in colors::background_blue (see colorize()).
    [[nodiscard]] constexpr std::string make_background_blue(const std::string_view text) {
        return colorize(text, background_blue);
    }
    /// Wraps text in colors::background_magenta (see colorize()).
    [[nodiscard]] constexpr std::string make_background_magenta(const std::string_view text) {
        return colorize(text, background_magenta);
    }
    /// Wraps text in colors::background_cyan (see colorize()).
    [[nodiscard]] constexpr std::string make_background_cyan(const std::string_view text) {
        return colorize(text, background_cyan);
    }
    /// Wraps text in colors::background_white (see colorize()).
    [[nodiscard]] constexpr std::string make_background_white(const std::string_view text) {
        return colorize(text, background_white);
    }

    // -------- Bold foreground helpers --------
    /// Wraps text in colors::bold_black (see colorize()).
    [[nodiscard]] constexpr std::string make_bold_black(const std::string_view text) {
        return colorize(text, bold_black);
    }
    /// Wraps text in colors::bold_red (see colorize()).
    [[nodiscard]] constexpr std::string make_bold_red(const std::string_view text) {
        return colorize(text, bold_red);
    }
    /// Wraps text in colors::bold_green (see colorize()).
    [[nodiscard]] constexpr std::string make_bold_green(const std::string_view text) {
        return colorize(text, bold_green);
    }
    /// Wraps text in colors::bold_yellow (see colorize()).
    [[nodiscard]] constexpr std::string make_bold_yellow(const std::string_view text) {
        return colorize(text, bold_yellow);
    }
    /// Wraps text in colors::bold_blue (see colorize()).
    [[nodiscard]] constexpr std::string make_bold_blue(const std::string_view text) {
        return colorize(text, bold_blue);
    }
    /// Wraps text in colors::bold_magenta (see colorize()).
    [[nodiscard]] constexpr std::string make_bold_magenta(const std::string_view text) {
        return colorize(text, bold_magenta);
    }
    /// Wraps text in colors::bold_cyan (see colorize()).
    [[nodiscard]] constexpr std::string make_bold_cyan(const std::string_view text) {
        return colorize(text, bold_cyan);
    }
    /// Wraps text in colors::bold_white (see colorize()).
    [[nodiscard]] constexpr std::string make_bold_white(const std::string_view text) {
        return colorize(text, bold_white);
    }

    // -------- Underlined foreground helpers --------
    /// Wraps text in colors::underline_black (see colorize()).
    [[nodiscard]] constexpr std::string make_underline_black(const std::string_view text) {
        return colorize(text, underline_black);
    }
    /// Wraps text in colors::underline_red (see colorize()).
    [[nodiscard]] constexpr std::string make_underline_red(const std::string_view text) {
        return colorize(text, underline_red);
    }
    /// Wraps text in colors::underline_green (see colorize()).
    [[nodiscard]] constexpr std::string make_underline_green(const std::string_view text) {
        return colorize(text, underline_green);
    }
    /// Wraps text in colors::underline_yellow (see colorize()).
    [[nodiscard]] constexpr std::string make_underline_yellow(const std::string_view text) {
        return colorize(text, underline_yellow);
    }
    /// Wraps text in colors::underline_blue (see colorize()).
    [[nodiscard]] constexpr std::string make_underline_blue(const std::string_view text) {
        return colorize(text, underline_blue);
    }
    /// Wraps text in colors::underline_magenta (see colorize()).
    [[nodiscard]] constexpr std::string make_underline_magenta(const std::string_view text) {
        return colorize(text, underline_magenta);
    }
    /// Wraps text in colors::underline_cyan (see colorize()).
    [[nodiscard]] constexpr std::string make_underline_cyan(const std::string_view text) {
        return colorize(text, underline_cyan);
    }
    /// Wraps text in colors::underline_white (see colorize()).
    [[nodiscard]] constexpr std::string make_underline_white(const std::string_view text) {
        return colorize(text, underline_white);
    }

    // -------- High-intensity foreground helpers --------
    /// Wraps text in colors::hi_black (see colorize()).
    [[nodiscard]] constexpr std::string make_hi_black(const std::string_view text) {
        return colorize(text, hi_black);
    }
    /// Wraps text in colors::hi_red (see colorize()).
    [[nodiscard]] constexpr std::string make_hi_red(const std::string_view text) {
        return colorize(text, hi_red);
    }
    /// Wraps text in colors::hi_green (see colorize()).
    [[nodiscard]] constexpr std::string make_hi_green(const std::string_view text) {
        return colorize(text, hi_green);
    }
    /// Wraps text in colors::hi_yellow (see colorize()).
    [[nodiscard]] constexpr std::string make_hi_yellow(const std::string_view text) {
        return colorize(text, hi_yellow);
    }
    /// Wraps text in colors::hi_blue (see colorize()).
    [[nodiscard]] constexpr std::string make_hi_blue(const std::string_view text) {
        return colorize(text, hi_blue);
    }
    /// Wraps text in colors::hi_magenta (see colorize()).
    [[nodiscard]] constexpr std::string make_hi_magenta(const std::string_view text) {
        return colorize(text, hi_magenta);
    }
    /// Wraps text in colors::hi_cyan (see colorize()).
    [[nodiscard]] constexpr std::string make_hi_cyan(const std::string_view text) {
        return colorize(text, hi_cyan);
    }
    /// Wraps text in colors::hi_white (see colorize()).
    [[nodiscard]] constexpr std::string make_hi_white(const std::string_view text) {
        return colorize(text, hi_white);
    }

    // -------- Bold high-intensity foreground helpers --------
    /// Wraps text in colors::bold_hi_black (see colorize()).
    [[nodiscard]] constexpr std::string make_bold_hi_black(const std::string_view text) {
        return colorize(text, bold_hi_black);
    }
    /// Wraps text in colors::bold_hi_red (see colorize()).
    [[nodiscard]] constexpr std::string make_bold_hi_red(const std::string_view text) {
        return colorize(text, bold_hi_red);
    }
    /// Wraps text in colors::bold_hi_green (see colorize()).
    [[nodiscard]] constexpr std::string make_bold_hi_green(const std::string_view text) {
        return colorize(text, bold_hi_green);
    }
    /// Wraps text in colors::bold_hi_yellow (see colorize()).
    [[nodiscard]] constexpr std::string make_bold_hi_yellow(const std::string_view text) {
        return colorize(text, bold_hi_yellow);
    }
    /// Wraps text in colors::bold_hi_blue (see colorize()).
    [[nodiscard]] constexpr std::string make_bold_hi_blue(const std::string_view text) {
        return colorize(text, bold_hi_blue);
    }
    /// Wraps text in colors::bold_hi_magenta (see colorize()).
    [[nodiscard]] constexpr std::string make_bold_hi_magenta(const std::string_view text) {
        return colorize(text, bold_hi_magenta);
    }
    /// Wraps text in colors::bold_hi_cyan (see colorize()).
    [[nodiscard]] constexpr std::string make_bold_hi_cyan(const std::string_view text) {
        return colorize(text, bold_hi_cyan);
    }
    /// Wraps text in colors::bold_hi_white (see colorize()).
    [[nodiscard]] constexpr std::string make_bold_hi_white(const std::string_view text) {
        return colorize(text, bold_hi_white);
    }

    // -------- High-intensity background helpers --------
    /// Wraps text in colors::hi_background_black (see colorize()).
    [[nodiscard]] constexpr std::string make_hi_background_black(const std::string_view text) {
        return colorize(text, hi_background_black);
    }
    /// Wraps text in colors::hi_background_red (see colorize()).
    [[nodiscard]] constexpr std::string make_hi_background_red(const std::string_view text) {
        return colorize(text, hi_background_red);
    }
    /// Wraps text in colors::hi_background_green (see colorize()).
    [[nodiscard]] constexpr std::string make_hi_background_green(const std::string_view text) {
        return colorize(text, hi_background_green);
    }
    /// Wraps text in colors::hi_background_yellow (see colorize()).
    [[nodiscard]] constexpr std::string make_hi_background_yellow(const std::string_view text) {
        return colorize(text, hi_background_yellow);
    }
    /// Wraps text in colors::hi_background_blue (see colorize()).
    [[nodiscard]] constexpr std::string make_hi_background_blue(const std::string_view text) {
        return colorize(text, hi_background_blue);
    }
    /// Wraps text in colors::hi_background_magenta (see colorize()).
    [[nodiscard]] constexpr std::string make_hi_background_magenta(const std::string_view text) {
        return colorize(text, hi_background_magenta);
    }
    /// Wraps text in colors::hi_background_cyan (see colorize()).
    [[nodiscard]] constexpr std::string make_hi_background_cyan(const std::string_view text) {
        return colorize(text, hi_background_cyan);
    }
    /// Wraps text in colors::hi_background_white (see colorize()).
    [[nodiscard]] constexpr std::string make_hi_background_white(const std::string_view text) {
        return colorize(text, hi_background_white);
    }

}  // namespace menagerie::chameleon::colors
