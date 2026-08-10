#include <menagerie/chameleon>

#include <gtest/gtest.h>

namespace chameleon = menagerie::chameleon;

TEST(InkSeparators, NewlineDefault) {
    EXPECT_EQ(chameleon::newline(), "\n");
    EXPECT_EQ(chameleon::newline(3), "\n\n\n");
}

TEST(InkSeparators, HrBuildsRule) {
    EXPECT_EQ(chameleon::hr(5), "-----");
    EXPECT_EQ(chameleon::hr(4, '='), "====");
}

TEST(InkSeparators, IndentEveryLine) {
    EXPECT_EQ(chameleon::indent("a\nb", 2), "  a\n  b");
}

TEST(InkSeparators, IndentPreservesTrailingNewline) {
    // Trailing '\n' does NOT pull an indent onto an empty final line.
    EXPECT_EQ(chameleon::indent("a\n", 2), "  a\n");
}

TEST(InkSeparators, PadLeft) {
    EXPECT_EQ(chameleon::pad("hi", 5), "hi   ");
}

TEST(InkSeparators, PadRight) {
    EXPECT_EQ(chameleon::pad("hi", 5, chameleon::Align::Right), "   hi");
}

TEST(InkSeparators, PadCenter) {
    EXPECT_EQ(chameleon::pad("hi", 6, chameleon::Align::Center), "  hi  ");
    EXPECT_EQ(chameleon::pad("hi", 5, chameleon::Align::Center), " hi  ");  // odd padding: extra space goes right
}

TEST(InkSeparators, PadNoopWhenAlreadyWide) {
    EXPECT_EQ(chameleon::pad("hello", 3), "hello");
}

TEST(InkSeparators, VisibleWidthSkipsAnsi) {
    const std::string s = chameleon::colors::make_red("abc");
    EXPECT_EQ(chameleon::detail::visible_width(s), 3u);
}

TEST(InkSeparators, VisibleWidthPlainAscii) {
    EXPECT_EQ(chameleon::detail::visible_width("hello"), 5u);
    EXPECT_EQ(chameleon::detail::visible_width(""), 0u);
}

TEST(InkSeparators, PadAnsiAwareWidth) {
    const std::string red_hi = chameleon::colors::make_red("hi");
    const std::string padded = chameleon::pad(red_hi, 5);
    // 3 spaces appended after red "hi" (visible width 2, target 5).
    EXPECT_EQ(padded, red_hi + "   ");
}

TEST(InkSeparators, LinesSplit) {
    const auto vec = chameleon::detail::lines("a\nb\nc");
    ASSERT_EQ(vec.size(), 3u);
    EXPECT_EQ(vec[0], "a");
    EXPECT_EQ(vec[1], "b");
    EXPECT_EQ(vec[2], "c");
}

TEST(InkSeparators, LinesEmptyInputYieldsOneEmpty) {
    const auto vec = chameleon::detail::lines("");
    ASSERT_EQ(vec.size(), 1u);
    EXPECT_EQ(vec[0], "");
}

TEST(InkSeparators, LinesTrailingNewlineYieldsEmptyTail) {
    const auto vec = chameleon::detail::lines("a\n");
    ASSERT_EQ(vec.size(), 2u);
    EXPECT_EQ(vec[0], "a");
    EXPECT_EQ(vec[1], "");
}
