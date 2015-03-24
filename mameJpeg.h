#ifndef __MAME_JPEG_HEADER__
# define __MAME_JPEG_HEADER__

# ifdef __cplusplus
extern "C" {
# endif /* __cplusplus */

#include <ctype.h>

#define CHECK_PARAM( X ) do{if( (X) == false ) return false; } while(0);
#define NULL_CHECK_PARAM( X )  CHECK_PARAM( (X) != NULL )
#define CHECK_PROC( LABEL, X ) do{ if( (X) == false ) goto LABEL; } while(0);


typedef struct {
    uint8_t* buffer;
    uint16_t cache;
    int cache_use_bits;
    size_t pos;
    size_t buffer_length;
} mameBitstream_context;

bool mameBitstream_initialize( mameBitstream_context* context, uint8_t* buffer, size_t buffer_length )
{
    NULL_CHECK_PARAM( buffer )

    context->buffer = buffer;
    context->buffer_length = buffer_length
    context->cache = 0x0000;
    context->cache_use_bits = 0;
    context->pos = 0;

    return true;
}

bool mameBitstream_readByte( mameBitstream_context* context )
{
    NULL_CHECK_PARAM( context )
    CHECK_PARAM( ( context->pos + 1 ) < context->buffer_length )
    CHECK_PARAM( context->cache_use_bits < 8 )

    context->cache = ( context->cache << 8 ) | ( context->buffer[ context->pos ] );
    context->cache_use_bits += 8;
    context->pos++;

    return true;
}

bool mameBitstream_readBits( mameBitstream_context* context, void* buffer, int buffer_length, int bits )
{
    NULL_CHECK_PARAM( context )
    NULL_CHECK_PARAM( buffer )
    CHECK_PARAM( bits <= ( 8 * buffer_length ) )

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


typedef enum {
    MAMEJPEG_ENCODE = 0,
    MAMEJPEG_DECODE = 1,
} mameJpeg_mode;

typedef enum{
    MAMEJPEG_SOI = 0xffd8,
    MAMEJPEG_APP0 = 0xffe0,
    MAMEJPEG_DQT = 0xffdb,
    MAMEJPEG_SOF0 = 0xffc0,
    MAMEJPEG_DHT = 0xffc4,
    MAMEJPEG_SOS = 0xffda,
    MAMEJPEG_EOI = 0xffd9,
} mameJpeg_marker;


typedef struct {
    uint8_t* work_buffer;
    size_t work_buffer_length;
    uint8_t* io_ptr;
    size_t io_size;
    mameBitstream_context io_stream[1];
    mameJpeg_mode mode;
    int progress;
} mameJpeg_context;

bool mameJpeg_getImageSize( void* input, int* width, int* height )
{
    width = height = 100;
    return true;
}

bool mameJpeg_getBufferSize( int width, int height )
{
    return ( width * height * 3 ) >> 1;
}

bool mameJpeg_getProgress( mameJpeg_context* context, int* progress )
{
    *progress = context->progress;

    return true;
}

bool mameJpeg_initialize( mameJpeg_context* context,
                          void* work_buffer,
                          size_t work_buffer_length,
                          void* input_ptr,
                          size_t input_size,
                          void* output_ptr,
                          size_t output_size,
                          mameJpeg_mode mode )
{
    NULL_CHECK_PARAM( context )
    NULL_CHECK_PARAM( work_buffer )
    CHECK_PARAM( 0 < work_buffer_length )
    NULL_CHECK_PARAM( input_ptr )
    CHECK_PARAM( input_size )
    NULL_CHECK_PARAM( output_ptr )
    CHECK_PARAM( output_size )
    CHECK_PARAM( mode == MAMEJPEG_ENCODE || mode == MAMEJPEG_DECODE )

    context->work_buffer = (uint8_t*)work_buffer;
    context->work_buffer_length = work_buffer_length;

    context->mode = mode;
    context->io_ptr =  ( mode == MAMEJPEG_ENCODE ) ? input_ptr : output_ptr;
    context->io_size = ( mode == MAMEJPEG_ENCODE ) ? input_size : output_size;
    mameBitstream_initialize( context->io_stream,
                              ( mode == MAMEJPEG_ENCODE ) ? output_ptr : input_ptr,
                              ( mode == MAMEJPEG_ENCODE ) ? output_ptr : input_ptr );

    return true;
}

bool mameJpeg_Decode( mameJpeg_context* context )
{
    NULL_CHECK_PARAM( context )
    CHECK_PARAM( context->mode == MAMEJPEG_DECODE )

    mameJpeg_marker marker;
    while( mameJpeg_getNextMarker( context, &marker ) )
    {
        switch( marker )
        {
            case MAMEJPEG_SOI:
            {
            }break;
            case MAMEJPEG_APP0:
            {
            }break;
            case MAMEJPEG_DQT:
            {
            }break;
            case MAMEJPEG_SOF0:
            {
            }break;
            case MAMEJPEG_DHT:
            {
            }break;
            case MAMEJPEG_SOS:
            {
            }break;
            case MAMEJPEG_EOI:
            {
            }break;
            default:
            {
            }break;
        }
    }

    return true;
}

bool mameJpeg_getNextMarker( mameJpeg_context* context, mameJpeg_marker* marker )
{
    uint8_t prefix;
    while( mameBitstream_readBits( context->io_stream, &prefix, 8 ) )
    {
        if( prefix == 0xff )
        {
            uint8_t val;
            CHECK_PROC( END_LOOP, mameBitstream_readBits( context->io_stream, &val, 8 ) );

            *marker = ( prefix << 8 ) | val;
            return true;
        }
    }
END_LOOP:

    return false;
}

bool mameJpeg_Encode( mameJpeg_context* context )
{
    NULL_CHECK_PARAM( context )
    CHECK_PARAM( context->mode == MAMEJPEG_ENCODE )

    return true;
}

bool mameJpeg_dispose( mameJpeg_context* context )
{
    return true;
}

# ifdef __cplusplus
}
# endif /* __cplusplus */

#endif /* __MAME_JPEG_HEADER__ */
