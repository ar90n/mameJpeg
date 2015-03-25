#include <cstdlib>

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "../mameJpeg.h"
 
TEST_CASE("Bitsteram read test", "[sample]")
{
    uint8_t buffer[9] = { 0xfa, 0xab, 0x32, 0xb3, 0xf8, 0xc3, 0xaa, 0xaa, 0xbb };
    mameJpeg_memory_callback_param param = { buffer, 0, sizeof( buffer ) / sizeof( uint8_t ) };
    mameBitstream_context bitstream[1];
    mameBitstream_input_initialize( bitstream, mameJpeg_input_from_memory_callback, &param  );

    uint8_t data8 = 0;
    CHECK( mameBitstream_readBits(bitstream, &data8, 1, 1 ) );
    CHECK( data8 == 0x00 );
    CHECK( mameBitstream_readBits(bitstream, &data8, 1, 1 ) );
    CHECK( data8 == 0x01 );
    CHECK( mameBitstream_readBits(bitstream, &data8, 1, 1 ) );
    CHECK( data8 == 0x00 );
    CHECK( mameBitstream_readBits(bitstream, &data8, 1, 1 ) );
    CHECK( data8 == 0x01 );

    CHECK( mameBitstream_readBits(bitstream, &data8, 1, 3 ) );
    CHECK( data8 == 0x07 );
    CHECK( mameBitstream_readBits(bitstream, &data8, 1, 3 ) );
    CHECK( data8 == 0x07 );
  
    CHECK( mameBitstream_readBits(bitstream, &data8, 1, 8 ) );
    CHECK( data8 == 0xaa );

    uint16_t data16 = 0;
    CHECK( mameBitstream_readBits(bitstream, &data16, 2, 12 ) );
    CHECK( data16 == 0xccc );
 
    uint32_t data32 = 0;
    CHECK( mameBitstream_readBits(bitstream, &data32, 4, 18 ) );
    CHECK( data32 == 0x30fe2 );

    CHECK( mameBitstream_readBits(bitstream, &data8, 1, 8 ) );
    CHECK( data8 == 0xaa );

    CHECK( mameBitstream_readBits(bitstream, &data16, 2, 16 ) );
    CHECK( data16 == 0xbbaa );

}

TEST_CASE("Bitsteram write test", "[sample]")
{
    uint8_t buffer[9];
    mameBitstream_context bitstream[1];
    mameJpeg_memory_callback_param param = { buffer, 0, sizeof( buffer ) / sizeof( uint8_t ) };
    mameBitstream_output_initialize( bitstream, mameJpeg_output_to_memory_callback, &param  );

    uint8_t data8 = 0x00;
    CHECK( mameBitstream_writeBits(bitstream, &data8, 1 ) );
    data8 = 0x01;
    CHECK( mameBitstream_writeBits(bitstream, &data8, 1 ) );
    data8 = 0x00;
    CHECK( mameBitstream_writeBits(bitstream, &data8, 1 ) );
    data8 = 0x01;
    CHECK( mameBitstream_writeBits(bitstream, &data8, 1 ) );

    data8 = 0x07;
    CHECK( mameBitstream_writeBits(bitstream, &data8, 3 ) );
    data8 = 0x07;
    CHECK( mameBitstream_writeBits(bitstream, &data8, 3 ) );
  
    data8 = 0xaa;
    CHECK( mameBitstream_writeBits(bitstream, &data8, 8 ) );

    uint16_t data16 = 0xccc;
    CHECK( mameBitstream_writeBits(bitstream, &data16, 12 ) );
 
    uint32_t data32 = 0x30fe2;
    CHECK( mameBitstream_writeBits(bitstream, &data32, 18 ) );

    data8 = 0xaa;
    CHECK( mameBitstream_writeBits(bitstream, &data8, 8 ) );
    data16 = 0xbbaa;
    CHECK( mameBitstream_writeBits(bitstream, &data16, 16 ) );

    int res = memcmp( buffer, "\xfa\xab\x32\xb3\xf8\xc3\xaa\xaa\xbb", sizeof( buffer ) / sizeof( uint8_t ) );
    CHECK( res == 0 );
}
