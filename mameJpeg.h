#ifndef __MAME_JPEG_HEADER__
# define __MAME_JPEG_HEADER__

# ifdef __cplusplus
extern "C" {
# endif /* __cplusplus */

#include <ctype.h>

#define MAMEJPEG_CHECK( X ) do{if((X)==false )return false;}while(0)
#define MAMEJPEG_NULL_CHECK( X )  MAMEJPEG_CHECK((X)!=NULL)
#define MAMEJPEG_PROC_CHECK( LABEL, X ) do{ if( (X) == false ) goto LABEL; } while(0)

typedef bool (*mameJpeg_read_callback_ptr)( void* param, uint8_t* byte );
typedef bool (*mameJpeg_write_callback_ptr)( void* param, uint8_t byte );
typedef bool (*mameJpeg_rewind_callback_ptr)( void* param );

typedef struct {
    void* buffer_ptr;
    size_t buffer_pos;
    size_t buffer_length;
} mameJpeg_memory_callback_param;

bool mameJpeg_input_from_memory_callback( void* param, uint8_t* byte )
{
    MAMEJPEG_NULL_CHECK( param );
    MAMEJPEG_NULL_CHECK( byte );

    mameJpeg_memory_callback_param *callback_param = (mameJpeg_memory_callback_param*)param;
    MAMEJPEG_NULL_CHECK( callback_param->buffer_ptr );
    MAMEJPEG_CHECK( callback_param->buffer_pos < callback_param->buffer_length );

    *byte = ((uint8_t*)callback_param->buffer_ptr)[ callback_param->buffer_pos ];
    callback_param->buffer_pos++;

    return true;
}

bool mameJpeg_output_to_memory_callback( void* param, uint8_t byte )
{
    MAMEJPEG_NULL_CHECK( param );

    mameJpeg_memory_callback_param *callback_param = (mameJpeg_memory_callback_param*)param;
    MAMEJPEG_NULL_CHECK( callback_param->buffer_ptr );
    MAMEJPEG_CHECK( callback_param->buffer_pos < callback_param->buffer_length );

    ((uint8_t*)callback_param->buffer_ptr)[ callback_param->buffer_pos ] = byte;
    callback_param->buffer_pos++;

    return true;
}

bool mameJpeg_rewind_memory_callback( void* param )
{
    MAMEJPEG_NULL_CHECK( param );

    mameJpeg_memory_callback_param *callback_param = (mameJpeg_memory_callback_param*)param;
    callback_param->buffer_pos = 0;

    return true;
}

bool mameJpeg_input_from_file_callback( void* param, uint8_t* byte )
{
    MAMEJPEG_NULL_CHECK( param );
    MAMEJPEG_NULL_CHECK( byte );

    FILE* fp = (FILE*)param;
    uint8_t tmp;
    size_t n = fread( &tmp, 1, 1, fp );

    return ( n == 1 );
}

bool mameJpeg_output_to_file_callback( void* param, uint8_t byte )
{
    MAMEJPEG_NULL_CHECK( param );

    FILE* fp = (FILE*)param;
    size_t n = fread( &byte, 1, 1, fp );

    return ( n == 1 );
}

bool mameJpeg_rewind_file_callback( void* param )
{
    MAMEJPEG_NULL_CHECK( param );

    FILE* fp = (FILE*)param;
    rewind( fp );

    return true;
}

typedef enum
{
    MAMEBITSTREAM_READ,
    MAMEBITSTREAM_WRITE,
} mameBitstream_mode;

typedef struct {
    union {
        mameJpeg_read_callback_ptr read;
        mameJpeg_write_callback_ptr write;
    } io_callback;
    mameJpeg_rewind_callback_ptr rewind_callback;
    void* callback_param;
    uint16_t cache;
    int cache_use_bits;
    mameBitstream_mode mode;
} mameBitstream_context;

bool mameBitstream_input_initialize( mameBitstream_context* context,
                                     mameJpeg_read_callback_ptr read_callback,
                                     mameJpeg_rewind_callback_ptr rewind_callback,
                                     void* callback_param )
{
    MAMEJPEG_NULL_CHECK( context );
    MAMEJPEG_NULL_CHECK( read_callback );
    MAMEJPEG_NULL_CHECK( rewind_callback );

    context->io_callback.read = read_callback;
    context->rewind_callback = rewind_callback;
    context->callback_param = callback_param;
    context->cache = 0x0000;
    context->cache_use_bits = 0;
    context->mode = MAMEBITSTREAM_READ;

    return true;
}

bool mameBitstream_output_initialize( mameBitstream_context* context,
                                      mameJpeg_write_callback_ptr write_callback,
                                      mameJpeg_rewind_callback_ptr rewind_callback,
                                      void* callback_param )
{
    MAMEJPEG_NULL_CHECK( context );
    MAMEJPEG_NULL_CHECK( write_callback );
    MAMEJPEG_NULL_CHECK( rewind_callback );

    context->io_callback.write = write_callback;
    context->rewind_callback = rewind_callback;
    context->callback_param = callback_param;
    context->cache = 0x0000;
    context->cache_use_bits = 0;
    context->mode = MAMEBITSTREAM_WRITE;

    return true;
}

bool mameBitstream_tryReadIntoCache( mameBitstream_context* context )
{
    MAMEJPEG_NULL_CHECK( context );
    MAMEJPEG_CHECK( context->mode == MAMEBITSTREAM_READ );
    MAMEJPEG_CHECK( context->cache_use_bits < 8 );
    //( ( 8 * sizeof( context->cache ) ) - context->cache_use_bits < 8

    uint8_t tmp;
    MAMEJPEG_CHECK( context->io_callback.read( context->callback_param, &tmp ) );
    context->cache |= ( tmp << ( 16 - 8 - context->cache_use_bits ) );
    context->cache_use_bits += 8;

    return true;
}

bool mameBitstream_tryWriteFromCache( mameBitstream_context* context )
{
    MAMEJPEG_NULL_CHECK( context );
    MAMEJPEG_CHECK( context->mode == MAMEBITSTREAM_WRITE );
    MAMEJPEG_CHECK( 8 <= context->cache_use_bits );

    uint8_t tmp = (uint8_t)( context->cache >> 8 );
    MAMEJPEG_CHECK( context->io_callback.write( context->callback_param, tmp ) );

    context->cache <<= 8;
    context->cache_use_bits -= 8;

    return true;
}

bool mameBitstream_readBits( mameBitstream_context* context,
                             void* buffer,
                             int buffer_length,
                             int bits )
{
    MAMEJPEG_NULL_CHECK( context );
    MAMEJPEG_CHECK( context->mode == MAMEBITSTREAM_READ );
    MAMEJPEG_NULL_CHECK( buffer );
    MAMEJPEG_CHECK( bits <= ( 8 * buffer_length ) );

    int buffer_pos = ( bits - 1 ) >> 3;
    while( 0 < bits )
    {
        uint8_t remain_bits = bits & 0x07;
        uint8_t read_bits = ( remain_bits != 0 ) ? remain_bits : 8;
        if( context->cache_use_bits < read_bits )
        {
            MAMEJPEG_CHECK( mameBitstream_tryReadIntoCache( context ) );
        }

        uint8_t shift_width = 16 - read_bits;
        ((uint8_t*)buffer)[ buffer_pos ] = ( context->cache >> shift_width );
        buffer_pos--;
        bits -= read_bits;

        context->cache <<= read_bits;
        context->cache_use_bits -= read_bits;
    }

    return ( bits == 0 );
}

bool mameBitstream_readByte( mameBitstream_context* context, uint8_t* byte )
{
    return mameBitstream_readBits( context, byte, 1, 8 );
}

bool mameBitstream_readTwoByte( mameBitstream_context* context, uint16_t* byte )
{
    uint8_t high;
    MAMEJPEG_CHECK( mameBitstream_readByte( context, &high ) );
    uint8_t low;
    MAMEJPEG_CHECK( mameBitstream_readByte( context, &low ) );
    *byte = ( high << 8 ) | low;

    return true ;
}

bool mameBitstream_writeBits( mameBitstream_context* context, void* buffer, int bits )
{
    MAMEJPEG_NULL_CHECK( context );
    MAMEJPEG_CHECK( context->mode == MAMEBITSTREAM_WRITE );
    MAMEJPEG_NULL_CHECK( buffer );

    int buffer_pos = ( bits - 1 ) >> 3;
    while( 0 < bits )
    {
        uint8_t remain_bits = bits & 0x07;
        uint8_t write_bits = ( remain_bits != 0 ) ? remain_bits : 8;
        uint8_t cache_mask = ( 1 << write_bits ) - 1;
        uint8_t shift_width = 16 - context->cache_use_bits - write_bits;
        context->cache |= ( (((uint8_t*)buffer)[ buffer_pos ] ) & cache_mask ) << shift_width;
        context->cache_use_bits += write_bits;

        buffer_pos--;
        bits -= write_bits;

        if( 8 <= context->cache_use_bits )
        {
            MAMEJPEG_CHECK( mameBitstream_tryWriteFromCache( context ) );
        }
    }

    return ( bits == 0 );
}

bool mameBitstream_rewind( mameBitstream_context* context )
{
    MAMEJPEG_NULL_CHECK( context );

    return context->rewind_callback( context->callback_param );
}

typedef enum {
    MAMEJPEG_ENCODE = 0,
    MAMEJPEG_DECODE = 1,
} mameJpeg_mode;

typedef enum {
    MAMEJPEG_FORMAT_Y,
    MAMEJPEG_FORMAT_YUV,
    MAMEJPEG_FORMAT_RGB,
    MAMEJPEG_FORMAT_UNKNOWN,
} mameJpeg_format;

typedef enum{
    MAMEJPEG_MARKER_SOI = 0xffd8,
    MAMEJPEG_MARKER_DQT = 0xffdb,
    MAMEJPEG_MARKER_SOF0 = 0xffc0,
    MAMEJPEG_MARKER_DHT = 0xffc4,
    MAMEJPEG_MARKER_SOS = 0xffda,
    MAMEJPEG_MARKER_EOI = 0xffd9,
    MAMEJPEG_MARKER_UNKNOWN = 0x0000,
} mameJpeg_marker;

typedef struct {
    uint8_t* work_buffer;
    size_t work_buffer_length;
    mameBitstream_context input_stream[1];
    mameBitstream_context output_stream[1];
    mameJpeg_format format;
    mameJpeg_mode mode;
    int progress;

    struct {
        uint8_t accuracy;
        uint16_t width;
        uint16_t height;
        uint8_t component_num;
        struct {
            int hor_sampling;
            int ver_sampling;
            int quant_table_index;
            int huff_table_dc_index;
            int huff_table_ac_index;
        } component [3];
        uint8_t quant_table[4][64];
        struct {
            uint16_t offsets[16];
            struct {
                uint16_t code;
                uint8_t value;
            } elements[ 512];
        } huff_table[2][2];
    } info;
} mameJpeg_context;

bool mameJpeg_initialize( mameJpeg_context* context,
                          mameJpeg_read_callback_ptr input_callback,
                          mameJpeg_rewind_callback_ptr input_rewind_callback,
                          void* input_callback_param,
                          mameJpeg_write_callback_ptr output_callback,
                          mameJpeg_rewind_callback_ptr output_rewind_callback,
                          void* output_callback_param,
                          mameJpeg_format format,
                          mameJpeg_mode mode )
{
    MAMEJPEG_NULL_CHECK( context );
    MAMEJPEG_NULL_CHECK( input_callback );
    MAMEJPEG_NULL_CHECK( input_rewind_callback );
    MAMEJPEG_NULL_CHECK( output_callback );
    MAMEJPEG_NULL_CHECK( output_rewind_callback );
    MAMEJPEG_CHECK( mode == MAMEJPEG_ENCODE || mode == MAMEJPEG_DECODE );

    mameBitstream_input_initialize( context->input_stream, input_callback, input_rewind_callback, input_callback_param );
    mameBitstream_output_initialize( context->output_stream, output_callback, output_rewind_callback, output_callback_param );
    context->format = format;
    context->mode = mode;
    return true;
}


bool mameJpeg_decodeSOF0Segment( mameJpeg_context* context );
bool mameJpeg_decodeDHTSegment( mameJpeg_context* context );
bool mameJpeg_getNextMarker( mameJpeg_context* context, mameJpeg_marker* marker );

bool mameJpeg_getBytePerPixel( mameJpeg_format format, int* byte_per_pixel )
{
    switch( format )
    {
        case MAMEJPEG_FORMAT_Y:
        {
            *byte_per_pixel = 1;
        }break;
        case MAMEJPEG_FORMAT_YUV: /* fall through */
        case MAMEJPEG_FORMAT_RGB:
        {
            *byte_per_pixel = 3;
        }break;
        default:
            return false;
    }

    return false;
}

bool mameJpeg_getBufferSize( mameJpeg_context* context, size_t* buffer_size )
{
    MAMEJPEG_NULL_CHECK( context );
    MAMEJPEG_NULL_CHECK( buffer_size );
    MAMEJPEG_CHECK( context->mode == MAMEJPEG_DECODE );

    MAMEJPEG_CHECK( mameBitstream_rewind( context->input_stream ) );
    MAMEJPEG_CHECK( mameBitstream_rewind( context->output_stream ) );

    mameJpeg_context work_context = *context;

    mameJpeg_marker marker;
    while( mameJpeg_getNextMarker( &work_context, &marker ) )
    {
        if( marker == MAMEJPEG_MARKER_SOF0 )
        {
            MAMEJPEG_CHECK( mameJpeg_decodeSOF0Segment( &work_context ) );
        }
        else if( marker == MAMEJPEG_MARKER_DHT )
        {
            MAMEJPEG_CHECK( mameJpeg_decodeDHTSegment( &work_context ) );
        }
        else if( marker == MAMEJPEG_MARKER_SOS )
        {
            break;
        }
    }

    int width = work_context.info.width;
    int height = work_context.info.height;
    int byte_per_pixel;
    MAMEJPEG_CHECK( mameJpeg_getBytePerPixel( work_context.format, &byte_per_pixel ) );

    *buffer_size = byte_per_pixel * width * height;
    *buffer_size += work_context.info.component_num * 64;
    //buffer_size += work_context.info.

    MAMEJPEG_CHECK( mameBitstream_rewind( context->input_stream ) );
    MAMEJPEG_CHECK( mameBitstream_rewind( context->output_stream ) );
    return true;
}

bool mameJpeg_getProgress( mameJpeg_context* context, int* progress )
{
    MAMEJPEG_NULL_CHECK( context );
    MAMEJPEG_NULL_CHECK( progress );

    *progress = context->progress;

    return true;
}
bool mameJpeg_setWorkBuffer( mameJpeg_context* context, void* work_buffer, size_t work_buffer_length )
{
    MAMEJPEG_NULL_CHECK( work_buffer );
    MAMEJPEG_CHECK( 0 < work_buffer_length );

    context->work_buffer = (uint8_t*)work_buffer;
    context->work_buffer_length = work_buffer_length;

    return true;
}

bool mameJpeg_getNextMarker( mameJpeg_context* context, mameJpeg_marker* marker )
{
    uint8_t prefix;
    while( mameBitstream_readBits( context->input_stream, &prefix, 1, 8 ) )
    {
        if( prefix == 0xff )
        {
            uint8_t val;
            MAMEJPEG_PROC_CHECK( END_LOOP, mameBitstream_readBits( context->input_stream, &val, 1, 8 ) );

            *marker = (mameJpeg_marker)(( prefix << 8 ) | val);
            return true;
        }
    }
END_LOOP:

    return false;
}

typedef bool (*mameJpeg_decodeSegmentFuncPtr)(mameJpeg_context*);

bool mameJpeg_getSegmentSize( mameJpeg_context* context, uint16_t* size )
{
    uint16_t whole_size;
    MAMEJPEG_CHECK( mameBitstream_readTwoByte( context->input_stream, &whole_size) );
    *size = whole_size - 2;

    return true;
}

bool mameJpeg_decodeSOISegment( mameJpeg_context* context )
{
    printf("call %s\n", __func__ );
    return true;
}

bool mameJpeg_decodeDQTSegment( mameJpeg_context* context )
{
    printf("call %s\n", __func__ );

    uint16_t dqt_size;
    MAMEJPEG_CHECK( mameJpeg_getSegmentSize( context, &dqt_size ) );
    MAMEJPEG_CHECK( dqt_size == ( 64 + 1 ) );

    uint8_t info;
    MAMEJPEG_CHECK( mameBitstream_readByte( context->input_stream, &info ) );
    MAMEJPEG_CHECK( ( info >> 4 ) == 0 );

    uint8_t table_no = info & 0x0f;
    for( int i = 0; i < 64; i++ )
    {
        uint8_t* quant_table_ptr = &( context->info.quant_table[table_no][i] );
        MAMEJPEG_CHECK( mameBitstream_readByte( context->input_stream, quant_table_ptr ) );
    }

    return true;
}

bool mameJpeg_decodeSOF0Segment( mameJpeg_context* context )
{
    printf("call %s\n", __func__ );
    
    uint16_t sof0_size;
    MAMEJPEG_CHECK( mameJpeg_getSegmentSize( context, &sof0_size ) );
    MAMEJPEG_CHECK( sof0_size == 15 );

    MAMEJPEG_CHECK( mameBitstream_readByte( context->input_stream, &(context->info.accuracy) ) );
    MAMEJPEG_CHECK( mameBitstream_readTwoByte( context->input_stream, &(context->info.height) ) );
    MAMEJPEG_CHECK( mameBitstream_readTwoByte( context->input_stream, &(context->info.width) ) );
    MAMEJPEG_CHECK( mameBitstream_readByte( context->input_stream, &(context->info.component_num) ) );
    MAMEJPEG_CHECK( context->info.component_num < 4 );

    for( int i = 0; i < context->info.component_num; i++ )
    {
        uint8_t component_index;
        MAMEJPEG_CHECK( mameBitstream_readByte( context->input_stream, &component_index ) );
        component_index--; // component_index is zero origin
        MAMEJPEG_CHECK( component_index < 3 );

        uint8_t sampling;
        MAMEJPEG_CHECK( mameBitstream_readByte( context->input_stream, &sampling ) );
        context->info.component[ component_index ].hor_sampling = sampling >> 4;
        context->info.component[ component_index ].ver_sampling = sampling & 0xf;

        uint8_t quant_table_index;
        MAMEJPEG_CHECK( mameBitstream_readByte( context->input_stream, &quant_table_index ) );
        context->info.component[ component_index ].quant_table_index = quant_table_index;
    }

    return true;
}

bool mameJpeg_decodeDHTSegment( mameJpeg_context* context )
{
    printf("call %s\n", __func__ );

    uint16_t dht_size;
    MAMEJPEG_CHECK( mameJpeg_getSegmentSize( context, &dht_size ) );
    MAMEJPEG_CHECK( 17 <= dht_size );

    uint8_t info;
    MAMEJPEG_CHECK( mameBitstream_readByte( context->input_stream, &info ) );
    uint8_t ac_dc = info >> 4;
    uint8_t luma_chroma = info & 0x0f;

    uint16_t code = 0;
    uint16_t total_codes = 0;
    for( int i = 0; i < 16; i++ )
    {
        uint8_t num_of_codes;
        MAMEJPEG_CHECK( mameBitstream_readByte( context->input_stream, &num_of_codes ) );
        context->info.huff_table[ac_dc][luma_chroma].offsets[i] = total_codes;
        for( int j = 0; j < num_of_codes; j++ )
        {
            context->info.huff_table[ac_dc][luma_chroma].elements[ total_codes + j ].code = code;
            code++;
        }

        code <<= 1;
        total_codes += num_of_codes;
    }
    MAMEJPEG_CHECK( 17 + total_codes == dht_size );
    
    for( int i = 0; i < total_codes; i++ )
    {
        uint8_t zero_length;
        MAMEJPEG_CHECK( mameBitstream_readByte( context->input_stream, &zero_length ) );
        context->info.huff_table[ac_dc][luma_chroma].elements[ i ].value = zero_length;
    }

    return true;
}

bool mameJpeg_decodeHuffmanCode( mameJpeg_context* context, uint16_t code, uint8_t code_length, uint8_t* value )
{

            uint8_t tmp;
            MAMEJPEG_CHECK( mameBitstream_readBits( context->input_stream, &tmp, 1, 1 ) );
            code = ( code << 1 ) | tmp;

            uint8_t from_index = ( code_length == 0 ) ? 0 : context->info.huff_table[ac_dc][luma_chroma].offsets[ code_length - 1 ];
            uint8_t to_index = context->info.huff_table[ac_dc][luma_chroma].offsets[ code_length ];
            uint8_t length = 0;
            for( uint8_t i = from_index; i < to_index; i++ )
            {
                if( context->info.huff_table[ac_dc][luma_chroma].elements[i].code == code )
                {
                    length = context->info.huff_table[ac_dc][luma_chroma].elements[i].value;

                    uint16_t value;
                    MAMEJPEG_CHECK( mameBitstream_readBits( context->input_stream, &value, 2, length ) );
                    printf("%d - %x\n", length,value);
                    break;
                }
            }
}

bool mameJpeg_decodeMCUDC( mameJpeg_context* context )
{
    return true;
}
bool mameJpeg_decodeMCUAC( mameJpeg_context* context )
{
    return true;
}

bool mameJpeg_decodeMCU( mameJpeg_context* context )
{
#if 1
    for( int i = 0; i < context->info.component_num; i++ )
    {
        printf("ac_dc %d, luma_chroma %d\n", ac_dc, luma_chroma);
        uint8_t code_length =0;
        uint16_t code = 0;
        for( uint8_t code_length = 1; code_length < 16; code_length++ )
        {
            uint8_t tmp;
            MAMEJPEG_CHECK( mameBitstream_readBits( context->input_stream, &tmp, 1, 1 ) );
            code = ( code << 1 ) | tmp;

            uint8_t from_index = ( code_length == 0 ) ? 0 : context->info.huff_table[ac_dc][luma_chroma].offsets[ code_length - 1 ];
            uint8_t to_index = context->info.huff_table[ac_dc][luma_chroma].offsets[ code_length ];
            uint8_t length = 0;
            for( uint8_t i = from_index; i < to_index; i++ )
            {
                if( context->info.huff_table[ac_dc][luma_chroma].elements[i].code == code )
                {
                    length = context->info.huff_table[ac_dc][luma_chroma].elements[i].value;

                    uint16_t value;
                    MAMEJPEG_CHECK( mameBitstream_readBits( context->input_stream, &value, 2, length ) );
                    printf("%d - %x\n", length,value);
                    break;
                }
            }
        }

        code_length =0;
        code = 0;
        for( uint8_t code_length = 0; code_length < 16; code_length++ )
        {
            uint8_t tmp;
            MAMEJPEG_CHECK( mameBitstream_readBits( context->input_stream, &tmp, 1, 1 ) );
            code = ( code << 1 ) | tmp;

            uint8_t from_index = ( code_length == 0 ) ? 0 : context->info.huff_table[1][0].offsets[ code_length - 1 ];
            uint8_t to_index = context->info.huff_table[1][0].offsets[ code_length ];
            uint8_t length = 0;
            for( uint8_t i = from_index; i < to_index; i++ )
            {
                if( context->info.huff_table[0][0].elements[i].code == code )
                {
                    length = context->info.huff_table[0][0].elements[i].value;
                    break;
                }
            }

            if( length == 0 )
            {
                break;
            }

            uint16_t value;
            MAMEJPEG_CHECK( mameBitstream_readBits( context->input_stream, &value, 2, length ) );
            printf("%x ", value);
        }

        code_length =0;
        code = 0;
        for( uint8_t code_length = 0; code_length < 16; code_length++ )
        {
            uint8_t tmp;
            MAMEJPEG_CHECK( mameBitstream_readBits( context->input_stream, &tmp, 1, 1 ) );
            code = ( code << 1 ) | tmp;

            uint8_t from_index = ( code_length == 0 ) ? 0 : context->info.huff_table[1][0].offsets[ code_length - 1 ];
            uint8_t to_index = context->info.huff_table[1][0].offsets[ code_length ];
            uint8_t length = 0;
            for( uint8_t i = from_index; i < to_index; i++ )
            {
                if( context->info.huff_table[0][0].elements[i].code == code )
                {
                    length = context->info.huff_table[0][0].elements[i].value;
                    break;
                }
            }

            if( length == 0 )
            {
                break;
            }

            uint16_t value;
            MAMEJPEG_CHECK( mameBitstream_readBits( context->input_stream, &value, 2, length ) );
            printf("%x ", value);
        }

    }
#endif
#if 0 
    for( int i =0; i < 15; i++ )
    {
            uint8_t value;
            mameBitst[ream_readBits( context->input_stream, &value, 1, 8 );
            printf("%x\n", value);
            //mameBitstream_readBits( context->input_stream, &value, 1, 1 );
            //printf("%x\n", value);
    }
#endif
    return true;
}

bool mameJpeg_decodeSOSSegment( mameJpeg_context* context )
{
    printf("call %s\n", __func__ );

    uint16_t sos_size;
    MAMEJPEG_CHECK( mameJpeg_getSegmentSize( context, &sos_size ) );

    uint8_t num_of_components;
    MAMEJPEG_CHECK( mameBitstream_readByte( context->input_stream, &num_of_components ) );

    for( int i = 0; i < num_of_components; i++ )
    {
        uint8_t component_index;
        MAMEJPEG_CHECK( mameBitstream_readByte( context->input_stream, &component_index ) );
        uint8_t info;
        MAMEJPEG_CHECK( mameBitstream_readByte( context->input_stream, &info ) );
        context->info.component[component_index - 1].huff_table_ac_index = info >> 4;
        context->info.component[component_index - 1].huff_table_dc_index = info & 0xf;
    }

    uint8_t dummy;
    MAMEJPEG_CHECK( mameBitstream_readByte( context->input_stream, &dummy ) );
    MAMEJPEG_CHECK( dummy == 0 );
    MAMEJPEG_CHECK( mameBitstream_readByte( context->input_stream, &dummy ) );
    MAMEJPEG_CHECK( dummy == 63 );
    MAMEJPEG_CHECK( mameBitstream_readByte( context->input_stream, &dummy ) );
    MAMEJPEG_CHECK( dummy == 0 );

    while( mameJpeg_decodeMCU( context ) )
    {
    }
    return true;
}

bool mameJpeg_decodeEOISegment( mameJpeg_context* context )
{
    printf("call %s\n", __func__ );
    return true;
}

bool mameJpeg_decodeUnknownSegment( mameJpeg_context* context )
{
    printf("call %s\n", __func__ );
    return true;
}

struct {
    mameJpeg_marker marker;
    mameJpeg_decodeSegmentFuncPtr decode_func;
} decode_func_table[] = {
    { MAMEJPEG_MARKER_SOI, mameJpeg_decodeSOISegment },
    { MAMEJPEG_MARKER_DQT, mameJpeg_decodeDQTSegment },
    { MAMEJPEG_MARKER_SOF0, mameJpeg_decodeSOF0Segment },
    { MAMEJPEG_MARKER_DHT, mameJpeg_decodeDHTSegment },
    { MAMEJPEG_MARKER_SOS, mameJpeg_decodeSOSSegment },
    { MAMEJPEG_MARKER_EOI, mameJpeg_decodeEOISegment },
    { MAMEJPEG_MARKER_UNKNOWN, mameJpeg_decodeUnknownSegment },
};

bool mameJpeg_decode( mameJpeg_context* context )
{
    MAMEJPEG_NULL_CHECK( context );
    MAMEJPEG_CHECK( context->mode == MAMEJPEG_DECODE );

    mameJpeg_marker marker;
    while( mameJpeg_getNextMarker( context, &marker ) )
    {
        int func_index = 0;
        while( ( decode_func_table[ func_index ].marker != marker )
            && ( decode_func_table[ func_index ].marker != MAMEJPEG_MARKER_UNKNOWN ) )
        {
            func_index++;
        }

        MAMEJPEG_CHECK( decode_func_table[ func_index ].decode_func( context ) );
    }

    return true;
}

#if 0
bool mameJpeg_Encode( mameJpeg_context* context )
{
    MAMEJPEG_NULL_CHECK( context )
    MAMEJPEG_CHECK( context->mode == MAMEJPEG_ENCODE )

    return true;
}
#endif

bool mameJpeg_dispose( mameJpeg_context* context )
{
    return true;
}

bool mameJpeg_dumpHeader( mameJpeg_context* context )
{
    MAMEJPEG_NULL_CHECK( context );

    printf("accuracy - %d\n", context->info.accuracy);
    printf("width - %d\n", context->info.width);
    printf("height - %d\n", context->info.height);
    printf("components - %d\n", context->info.component_num);

    int dqt_table_index_max = 0;
    for( int i = 0; i < context->info.component_num; i++ )
    {
        printf("compoent %d\n", i );
        printf("  hor sampling - %d\n", context->info.component[i].hor_sampling );
        printf("  ver sampling - %d\n", context->info.component[i].ver_sampling );
        printf("  dqt index    - %d\n", context->info.component[i].quant_table_index );
        dqt_table_index_max = ( dqt_table_index_max < context->info.component[i].quant_table_index ) ? context->info.component[i].quant_table_index : dqt_table_index_max;
    }

    for( int i = 0; i <= dqt_table_index_max; i++ )
    {
        printf("dqt - %d\n", i );
        for( int j = 0; j < 64; j++ )
        {
            printf("%3d%c", context->info.quant_table[i][j], ( ( j + 1 ) % 8 == 0 ) ? '\n' : ' ');
        }
    }

    for( int ac_dc_index = 0; ac_dc_index < 2; ac_dc_index++ )
    {
        const char* ac_dc_str = ( ac_dc_index == 0 ) ? "dc" : "ac" ;
        for( int luma_chroma_index = 0; luma_chroma_index < 2; luma_chroma_index++ )
        {
            const char* luma_chrom_str = ( luma_chroma_index == 0 ) ? "luma" : "chroma";
            printf("dht - %s/%s\n",ac_dc_str,luma_chrom_str);
            for( int i = 0; i < 16; i++ )
            {
                int from_index = ( i == 0 ) ? 0 : context->info.huff_table[ac_dc_index][luma_chroma_index].offsets[i-1];
                if( from_index == context->info.huff_table[ac_dc_index][luma_chroma_index].offsets[i] )
                {
                    continue;
                }

                printf("  %d bit code\n", i);
                for( int j = from_index ; j< context->info.huff_table[ac_dc_index][luma_chroma_index].offsets[i]; j++ )
                {
                    printf("    %x - %d\n",context->info.huff_table[ac_dc_index][luma_chroma_index].elements[j].code, context->info.huff_table[ac_dc_index][luma_chroma_index].elements[j].value );
                }
            }
        }
    }

    return true;
}

# ifdef __cplusplus
}
# endif /* __cplusplus */

#endif /* __MAME_JPEG_HEADER__ */
