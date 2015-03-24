#ifndef __MAME_JPEG_HEADER__
# define __MAME_JPEG_HEADER__

# ifdef __cplusplus
extern "C" {
# endif /* __cplusplus */

#include <ctype.h>

#define MAMEJPEG_CHECK_PARAM( X ) do{if((X)==false )return false;}while(0);
#define MAMEJPEG_NULL_CHECK_PARAM( X )  MAMEJPEG_CHECK((X)!=NULL)
#define MAMEJPEG_CHECK_PROC( LABEL, X ) do{ if( (X) == false ) goto LABEL; } while(0);

typedef bool (*)( void* param, uint8_t* byte ) mameJpeg_input_byte_callback_ptr;
typedef bool (*)( void* param, uint8_t byte ) mameJpeg_output_byte_callback_ptr;

typedef struct {
    mameJpeg_input_byte_callback_ptr callback;
    void* callback_param;
    uint16_t cache;
    int cache_use_bits;
} mameBitstream_input_context;

typedef struct {
    mameJpeg_output_byte_callback_ptr callback;
    void* callback_param;
    uint16_t cache;
    int cache_use_bits;
} mameBitstream_output_context;


bool mameBitstream_input_initialize( mameBitstream_input_context* context, mameJpeg_read_byte_callback_ptr callback, void* callback_param )
{
    MAMEJPEG_NULL_CHECK( context );
    MAMEJPEG_NULL_CHECK( callback );

    context->callback = callback;
    context-callback_param = callback_param;
    context->cache = 0x0000;
    context->cache_use_bits = 0;

    return true;
}

bool mameBitstream_output_initialize( mameBitstream_input_context* context, mameJpeg_write_byte_callback_ptr callback, void* callback_param )
{
    MAMEJPEG_NULL_CHECK( context );
    MAMEJPEG_NULL_CHECK( callback );

    context->callback = callback;
    context-callback_param = callback_param;
    context->cache = 0x0000;
    context->cache_use_bits = 0;

    return true;
}

bool mameBitstream_tryReadIntoCache( mameBitstream_input_context* context )
{
    MAMEJPEG_NULL_CHECK_PARAM( context )
    MAMEJPEG_CHECK_PARAM( context->cache_use_bits < 8 )

    uint8_t tmp;
    MAMEJPEG_CHECK_PARAM( context->callback( context->callback_param, &tmp ) );

    context->cache |= ( tmp << context->cache_use_bits );
    context->cache_use_bits += 8;
    context->pos++;

    return true;
}

bool mameBitstream_writeByte( mameBitstream_output_context* context )
{
    MAMEJPEG_NULL_CHECK( context )
    MAMEJPEG_CHECK( 8 <= context->cache_use_bits )

    uint8_t tmp = (uint8_t)( context->cache & 0xff );
    MAMEJPEG_CHECK_PROC( context->callback( context->callback_param, tmp ) );

    context->cache >>= 8;
    context->cache_use_bits -= 8;
    context->pos++;

    return true;
}

bool mameBitstream_readBits( mameBitstream_input_context* context, void* buffer, int buffer_length, int bits )
{
    MAMEJPEG_NULL_CHECK_PARAM( context )
    MAMEJPEG_NULL_CHECK_PARAM( buffer )
    MAMEJPEG_CHECK_PARAM( bits <= ( 8 * buffer_length ) )

    int buffer_pos = 0;
    while( 0 < bits )
    {
        if( context->cache_use_bits < bit && context->cache_use_bits < 8 )
        {
            MAMEJPEG_CHECK_PROC( ON_ERROR,  mameBitstream_tryReadIntoCache( context ) );
        }

        int read_bits = ( 8 < bits ) ? 8 : bits;
        uint8_t cache_mask = ( 1 << read_bits ) - 1;
        ((uint8_t*)buffer)[ buffer_pos ] = context->cache & cache_mask;
        buffer_pos++;
        bits -= read_bits;

        context->cache >>= read_bits;
        context->cache_use_bits -= read_bits;
    }

ON_ERROR:
    return ( bits == 0 );
}

bool mameBitstream_writeBits( mameBitstream_context* context, void* buffer, int bits )
{
    MAMEJPEG_NULL_CHECK( context )
    MAMEJPEG_NULL_CHECK( buffer )

    int buffer_pos = 0;
    while( 0 < bits )
    {
        int write_bits = ( 8 < bits ) ? 8 : bits;
        uint8_t cache_mask = ( 1 << write_bits ) - 1;
        context->cache |= ( (((uint8_t*)buffer)[ buffer_pos ] ) & cache_mask ) << context->cache_use_bits;
        context->cache_use_bits += write_bits;

        buffer_pos++;
        bits -= write_bits;

        if( 8 <= context->cache_use_bits )
        {
            MAMEJPEG_CHECK_PROC( ON_ERROR, mameBitstream_tryRriteFromCache( context ) );
        }
    }

ON_ERROR:
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
