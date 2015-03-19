#define CATCH_CONFIG_MAIN

#include "catch.hpp"

int f()
{
    return 2;
}
 
TEST_CASE("Test", "[sample]")
{
    CHECK( f() == 2 );
    REQUIRE( f() <= 0 );
}
