#ifndef __MAME_JPEG_HEADER__
# define __MAME_JPEG_HEADER__

# ifdef __cplusplus
extern "C" {
# endif /* __cplusplus */

#include <ctype.h>

#define MAMEJPEG_CHECK( X ) do{if((X)==false )return false;}while(0);
#define MAMEJPEG_NULL_CHECK( X )  MAMEJPEG_CHECK((X)!=NULL)
#define MAMEJPEG_PROC_CHECK( LABEL, X ) do{ if( (X) == false ) goto LABEL; } while(0);

typedef bool (*mameJpeg_input_byte_callback_ptr)( void* param, uint8_t* byte );
typedef bool (*mameJpeg_output_byte_callback_ptr)( void* param, uint8_t byte );

typedef struct {
    void* buffer_ptr;
    size_t buffer_pos;
    size_t buffer_length;
} mameJpeg_memory_callback_param;

bool mameJpeg_input_from_memory_callback( void* param, uint8_t* byte )
{
    MAMEJPEG_NULL_CHECK( param )
    MAMEJPEG_NULL_CHECK( byte )

    mameJpeg_memory_callback_param *callback_param = (mameJpeg_memory_callback_param*)param;
    MAMEJPEG_CHECK( callback_param->buffer_pos < callback_param->buffer_length )

    *byte = ((uint8_t*)callback_param->buffer_ptr)[ callback_param->buffer_pos ];
    callback_param->buffer_pos++;

    return true;
}

bool mameJpeg_output_to_memory_callback( void* param, uint8_t byte )
{
    MAMEJPEG_NULL_CHECK( param )

    mameJpeg_memory_callback_param *callback_param = (mameJpeg_memory_callback_param*)param;
    MAMEJPEG_CHECK( callback_param->buffer_pos < callback_param->buffer_length )

    ((uint8_t*)callback_param->buffer_ptr)[ callback_param->buffer_pos ] = byte;
    (( mameJpeg_memory_callback_param* )param)->buffer_pos++;

    return true;
}

typedef enum
{
    MAMEBITSTREAM_INPUT,
    MAMEBITSTREAM_OUTPUT,
} mameBitstream_mode;

typedef struct {
    union {
        mameJpeg_input_byte_callback_ptr input;
        mameJpeg_output_byte_callback_ptr output;
    } callback;
    void* callback_param;
    uint16_t cache;
    int cache_use_bits;
    mameBitstream_mode mode;
} mameBitstream_context;

bool mameBitstream_input_initialize( mameBitstream_context* context, mameJpeg_input_byte_callback_ptr callback, void* callback_param )
{
    MAMEJPEG_NULL_CHECK( context );
    MAMEJPEG_NULL_CHECK( callback );

    context->callback.input = callback;
    context->callback_param = callback_param;
    context->cache = 0x0000;
    context->cache_use_bits = 0;
    context->mode = MAMEBITSTREAM_INPUT;

    return true;
}

bool mameBitstream_output_initialize( mameBitstream_context* context, mameJpeg_output_byte_callback_ptr callback, void* callback_param )
{
    MAMEJPEG_NULL_CHECK( context );
    MAMEJPEG_NULL_CHECK( callback );

    context->callback.output = callback;
    context->callback_param = callback_param;
    context->cache = 0x0000;
    context->cache_use_bits = 0;
    context->mode = MAMEBITSTREAM_OUTPUT;

    return true;
}

bool mameBitstream_tryReadIntoCache( mameBitstream_context* context )
{
    MAMEJPEG_NULL_CHECK( context )
    MAMEJPEG_CHECK( context->mode == MAMEBITSTREAM_INPUT )
    MAMEJPEG_CHECK( context->cache_use_bits < 8 )
    //( ( 8 * sizeof( context->cache ) ) - context->cache_use_bits < 8

    uint8_t tmp;
    MAMEJPEG_CHECK( context->callback.input( context->callback_param, &tmp ) );
    context->cache |= ( tmp << context->cache_use_bits );
    context->cache_use_bits += 8;

    return true;
}

bool mameBitstream_tryWriteFromCache( mameBitstream_context* context )
{
    MAMEJPEG_NULL_CHECK( context )
    MAMEJPEG_CHECK( context->mode == MAMEBITSTREAM_OUTPUT )
    MAMEJPEG_CHECK( 8 <= context->cache_use_bits )

    uint8_t tmp = (uint8_t)( context->cache & 0xff );
    MAMEJPEG_CHECK( context->callback.output( context->callback_param, tmp ) );

    context->cache >>= 8;
    context->cache_use_bits -= 8;

    return true;
}

bool mameBitstream_readBits( mameBitstream_context* context, void* buffer, int buffer_length, int bits )
{
    MAMEJPEG_NULL_CHECK( context )
    MAMEJPEG_CHECK( context->mode == MAMEBITSTREAM_INPUT )
    MAMEJPEG_NULL_CHECK( buffer )
    MAMEJPEG_CHECK( bits <= ( 8 * buffer_length ) )

    int buffer_pos = 0;
    while( 0 < bits )
    {
        int read_bits = ( 8 < bits ) ? 8 : bits;
        if( context->cache_use_bits < read_bits )
        {
            MAMEJPEG_PROC_CHECK( ON_ERROR,  mameBitstream_tryReadIntoCache( context ) );
        }

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
    MAMEJPEG_CHECK( context->mode == MAMEBITSTREAM_OUTPUT )
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
            MAMEJPEG_PROC_CHECK( ON_ERROR, mameBitstream_tryWriteFromCache( context ) );
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
    mameBitstream_context input_stream[1];
    mameBitstream_context output_stream[1];
    mameJpeg_mode mode;
    int progress;
} mameJpeg_context;

bool mameJpeg_getImageSize( void* input, int* width, int* height )
{
    *width = *height = 100;
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
                          mameJpeg_input_byte_callback_ptr input_callback,
                          void* input_callback_param,
                          mameJpeg_output_byte_callback_ptr output_callback,
                          void* output_callback_param,
                          void* work_buffer,
                          size_t work_buffer_length,
                          mameJpeg_mode mode )
{
    MAMEJPEG_NULL_CHECK( context )
    MAMEJPEG_NULL_CHECK( input_callback )
    MAMEJPEG_NULL_CHECK( output_callback )
    MAMEJPEG_NULL_CHECK( work_buffer )
    MAMEJPEG_CHECK( 0 < work_buffer_length )
    MAMEJPEG_CHECK( mode == MAMEJPEG_ENCODE || mode == MAMEJPEG_DECODE )

    mameBitstream_input_initialize( context->input_stream, input_callback, input_callback_param );
    mameBitstream_output_initialize( context->output_stream, output_callback, output_callback_param );
    context->work_buffer = (uint8_t*)work_buffer;
    context->work_buffer_length = work_buffer_length;
    context->mode = mode;
    return true;
}

#if 0
bool mameJpeg_getNextMarker( mameJpeg_context* context, mameJpeg_marker* marker )
{
    uint8_t prefix;
    while( mameBitstream_readBits( context->io_stream, &prefix, 8 ) )
    {
        if( prefix == 0xff )
        {
            uint8_t val;
            MAMEJPEG_PROC_CHECK( END_LOOP, mameBitstream_readBits( context->io_stream, &val, 8 ) );

            *marker = (mameJpeg_marker)(( prefix << 8 ) | val);
            return true;
        }
    }
END_LOOP:

    return false;
}


bool mameJpeg_Decode( mameJpeg_context* context )
{
    NULL_CHECK( context )
    CHECK( context->mode == MAMEJPEG_DECODE )

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


bool mameJpeg_Encode( mameJpeg_context* context )
{
    MAMEJPEG_NULL_CHECK( context )
    MAMEJPEG_CHECK( context->mode == MAMEJPEG_ENCODE )

    return true;
}

bool mameJpeg_dispose( mameJpeg_context* context )
{
    return true;
}
#endif
# ifdef __cplusplus
}
# endif /* __cplusplus */

#endif /* __MAME_JPEG_HEADER__ */
