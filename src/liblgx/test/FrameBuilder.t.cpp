#include "catch_amalgamated.hpp"

#include "FrameBuilder.h"

utils::FrameBuilder frameBuilder;
uint32_t fakeFrameDataPart1[0x1FC000] = { 0 };
uint32_t fakeFrameDataPart2[0x1FC000] = { 0 };

TEST_CASE("Placeholder", "[Placeholder]") {
    REQUIRE(1 == 1);
}