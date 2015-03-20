#ifndef __MAME_JPEG_HEADER__
# define __MAME_JPEG_HEADER__

# ifdef __cplusplus
extern "C" {
# endif /* __cplusplus */

#include <ctype.h>

#define CHECK( X ) do{if( (X) == false ) return false; } while(0);
#define NULL_CHECK( X )  CHECK( (X) != NULL )

typedef struct {
    uint8_t* buffer;
    uint16_t cache;
    int cache_use_bits;
    size_t pos;
    size_t buffer_length;
} mameBitstream_context;

bool mameBitstream_initialize( mameBitstream_context* context, uint8_t* buffer, size_t buffer_length )
{
    NULL_CHECK( buffer )

    context->buffer = buffer;
    context->buffer_length = buffer_length
    context->cache = 0x0000;
    context->cache_use_bits = 0;
    context->pos = 0;

    return true;
}

bool mameBitstream_readByte( mameBitstream_context* context )
{
    NULL_CHECK( context )
    CHECK( ( context->pos + 1 ) < context->buffer_length )
    CHECK( context->cache_use_bits < 8 )

    context->cache = ( context->cache << 8 ) | ( context->buffer[ context->pos ] );
    context->cache_use_bits += 8;
    context->pos++;

    return true;
}

bool mameBitstream_readBits( mameBitstream_context* context, void* buffer, int buffer_length, int bits )
{
    NULL_CHECK( context )
    NULL_CHECK( buffer )
    CHECK( bits <= ( 8 * buffer_length ) )

    int buffer_pos = 0;
    while( 0 < bits )
    {
        if( context->cache_use_bits < bits )
        {
            bool ret = mameBitstream_readByte( context );
            if( !ret )
            {
                break;
            }
        }

        int read_bits = ( 8 < bits ) ? 8 : bits;
        uint8_t cache_mask = ( 1 << read_bits ) - 1;
        buffer[ buffer_pos ] = context->cache & cache_mask;
        context->cache >>= read_bits;
        context->cache_use_bits -= read_bits;
        bits -= read_bits;
    }

    return ( bits == 0 );
}














typedef struct {
} mameJpeg_context;





# ifdef __cplusplus
}
# endif /* __cplusplus */

#endif /* __MAME_JPEG_HEADER__ */
