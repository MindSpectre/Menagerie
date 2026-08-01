#include <menagerie/chameleon>

#include <gtest/gtest.h>

namespace chameleon = menagerie::chameleon;

TEST(InkColors, ColorizeWrapsWithCodeAndReset) {
    const auto s = chameleon::colors::colorize("hi", chameleon::colors::red);
    EXPECT_EQ(s, "\033[0;31mhi\033[0m");
}

TEST(InkColors, MakeRedRoundTrip) {
    EXPECT_EQ(chameleon::colors::make_red("err"), "\033[0;31merr\033[0m");
}

TEST(InkColors, MakeBoldRedRoundTrip) {
    EXPECT_EQ(chameleon::colors::make_bold_red("FATAL"), "\033[1;31mFATAL\033[0m");
}
