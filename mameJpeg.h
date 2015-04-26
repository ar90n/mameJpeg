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
    uint32_t cache;
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
    context->cache = 0x00000000;
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

    uint8_t prev_byte = 0;
    uint8_t byte = 0;
    do
    {
        prev_byte = byte;
        MAMEJPEG_CHECK( context->io_callback.read( context->callback_param, &byte ) );
    } while( byte == 0xff );
    byte = ( prev_byte == 0xff && byte == 0x00 ) ? 0xff : byte;

    uint32_t bit_shift = 8 * ( sizeof( context->cache ) - 1 ) - context->cache_use_bits;
    context->cache |= ( byte << bit_shift );
    context->cache_use_bits += 8;

    return true;
}

bool mameBitstream_tryWriteFromCache( mameBitstream_context* context )
{
    MAMEJPEG_NULL_CHECK( context );
    MAMEJPEG_CHECK( context->mode == MAMEBITSTREAM_WRITE );
    MAMEJPEG_CHECK( 8 <= context->cache_use_bits );

    uint32_t bit_shift = 8 * ( sizeof( context->cache ) - 1 );
    uint8_t tmp = (uint8_t)( context->cache >> bit_shift );
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

        uint8_t shift_width = 8 * sizeof( context->cache ) - read_bits;
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
    MAMEJPEG_NULL_CHECK( context );
    MAMEJPEG_NULL_CHECK( byte );
    MAMEJPEG_CHECK( context->mode == MAMEBITSTREAM_READ );

    MAMEJPEG_CHECK( context->io_callback.read( context->callback_param, byte ) );
    return true;
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
        uint8_t shift_width = 8 * sizeof( context->cache ) - context->cache_use_bits - write_bits;
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

bool mameBitstream_writeByte( mameBitstream_context* context, uint8_t byte )
{
    return mameBitstream_writeBits( context, &byte, 8 );
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

typedef enum{
    MAMEJPEG_MARKER_SOI = 0xffd8,
    MAMEJPEG_MARKER_DQT = 0xffdb,
    MAMEJPEG_MARKER_SOF0 = 0xffc0,
    MAMEJPEG_MARKER_DHT = 0xffc4,
    MAMEJPEG_MARKER_SOS = 0xffda,
    MAMEJPEG_MARKER_EOI = 0xffd9,
    MAMEJPEG_MARKER_DRI = 0xffdd,
    MAMEJPEG_MARKER_UNKNOWN = 0x0000,
} mameJpeg_marker;

typedef struct {
    uint8_t* line_buffer;
    size_t line_buffer_length;
    mameBitstream_context input_stream[1];
    mameBitstream_context output_stream[1];
    mameJpeg_mode mode;
    int progress;

    struct {
        uint8_t accuracy;
        uint16_t width;
        uint16_t height;
        uint8_t component_num;
        uint16_t restart_interval;
        struct {
            int hor_sampling;
            int ver_sampling;
            int quant_table_index;
            int huff_table_index[2];
            int prev_dc_value;
        } component [3];
        struct {
            uint16_t offsets[16];
            struct {
                uint16_t code;
                uint8_t value;
            } elements[512];
        } huff_table[2][2];

        double* mcu_dct_coeffs;
        double* mcu_pixels;
        uint8_t quant_table[4][64];
    } info;
} mameJpeg_context;

bool mameJpeg_initialize( mameJpeg_context* context,
                          mameJpeg_read_callback_ptr input_callback,
                          mameJpeg_rewind_callback_ptr input_rewind_callback,
                          void* input_callback_param,
                          mameJpeg_write_callback_ptr output_callback,
                          mameJpeg_rewind_callback_ptr output_rewind_callback,
                          void* output_callback_param,
                          mameJpeg_mode mode )
{
    MAMEJPEG_NULL_CHECK( context );
    MAMEJPEG_NULL_CHECK( input_callback );
    MAMEJPEG_NULL_CHECK( input_rewind_callback );
    MAMEJPEG_NULL_CHECK( output_callback );
    MAMEJPEG_NULL_CHECK( output_rewind_callback );
    MAMEJPEG_CHECK( mode == MAMEJPEG_ENCODE || mode == MAMEJPEG_DECODE );

    memset( context, 0x00, sizeof( mameJpeg_context ));
    mameBitstream_input_initialize( context->input_stream, input_callback, input_rewind_callback, input_callback_param );
    mameBitstream_output_initialize( context->output_stream, output_callback, output_rewind_callback, output_callback_param );
    context->mode = mode;

    return true;
}


bool mameJpeg_decodeSOF0Segment( mameJpeg_context* context );
bool mameJpeg_decodeDHTSegment( mameJpeg_context* context );
bool mameJpeg_getNextMarker( mameJpeg_context* context, mameJpeg_marker* marker );

bool mameJpeg_getImageInfo( mameJpeg_read_callback_ptr input_callback,
                            mameJpeg_rewind_callback_ptr rewind_callback,
                            void* input_callback_param,
                            uint16_t* width,
                            uint16_t* height,
                            uint8_t* component_num )
{
    MAMEJPEG_NULL_CHECK( input_callback );
    MAMEJPEG_NULL_CHECK( input_callback_param );

    mameJpeg_context context[1];
    mameBitstream_input_initialize( context->input_stream, input_callback, rewind_callback, input_callback_param );

    mameJpeg_marker marker;
    while( mameJpeg_getNextMarker( context, &marker ) )
    {
        if( marker == MAMEJPEG_MARKER_SOF0 )
        {
            MAMEJPEG_CHECK( mameJpeg_decodeSOF0Segment( context ) );
        }
        else if( marker == MAMEJPEG_MARKER_DHT )
        {
            MAMEJPEG_CHECK( mameJpeg_decodeDHTSegment( context ) );//TODO : eliminate here
        }
        else if( marker == MAMEJPEG_MARKER_SOS )
        {
            break;
        }
    }

    if( width != NULL )
    {
        *width = context->info.width;
    }

    if( height != NULL )
    {
        *height = context->info.height;
    }

    if( component_num != NULL )
    {
        *component_num = context->info.component_num;
    }

    return true;
}

bool mameJpeg_getWorkBufferSize( mameJpeg_context* context, size_t* work_buffer_length )
{
    MAMEJPEG_NULL_CHECK( context );
    MAMEJPEG_NULL_CHECK( work_buffer_length );

    mameJpeg_marker marker;
    while( mameJpeg_getNextMarker( context, &marker ) )
    {
        if( marker == MAMEJPEG_MARKER_SOF0 )
        {
            MAMEJPEG_CHECK( mameJpeg_decodeSOF0Segment( context ) );
        }
        else if( marker == MAMEJPEG_MARKER_DHT )
        {
            MAMEJPEG_CHECK( mameJpeg_decodeDHTSegment( context ) );//TODO : eliminate here
        }
        else if( marker == MAMEJPEG_MARKER_SOS )
        {
            break;
        }
    }

    *work_buffer_length = context->info.component_num * context->info.width * context->info.height ;
    *work_buffer_length += context->info.component_num * 64;
    *work_buffer_length += 1024 * 1024 * 4 ;

    context->input_stream->rewind_callback( context->input_stream->callback_param );
    return true;
}

bool mameJpeg_setWorkBuffer( mameJpeg_context* context, void* work_buffer, size_t work_buffer_length )
{
    MAMEJPEG_NULL_CHECK( work_buffer );
    MAMEJPEG_CHECK( 0 < work_buffer_length );

    context->info.mcu_dct_coeffs = (double*)work_buffer;
    context->info.mcu_pixels = context->info.mcu_dct_coeffs + 64;
    context->line_buffer = (uint8_t*)(context->info.mcu_pixels + 64);
    context->line_buffer_length = work_buffer_length - sizeof(double) * 128;
    return true;
}

bool mameJpeg_getProgress( mameJpeg_context* context, int* progress )
{
    MAMEJPEG_NULL_CHECK( context );
    MAMEJPEG_NULL_CHECK( progress );

    *progress = context->progress;

    return true;
}

bool mameJpeg_getNextMarker( mameJpeg_context* context, mameJpeg_marker* marker )
{
    MAMEJPEG_NULL_CHECK( context );
    MAMEJPEG_NULL_CHECK( marker );

    uint8_t prefix;
    while( mameBitstream_readByte( context->input_stream, &prefix ) )
    {
        if( prefix == 0xff )
        {
            uint8_t val;
            MAMEJPEG_CHECK( mameBitstream_readByte( context->input_stream, &val ) );

            *marker = (mameJpeg_marker)(( prefix << 8 ) | val);
            return true;
        }
    }

    return false;
}

typedef bool (*mameJpeg_decodeSegmentFuncPtr)(mameJpeg_context*);

bool mameJpeg_getSegmentSize( mameJpeg_context* context, uint16_t* size )
{
    MAMEJPEG_NULL_CHECK( context );
    MAMEJPEG_NULL_CHECK( size );

    uint16_t whole_size;
    MAMEJPEG_CHECK( mameBitstream_readTwoByte( context->input_stream, &whole_size) );
    *size = whole_size - 2;

    return true;
}

bool mameJpeg_decodeSOISegment( mameJpeg_context* context )
{
    MAMEJPEG_NULL_CHECK( context );
    printf("call %s\n", __func__ );
    return true;
}

bool mameJpeg_decodeDQTSegment( mameJpeg_context* context )
{
    MAMEJPEG_NULL_CHECK( context );
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
    MAMEJPEG_NULL_CHECK( context );
    printf("call %s\n", __func__ );
    
    uint16_t sof0_size;
    MAMEJPEG_CHECK( mameJpeg_getSegmentSize( context, &sof0_size ) );
    MAMEJPEG_CHECK( ( ( sof0_size - 6 ) % 3 ) == 0 );

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
    MAMEJPEG_NULL_CHECK( context );
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

bool mameJpeg_readImageData( mameJpeg_context* context, void* buffer, int buffer_length, int bits )
{
    return mameBitstream_readBits( context->input_stream, buffer, buffer_length, bits );
}

bool mameJpeg_writeImageData( mameJpeg_context* context, void* buffer, int bits )
{
    return mameBitstream_writeBits( context->output_stream, buffer, bits );
}

bool mameJpeg_decodeHuffmanCode( mameJpeg_context* context, uint8_t ac_dc, uint8_t table_index, uint16_t code, uint8_t code_length, uint8_t* value )
{
    MAMEJPEG_NULL_CHECK( context );
    MAMEJPEG_NULL_CHECK( value );
    MAMEJPEG_CHECK( 0 < code_length );

    uint8_t from_index = ( code_length == 0 ) ? 0 : context->info.huff_table[ac_dc][table_index].offsets[ code_length - 1 ];
    uint8_t to_index = context->info.huff_table[ac_dc][table_index].offsets[ code_length ];
    for( uint8_t i = from_index; i < to_index; i++ )
    {
        if( context->info.huff_table[ac_dc][table_index].elements[i].code == code )
        {
            *value = context->info.huff_table[ac_dc][table_index].elements[i].value;
            return true;
        }
    }

    return false;
}

bool mameJpeg_getNextDecodedValue( mameJpeg_context* context, uint8_t ac_dc, uint8_t compoent_index, uint8_t* decoded_value )
{
    MAMEJPEG_NULL_CHECK( context );
    MAMEJPEG_NULL_CHECK( decoded_value );

    uint16_t code = 0;
    for( uint8_t code_length = 1; code_length < 16; code_length++ )
    {
        uint8_t tmp;
        MAMEJPEG_CHECK( mameBitstream_readBits( context->input_stream, &tmp, 1, 1 ) );
        code = ( code << 1 ) | tmp;

        uint8_t length;
        bool ret = mameJpeg_decodeHuffmanCode( context, ac_dc, context->info.component[ compoent_index ].huff_table_index[ac_dc], code, code_length, decoded_value );
        if( ret )
        {
            return true;
        }
    }

    //TODO : detect marker.

    return false;
}

bool mameJpeg_calcDCValue( mameJpeg_context* context, uint16_t diff_value, uint8_t length, int16_t* dc_value )
{
    MAMEJPEG_NULL_CHECK( context );
    MAMEJPEG_NULL_CHECK( dc_value );
    MAMEJPEG_CHECK( 0 < length );

    diff_value &= ( 1 << length ) - 1;
    int base_value = ( ( diff_value >> ( length - 1 ) ) == 0 ) ? ( -( ( 1 << length ) - 1 ) ): ( 1 << ( length - 1 ) );
    *dc_value = ( diff_value & ( ( 1 << ( length - 1 ) ) - 1 ) ) + base_value;
    return true;
}

bool mameJpeg_decodeDC( mameJpeg_context* context, uint8_t compoent_index, bool* has_data )
{
    MAMEJPEG_NULL_CHECK( context );

    uint8_t length;
    MAMEJPEG_CHECK( mameJpeg_getNextDecodedValue( context, 0, compoent_index, &length ) );
    if( length == 0 )
    {
        context->info.mcu_dct_coeffs[0] = context->info.component[ compoent_index ].prev_dc_value;
        *has_data = false;
        return true;
    }

    *has_data = true;

    uint16_t huffman_dc_value;
    MAMEJPEG_CHECK( mameBitstream_readBits( context->input_stream, &huffman_dc_value, 2, length ) );
    int16_t diff_value = 0;
    MAMEJPEG_CHECK( mameJpeg_calcDCValue( context, huffman_dc_value, length, &diff_value ) );

    context->info.component[ compoent_index ].prev_dc_value += diff_value;
    context->info.mcu_dct_coeffs[0] = context->info.component[ compoent_index ].prev_dc_value;
    return true;
}

uint8_t mameJpeg_getZigZagIndex( mameJpeg_context* context, uint8_t index )
{
    uint8_t zigzag_index_map[] =
    {
        0,   1,  8, 16,  9,  2,  3, 10,
        17, 24, 32, 25, 18, 11,  4,  5,
        12, 19, 26, 33, 40, 48, 41, 34,
        27, 20, 13,  6,  7, 14, 21, 28,
        35, 42, 49, 56, 57, 50, 43, 36,
        29, 22, 15, 23, 30, 37, 44, 51,
        58, 59, 52, 45, 38, 31, 39, 46,
        53, 60, 61, 54, 47, 55, 62, 63
    };

    return zigzag_index_map[ index ];
}

bool mameJpeg_decodeAC( mameJpeg_context* context, uint8_t compoent_index, bool* has_data )
{
    *has_data = false;
    uint8_t elem_num = 1;
    while( elem_num < 64 )
    {
        uint8_t value;
        MAMEJPEG_CHECK( mameJpeg_getNextDecodedValue( context, 1, compoent_index, &value ) );

        uint8_t zero_run_length = value >> 4;
        uint8_t bit_length = value & 0x0f;
        if( zero_run_length == 0 && bit_length == 0 )
        {
            zero_run_length = 64 - elem_num;
        }

        for( uint8_t i = 0; i < zero_run_length; i++ )
        {
            uint8_t zigzag_index = mameJpeg_getZigZagIndex( context, elem_num );
            context->info.mcu_dct_coeffs[zigzag_index] = 0;
            elem_num++;
        }

        if( 0 < bit_length  )
        {
            *has_data = true;

            uint16_t huffman_value = 0;
            MAMEJPEG_CHECK( mameBitstream_readBits( context->input_stream, &huffman_value, 2, bit_length ) );
            int16_t ac_coeff;
            MAMEJPEG_CHECK( mameJpeg_calcDCValue( context, huffman_value, bit_length, &ac_coeff ) );

            uint8_t zigzag_index = mameJpeg_getZigZagIndex( context, elem_num );
            context->info.mcu_dct_coeffs[zigzag_index] = ac_coeff;

            elem_num++;
        }
    }

    return true;
}

bool mameJpeg_applyDCT( mameJpeg_context* context, bool is_forward )
{
    MAMEJPEG_NULL_CHECK( context );

#if 0
    for( int i = 0; i < 64; i++ )
    {
        printf("%8.3f%c", context->info.mcu_dct_coeffs[i] , ( ( i + 1 ) % 8 == 0 ) ? '\n' : ' ' );
    }
    printf("\n" );
#endif

    double res1[8][8];
    for( int y = 0; y < 8; y++ )
    {
        for( int x = 0; x < 8; x++ )
        {
            uint8_t coeff_index_base = y * 8; 
            double tmp = ( 1.0 / 1.4142 ) * context->info.mcu_dct_coeffs[coeff_index_base + 0] * cos( 3.14 / 8 * 0 * ( x + 0.5 ) );
            tmp += context->info.mcu_dct_coeffs[coeff_index_base + 1] * cos( 3.14 / 8 * 1 * ( x + 0.5 ) );
            tmp += context->info.mcu_dct_coeffs[coeff_index_base + 2] * cos( 3.14 / 8 * 2 * ( x + 0.5 ) );
            tmp += context->info.mcu_dct_coeffs[coeff_index_base + 3] * cos( 3.14 / 8 * 3 * ( x + 0.5 ) );
            tmp += context->info.mcu_dct_coeffs[coeff_index_base + 4] * cos( 3.14 / 8 * 4 * ( x + 0.5 ) );
            tmp += context->info.mcu_dct_coeffs[coeff_index_base + 5] * cos( 3.14 / 8 * 5 * ( x + 0.5 ) );
            tmp += context->info.mcu_dct_coeffs[coeff_index_base + 6] * cos( 3.14 / 8 * 6 * ( x + 0.5 ) );
            tmp += context->info.mcu_dct_coeffs[coeff_index_base + 7] * cos( 3.14 / 8 * 7 * ( x + 0.5 ) );

            res1[x][y] = tmp;
        }

    }
    for( int y = 0; y < 8; y++ )
    {
        for( int x = 0; x < 8; x++ )
        {
            double tmp = ( 1.0 / 1.4142 ) * res1[y][0] * cos( 3.14 / 8 * 0 * ( x + 0.5 ) );
            tmp += res1[y][1] * cos( 3.14 / 8 * 1 * ( x + 0.5 ) );
            tmp += res1[y][2] * cos( 3.14 / 8 * 2 * ( x + 0.5 ) );
            tmp += res1[y][3] * cos( 3.14 / 8 * 3 * ( x + 0.5 ) );
            tmp += res1[y][4] * cos( 3.14 / 8 * 4 * ( x + 0.5 ) );
            tmp += res1[y][5] * cos( 3.14 / 8 * 5 * ( x + 0.5 ) );
            tmp += res1[y][6] * cos( 3.14 / 8 * 6 * ( x + 0.5 ) );
            tmp += res1[y][7] * cos( 3.14 / 8 * 7 * ( x + 0.5 ) );
            tmp /= 4;
            tmp += 128;
            tmp = ( 255 < tmp ) ? 255 : ( ( tmp < 0 ) ? 0 : tmp );
            context->info.mcu_pixels[x * 8 + y] = tmp;
        }
    }

    return true;
}

bool mameJpeg_applyInverseQunatize( mameJpeg_context* context, uint8_t table_index  )
{
    MAMEJPEG_NULL_CHECK( context );

    for( int i = 0; i < ( 8 * 8 ); i++ )
    {
        uint8_t zigzag_index = mameJpeg_getZigZagIndex( context, i );
        context->info.mcu_dct_coeffs[zigzag_index] *= context->info.quant_table[table_index][i];
    }

    return true;
}

bool mameJpeg_clearDCTCoeffBuffer( mameJpeg_context* context )
{
    for( int i = 0; i < 64; i++ )
    {
        context->info.mcu_dct_coeffs[i] = 0.0;
    }

    return true;
}

bool mameJpeg_moveMCUToBuffer( mameJpeg_context* context, uint16_t hor_mcu_index, uint16_t ver_mcu_index, uint8_t compoent_index )
{
    MAMEJPEG_CHECK( context->info.component_num == 1 || context->info.component_num == 3 );

    if( context->info.component_num == 1 )
    {
        for( int y = 0; y < 8; y++ )
        {
            for( int x = 0; x < 8; x++ )
            {
                uint16_t index = y * context->info.width + 8 * hor_mcu_index + x;
                context->line_buffer[ index ] = ( uint8_t )(context->info.mcu_pixels[y * 8 + x]);
            }
        }
    }
    else
    {
        for( int y = 0; y < 8; y++ )
        {
            for( int x = 0; x < 8; x++ )
            {
                double yuv_rgb_coeff[3][4] = { 
                    { 1.0, 1.0, 1.0, 0.0 },
                    { 0.0, -0.187324, 1.8556, 128.0 },
                    { 1.5748, -0.468124, 0.0, 128.0 }
                };
                /*
                double yuv_rgb_coeff[3][4] = { 
                    { 1.0, 1.0, 1.0, 0.0 },
                    { 1.0, 1.0, 1.0, 128.0 },
                    { 1.0, 1.0, 1.0, 128.0 }
                };
                */
                if( compoent_index == 3 )
                {
                    continue;
                }
                uint16_t index = 3 * ( y * context->info.width + 8 * hor_mcu_index + x );
                double tmpR = context->line_buffer[ index + 0 ] + yuv_rgb_coeff[compoent_index][0] * ( context->info.mcu_pixels[y * 8 + x] - yuv_rgb_coeff[compoent_index][3] );
                tmpR = ( 255.0 < tmpR ) ? 255.0 : ( ( tmpR < 0 ) ? 0 : tmpR );
                context->line_buffer[ index + 0 ] += (uint8_t) tmpR;

                double tmpG = context->line_buffer[ index + 1 ] + yuv_rgb_coeff[compoent_index][1] * ( context->info.mcu_pixels[y * 8 + x] - yuv_rgb_coeff[compoent_index][3] );
                printf("tmpG:%lf\n",tmpG);
                tmpG = ( 255.0 < tmpG ) ? 255.0 : ( ( tmpG < 0 ) ? 0 : tmpG );
                context->line_buffer[ index + 1 ] += (uint8_t) tmpG;

                double tmpB = context->line_buffer[ index + 2 ] + yuv_rgb_coeff[compoent_index][2] * ( context->info.mcu_pixels[y * 8 + x] - yuv_rgb_coeff[compoent_index][3] );
                tmpB = ( 255.0 < tmpB ) ? 255.0 : ( ( tmpB < 0 ) ? 0 : tmpB );
                context->line_buffer[ index + 2 ] += (uint8_t) tmpB;

                printf("%d %d %d %d %d %d %d %d\n", context->line_buffer[ index + 0 ], context->line_buffer[ index + 1 ], context->line_buffer[ index + 2 ], hor_mcu_index, ver_mcu_index, x, y, index  );
            }
        }
    }

    return true;
}

bool mameJpeg_decodeMCU( mameJpeg_context* context, uint16_t hor_mcu_index, uint16_t ver_mcu_index )
{
    MAMEJPEG_NULL_CHECK( context );

    for( int i = 0; i < context->info.component_num; i++ )
    {
        bool has_dc_data = false;
        bool has_ac_data = false;
        MAMEJPEG_CHECK( mameJpeg_clearDCTCoeffBuffer( context ) );
        MAMEJPEG_CHECK( mameJpeg_decodeDC( context, i ,&has_dc_data ) );
        MAMEJPEG_CHECK( mameJpeg_decodeAC( context, i ,&has_ac_data ) );

        if( has_dc_data || has_ac_data )
        {
            MAMEJPEG_CHECK( mameJpeg_applyInverseQunatize( context, i ) );
            MAMEJPEG_CHECK( mameJpeg_applyDCT( context, false ) );
        }

        mameJpeg_moveMCUToBuffer( context, hor_mcu_index, ver_mcu_index, i );
    }

    return true;
}

bool mameJpeg_restartDecode( mameJpeg_context* context )
{
    MAMEJPEG_NULL_CHECK( context );

    context->info.component[0].prev_dc_value = 0;
    context->info.component[1].prev_dc_value = 0;
    context->info.component[2].prev_dc_value = 0;
    return true;
}

bool mameJpeg_writeBuffer( mameJpeg_context* context )
{
    for( uint16_t i = 0; i < 8 * context->info.component_num * context->info.width; i++ )
    {
        printf("%d %d\n", i, context->line_buffer[i] );
        mameBitstream_writeByte( context->output_stream, context->line_buffer[i] );
    }

    return true;
}

bool mameJpeg_decodeImage( mameJpeg_context* context )
{
    int restart_interval = context->info.restart_interval;

    uint16_t hor_mcu_num = context->info.width >> 3;
    uint16_t ver_mcu_num = context->info.width >> 3;
    for( uint16_t ver_mcu_index = 0; ver_mcu_index < ver_mcu_num; ver_mcu_index++ )
    {
        for( uint16_t hor_mcu_index = 0; hor_mcu_index < hor_mcu_num; hor_mcu_index++)
        {
            mameJpeg_decodeMCU( context, hor_mcu_index, ver_mcu_index );

            restart_interval--;
            if( restart_interval == 0 )
            {
                mameJpeg_restartDecode( context );
                restart_interval = context->info.restart_interval;
            }
        }

        mameJpeg_writeBuffer( context );
    }

    //cache invalidate
    context->input_stream->cache = 0;
    context->input_stream->cache_use_bits = 0;

    return true;
}

bool mameJpeg_decodeSOSSegment( mameJpeg_context* context )
{
    MAMEJPEG_NULL_CHECK( context );
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
        context->info.component[component_index - 1].huff_table_index[0] = info & 0xf;
        context->info.component[component_index - 1].huff_table_index[1] = info >> 4;
    }

    uint8_t dummy;
    MAMEJPEG_CHECK( mameBitstream_readByte( context->input_stream, &dummy ) );
    MAMEJPEG_CHECK( dummy == 0 );
    MAMEJPEG_CHECK( mameBitstream_readByte( context->input_stream, &dummy ) );
    MAMEJPEG_CHECK( dummy == 63 );
    MAMEJPEG_CHECK( mameBitstream_readByte( context->input_stream, &dummy ) );
    MAMEJPEG_CHECK( dummy == 0 );

    MAMEJPEG_CHECK( mameJpeg_decodeImage( context ) );
    return true;
}

bool mameJpeg_decodeEOISegment( mameJpeg_context* context )
{
    MAMEJPEG_NULL_CHECK( context );
    printf("call %s\n", __func__ );
    return true;
}

bool mameJpeg_decodeDRISegment( mameJpeg_context* context )
{
    MAMEJPEG_NULL_CHECK( context );
    printf("call %s\n", __func__ );

    uint16_t dri_size;
    MAMEJPEG_CHECK( mameJpeg_getSegmentSize( context, &dri_size ) );
    MAMEJPEG_CHECK( dri_size == 2 );

    MAMEJPEG_CHECK( mameBitstream_readTwoByte( context->input_stream, &context->info.restart_interval ) );

    return true;
}

bool mameJpeg_decodeUnknownSegment( mameJpeg_context* context )
{
    MAMEJPEG_NULL_CHECK( context );
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
    { MAMEJPEG_MARKER_DRI, mameJpeg_decodeDRISegment },
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
    MAMEJPEG_NULL_CHECK( context );
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

                printf("  %d bit code ( %d - %d )\n", i, from_index,context->info.huff_table[ac_dc_index][luma_chroma_index].offsets[i] );
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
