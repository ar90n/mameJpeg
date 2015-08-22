#ifndef __MAME_JPEG_HEADER__
# define __MAME_JPEG_HEADER__

# ifdef __cplusplus
extern "C" {
# endif /* __cplusplus */

# include <stdbool.h>
# include <string.h>
# include <stdlib.h>
# include <math.h>

/* data type definitions */
typedef bool (*mameJpeg_stream_read_callback_ptr)( void* param, uint8_t* byte );
typedef bool (*mameJpeg_stream_write_callback_ptr)( void* param, uint8_t byte );

struct mameJpeg_context_t;
typedef struct mameJpeg_context_t mameJpeg_context;

typedef enum {
    MAMEJPEG_MODE_ENCODE = 0,
    MAMEJPEG_MODE_DECODE = 1,
} mameJpeg_mode;

typedef enum {
    MAMEJPEG_FORMAT_YCBCR_444  = 0,
    MAMEJPEG_FORMAT_YCBCR_422v = 1,
    MAMEJPEG_FORMAT_YCBCR_422h = 2,
    MAMEJPEG_FORMAT_YCBCR_420  = 3,
    MAMEJPEG_FORMAT_Y_444      = 4,
} mameJpeg_format;

/* prototype definitions of mameJpeg API. */
bool mameJpeg_getEncodeBufferSize( size_t width, size_t height, mameJpeg_format format, size_t* encode_buffer_size );
bool mameJpeg_initializeEncode( mameJpeg_context* context,
                                mameJpeg_stream_read_callback_ptr input_callback,
                                void* input_callback_param,
                                mameJpeg_stream_write_callback_ptr output_callback,
                                void* output_callback_param,
                                size_t width,
                                size_t height,
                                mameJpeg_format format,
                                uint8_t* work_buffer,
                                size_t work_buffer_size );
bool mameJpeg_encode( mameJpeg_context* context );

bool mameJpeg_getDecodeBufferSize( mameJpeg_stream_read_callback_ptr read_callback_ptr,
                                   void* read_callback_param,
                                   uint16_t* width_ptr,
                                   uint16_t* height_ptr,
                                   uint8_t* component_num_ptr,
                                   size_t* decode_buffer_size_ptr );
bool mameJpeg_initializeDecode( mameJpeg_context* context,
                                mameJpeg_stream_read_callback_ptr input_callback,
                                void* input_callback_param,
                                mameJpeg_stream_write_callback_ptr output_callback,
                                void* output_callback_param,
                                uint8_t* work_buffer,
                                size_t work_buffer_size );
bool mameJpeg_decode( mameJpeg_context* context );

/* prototype definitions of refarence callbacks */
typedef struct {
    void* buffer_ptr;
    size_t buffer_pos;
    size_t buffer_size;
} mameJpeg_memory_callback_param;
bool mameJpeg_input_from_memory_callback( void* param, uint8_t* byte );
bool mameJpeg_output_to_memory_callback( void* param, uint8_t byte );
bool mameJpeg_input_from_file_callback( void* param, uint8_t* byte );
bool mameJpeg_output_to_file_callback( void* param, uint8_t byte );

#define MAMEJPEG_CHECK( X ) do{if((X)==false )return false;}while(0)
#define MAMEJPEG_NULL_CHECK( X )  MAMEJPEG_CHECK((X)!=NULL)

#define MAMEJPEG_MAX( VAL1, VAL2 ) ( ( VAL1 < VAL2 ) ? VAL2 : VAL1 )
#define MAMEJPEG_MIN( VAL1, VAL2 ) ( ( VAL1 < VAL2 ) ? VAL1 : VAL2 )
#define MAMEJPEG_CLIP( VAL, LOW, HIGH ) MAMEJPEG_MIN( MAMEJPEG_MAX( VAL, LOW ), HIGH )

typedef enum
{
    MAMEBITSTREAM_READ,
    MAMEBITSTREAM_WRITE,
} mameJpeg_stream_mode;

typedef struct {
    union {
        mameJpeg_stream_read_callback_ptr read;
        mameJpeg_stream_write_callback_ptr write;
    } io_callback;
    void* callback_param;
    uint32_t cache;
    int cache_use_bits;
    mameJpeg_stream_mode mode;
} mameJpeg_stream_context;

static
bool mameJpeg_stream_input_initialize( mameJpeg_stream_context* context,
        mameJpeg_stream_read_callback_ptr read_callback,
        void* callback_param )
{
    MAMEJPEG_NULL_CHECK( context );
    MAMEJPEG_NULL_CHECK( read_callback );

    context->io_callback.read = read_callback;
    context->callback_param = callback_param;
    context->cache = 0x0000;
    context->cache_use_bits = 0;
    context->mode = MAMEBITSTREAM_READ;

    return true;
}

static
bool mameJpeg_stream_output_initialize( mameJpeg_stream_context* context,
                                        mameJpeg_stream_write_callback_ptr write_callback,
                                        void* callback_param )
{
    MAMEJPEG_NULL_CHECK( context );
    MAMEJPEG_NULL_CHECK( write_callback );

    context->io_callback.write = write_callback;
    context->callback_param = callback_param;
    context->cache = 0x00000000;
    context->cache_use_bits = 0;
    context->mode = MAMEBITSTREAM_WRITE;

    return true;
}

static
bool mameJpeg_stream_tryReadIntoCache( mameJpeg_stream_context* context )
{
    MAMEJPEG_NULL_CHECK( context );
    MAMEJPEG_CHECK( context->mode == MAMEBITSTREAM_READ );
    MAMEJPEG_CHECK( context->cache_use_bits < 8 );

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

static
bool mameJpeg_stream_readBits( mameJpeg_stream_context* context,
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
            MAMEJPEG_CHECK( mameJpeg_stream_tryReadIntoCache( context ) );
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

static
bool mameJpeg_stream_tryWriteFromCache( mameJpeg_stream_context* context )
{
    MAMEJPEG_NULL_CHECK( context );
    MAMEJPEG_CHECK( context->mode == MAMEBITSTREAM_WRITE );
    MAMEJPEG_CHECK( 8 <= context->cache_use_bits );

    uint32_t bit_shift = 8 * ( sizeof( context->cache ) - 1 );
    uint8_t tmp = (uint8_t)( context->cache >> bit_shift );
    MAMEJPEG_CHECK( context->io_callback.write( context->callback_param, tmp ) );

    if( tmp == 0xff )
    {
        uint8_t tmp = 0x00;
        MAMEJPEG_CHECK( context->io_callback.write( context->callback_param, tmp ) );
    }

    context->cache <<= 8;
    context->cache_use_bits -= 8;

    return true;
}

static
bool mameJpeg_stream_writeBits( mameJpeg_stream_context* context, void* buffer, int bits )
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
            MAMEJPEG_CHECK( mameJpeg_stream_tryWriteFromCache( context ) );
        }
    }

    return ( bits == 0 );
}

static
bool mameJpeg_stream_flushBits( mameJpeg_stream_context* context )
{
    MAMEJPEG_NULL_CHECK( context );

    while( 8 < context->cache_use_bits )
    {
        MAMEJPEG_CHECK( mameJpeg_stream_tryWriteFromCache( context ) );
    }

    if( 0 < context->cache_use_bits )
    {
        uint32_t bit_shift = 8 * ( sizeof( context->cache ) - 1 );
        uint8_t tmp = (uint8_t)( context->cache >> bit_shift );
        MAMEJPEG_CHECK( context->io_callback.write( context->callback_param, tmp ) );
    }

    context->cache = 0;
    context->cache_use_bits = 0;

    return true;
}

static
bool mameJpeg_stream_readByte( mameJpeg_stream_context* context, uint8_t* byte )
{
    MAMEJPEG_NULL_CHECK( context );
    MAMEJPEG_NULL_CHECK( byte );
    MAMEJPEG_CHECK( context->mode == MAMEBITSTREAM_READ );

    MAMEJPEG_CHECK( context->io_callback.read( context->callback_param, byte ) );
    return true;
}

static
bool mameJpeg_stream_readTwoBytes( mameJpeg_stream_context* context, uint16_t* byte )
{
    uint8_t high;
    MAMEJPEG_CHECK( mameJpeg_stream_readByte( context, &high ) );
    uint8_t low;
    MAMEJPEG_CHECK( mameJpeg_stream_readByte( context, &low ) );
    *byte = ( high << 8 ) | low;

    return true ;
}

static
bool mameJpeg_stream_writeByte( mameJpeg_stream_context* context, uint8_t byte )
{
    MAMEJPEG_NULL_CHECK( context );
    MAMEJPEG_CHECK( context->mode == MAMEBITSTREAM_WRITE );

    MAMEJPEG_CHECK( mameJpeg_stream_flushBits( context ) );
    MAMEJPEG_CHECK( context->io_callback.write( context->callback_param, byte ) );
    return true;
}

static
bool mameJpeg_stream_writeTwoBytes( mameJpeg_stream_context* context, uint16_t two_bytes )
{
    MAMEJPEG_NULL_CHECK( context );

    MAMEJPEG_CHECK( mameJpeg_stream_flushBits( context ) );
    uint8_t high = two_bytes >> 8;
    MAMEJPEG_CHECK( mameJpeg_stream_writeByte( context, high ) );
    uint8_t low = two_bytes & 0xff;
    MAMEJPEG_CHECK( mameJpeg_stream_writeByte( context, low ) );

    return true ;
}

typedef double mameJpeg_dct_element;

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
    uint16_t code;
    uint8_t value;
}huffman_element;

struct mameJpeg_context_t {
    uint8_t* line_buffer;
    size_t line_buffer_length;
    mameJpeg_stream_context input_stream[1];
    mameJpeg_stream_context output_stream[1];
    mameJpeg_mode mode;

    struct {
        uint8_t accuracy;
        uint16_t width;
        uint16_t height;
        uint8_t component_num;
        uint16_t restart_interval;
        struct {
            uint8_t hor_sampling;
            uint8_t ver_sampling;
            uint8_t quant_table_index;
            uint8_t huff_table_index[2];
            int prev_dc_value;
        } component [3];
        struct {
            uint16_t offsets[17];
            huffman_element* elements;
        } huff_table[2][2];

        uint8_t* quant_table[4];

        double* dct_work_buffer;
        double* mcu_pixels;

    } info;

    uint8_t* work_buffer_beg;
    uint8_t* work_buffer_end;
    uint8_t* work_buffer_ptr;
};

static
bool mameJpeg_assignBuffer( mameJpeg_context* context, size_t size, void** buffer_ptr )
{
    MAMEJPEG_NULL_CHECK( context );
    MAMEJPEG_NULL_CHECK( buffer_ptr );
    MAMEJPEG_CHECK( ( context->work_buffer_ptr + size ) <= context->work_buffer_end );

    *buffer_ptr = context->work_buffer_ptr;
    context->work_buffer_ptr += size;

    return true;
}

static
uint8_t mameJpeg_getComponentNum( mameJpeg_format format )
{
    return ( format == MAMEJPEG_FORMAT_Y_444 ) ? 1 : 3 ;
}

static
uint8_t mameJpeg_getLumaHorSampling( mameJpeg_format format )
{
    return ( format == MAMEJPEG_FORMAT_YCBCR_422h || format == MAMEJPEG_FORMAT_YCBCR_420 ) ? 2 : 1;
}

static
uint8_t mameJpeg_getLumaVerSampling( mameJpeg_format format )
{
    return ( format == MAMEJPEG_FORMAT_YCBCR_422v || format == MAMEJPEG_FORMAT_YCBCR_420 ) ? 2 : 1;
}

static
size_t mameJpeg_getLineBufferSize( uint16_t width, uint8_t component_num, uint8_t ver_sampling )
{
    size_t hor_bytes = component_num * width;
    size_t ver_bytes = ver_sampling * 8;
    return hor_bytes * ver_bytes;
}

static
size_t mameJpeg_getMCUBufferSize( uint16_t width, uint8_t component_num, uint8_t luma_hor_sampling, uint8_t luma_ver_sampling )
{
    size_t hor_bytes = component_num * luma_hor_sampling * 8;
    size_t ver_bytes = luma_ver_sampling * 8;
    return sizeof( mameJpeg_dct_element ) * hor_bytes * ver_bytes;
}

static
bool mameJpeg_getNumOfMCU( mameJpeg_context* context, uint16_t* hor_mcu_num, uint16_t* ver_mcu_num )
{
    MAMEJPEG_NULL_CHECK( context );
    MAMEJPEG_NULL_CHECK( hor_mcu_num );
    MAMEJPEG_NULL_CHECK( ver_mcu_num );

    uint8_t mcu_width_bits = 3 + context->info.component[0].hor_sampling - 1;
    uint8_t mcu_height_bits = 3 + context->info.component[0].ver_sampling - 1;
    *hor_mcu_num = ( context->info.width + ( 1 << mcu_width_bits ) - 1 ) >> mcu_width_bits;
    *ver_mcu_num = ( context->info.height + ( 1 << mcu_height_bits ) - 1 ) >> mcu_height_bits;

    return true;
}

static
uint8_t mameJpeg_getZigZagIndex( mameJpeg_context* context, uint8_t index )
{
    static const uint8_t zigzag_index_map[] =
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

static
bool mameJpeg_applyDCT( mameJpeg_context* context, bool is_forward )
{
    MAMEJPEG_NULL_CHECK( context );

#if 0
    for( int i = 0; i < 64; i++ )
    {
        printf("%8.3f%c", context->info.dct_work_buffer[i] , ( ( i + 1 ) % 8 == 0 ) ? '\n' : ' ' );
    }
    printf("\n" );
#endif

    if( is_forward )
    {
        double res1[8][8];
        for( int y = 0; y < 8; y++ )
        {
            for( int x = 0; x < 8; x++ )
            {
                uint8_t coeff_index_base = y * 8; 
                double tmp = context->info.dct_work_buffer[coeff_index_base + 0] * cos( 3.14 / 8 * x * ( 0 + 0.5 ) );
                tmp += context->info.dct_work_buffer[coeff_index_base + 1] * cos( 3.14 / 8 * x * ( 1 + 0.5 ) );
                tmp += context->info.dct_work_buffer[coeff_index_base + 2] * cos( 3.14 / 8 * x * ( 2 + 0.5 ) );
                tmp += context->info.dct_work_buffer[coeff_index_base + 3] * cos( 3.14 / 8 * x * ( 3 + 0.5 ) );
                tmp += context->info.dct_work_buffer[coeff_index_base + 4] * cos( 3.14 / 8 * x * ( 4 + 0.5 ) );
                tmp += context->info.dct_work_buffer[coeff_index_base + 5] * cos( 3.14 / 8 * x * ( 5 + 0.5 ) );
                tmp += context->info.dct_work_buffer[coeff_index_base + 6] * cos( 3.14 / 8 * x * ( 6 + 0.5 ) );
                tmp += context->info.dct_work_buffer[coeff_index_base + 7] * cos( 3.14 / 8 * x * ( 7 + 0.5 ) );
                if( x == 0 )
                {
                    tmp /= 1.4142;
                }
                res1[x][y] = tmp;
            }

        }
        for( int y = 0; y < 8; y++ )
        {
            for( int x = 0; x < 8; x++ )
            {
                uint16_t dst_index = x * 8 + y;
                double tmp = res1[y][0] * cos( 3.14 / 8 * x * ( 0 + 0.5 ) );
                tmp += res1[y][1] * cos( 3.14 / 8 * x * ( 1 + 0.5 ) );
                tmp += res1[y][2] * cos( 3.14 / 8 * x * ( 2 + 0.5 ) );
                tmp += res1[y][3] * cos( 3.14 / 8 * x * ( 3 + 0.5 ) );
                tmp += res1[y][4] * cos( 3.14 / 8 * x * ( 4 + 0.5 ) );
                tmp += res1[y][5] * cos( 3.14 / 8 * x * ( 5 + 0.5 ) );
                tmp += res1[y][6] * cos( 3.14 / 8 * x * ( 6 + 0.5 ) );
                tmp += res1[y][7] * cos( 3.14 / 8 * x * ( 7 + 0.5 ) );
                if( x == 0 )
                {
                    tmp /= 1.4142;
                }
                tmp /= 4;
                context->info.dct_work_buffer[ dst_index ] = tmp;
            }
        }
    }
    else
    {
        double res1[8][8];
        for( int y = 0; y < 8; y++ )
        {
            for( int x = 0; x < 8; x++ )
            {
                uint8_t coeff_index_base = y * 8; 
                double tmp = ( 1.0 / 1.4142 ) * context->info.dct_work_buffer[coeff_index_base + 0] * cos( 3.14 / 8 * 0 * ( x + 0.5 ) );
                tmp += context->info.dct_work_buffer[coeff_index_base + 1] * cos( 3.14 / 8 * 1 * ( x + 0.5 ) );
                tmp += context->info.dct_work_buffer[coeff_index_base + 2] * cos( 3.14 / 8 * 2 * ( x + 0.5 ) );
                tmp += context->info.dct_work_buffer[coeff_index_base + 3] * cos( 3.14 / 8 * 3 * ( x + 0.5 ) );
                tmp += context->info.dct_work_buffer[coeff_index_base + 4] * cos( 3.14 / 8 * 4 * ( x + 0.5 ) );
                tmp += context->info.dct_work_buffer[coeff_index_base + 5] * cos( 3.14 / 8 * 5 * ( x + 0.5 ) );
                tmp += context->info.dct_work_buffer[coeff_index_base + 6] * cos( 3.14 / 8 * 6 * ( x + 0.5 ) );
                tmp += context->info.dct_work_buffer[coeff_index_base + 7] * cos( 3.14 / 8 * 7 * ( x + 0.5 ) );

                res1[x][y] = tmp;
            }
        }
        for( int y = 0; y < 8; y++ )
        {
            for( int x = 0; x < 8; x++ )
            {
                uint16_t dst_index = x * 8 + y;
                double tmp = ( 1.0 / 1.4142 ) * res1[y][0] * cos( 3.14 / 8 * 0 * ( x + 0.5 ) );
                tmp += res1[y][1] * cos( 3.14 / 8 * 1 * ( x + 0.5 ) );
                tmp += res1[y][2] * cos( 3.14 / 8 * 2 * ( x + 0.5 ) );
                tmp += res1[y][3] * cos( 3.14 / 8 * 3 * ( x + 0.5 ) );
                tmp += res1[y][4] * cos( 3.14 / 8 * 4 * ( x + 0.5 ) );
                tmp += res1[y][5] * cos( 3.14 / 8 * 5 * ( x + 0.5 ) );
                tmp += res1[y][6] * cos( 3.14 / 8 * 6 * ( x + 0.5 ) );
                tmp += res1[y][7] * cos( 3.14 / 8 * 7 * ( x + 0.5 ) );
                tmp /= 4;
                context->info.dct_work_buffer[ dst_index ] = tmp;
            }
        }
    }

#if 0
    for( int i = 0; i < 64; i++ )
    {
        printf("%8.3f%c", context->info.dct_work_buffer[i] , ( ( i + 1 ) % 8 == 0 ) ? '\n' : ' ' );
    }
    printf("\n" );
#endif
    return true;
}

static const double MAMEJPEG_YCBCR_TO_RGB_COEFF[3][4] = { 
    { 1.0, 1.0, 1.0, 128.0 },
    { 0.0, -0.344, 1.772, 0.0 },
    { 1.402, -0.714, 0.0, 0.0 }
};

static
bool mameJpeg_getSegmentSize( mameJpeg_context* context, uint16_t* size )
{
    MAMEJPEG_NULL_CHECK( context );
    MAMEJPEG_NULL_CHECK( size );

    uint16_t whole_size;
    MAMEJPEG_CHECK( mameJpeg_stream_readTwoBytes( context->input_stream, &whole_size) );
    *size = whole_size - 2;

    return true;
}
static
bool mameJpeg_getNextMarker( mameJpeg_context* context, mameJpeg_marker* marker )
{
    MAMEJPEG_NULL_CHECK( context );
    MAMEJPEG_NULL_CHECK( marker );

    uint8_t prefix;
    while( mameJpeg_stream_readByte( context->input_stream, &prefix ) )
    {
        if( prefix == 0xff )
        {
            uint8_t val;
            MAMEJPEG_CHECK( mameJpeg_stream_readByte( context->input_stream, &val ) );

            *marker = (mameJpeg_marker)(( prefix << 8 ) | val);
            return true;
        }
    }

    return false;
}

static
bool mameJpeg_restartDecode( mameJpeg_context* context )
{
    MAMEJPEG_NULL_CHECK( context );

    context->info.component[0].prev_dc_value = 0;
    context->info.component[1].prev_dc_value = 0;
    context->info.component[2].prev_dc_value = 0;
    return true;
}

static
bool mameJpeg_getOutputBytes( mameJpeg_context* context, uint16_t* output_bytes )
{
    MAMEJPEG_NULL_CHECK( context );
    MAMEJPEG_NULL_CHECK( output_bytes );

    *output_bytes = context->info.component_num * 8 * context->info.component[0].ver_sampling * context->info.width;
    return true;
}

static
bool mameJpeg_writeBuffer( mameJpeg_context* context )
{
    uint16_t output_bytes = 0;
    MAMEJPEG_CHECK( mameJpeg_getOutputBytes( context, &output_bytes ) );

    for( uint16_t i = 0; i < output_bytes; i++ )
    {
        mameJpeg_stream_writeByte( context->output_stream, context->line_buffer[i] );
    }

    return true;
}

static
bool mameJpeg_decodeHuffmanCode( mameJpeg_context* context, uint8_t ac_dc, uint8_t table_index, uint16_t code, uint8_t code_length, uint8_t* value )
{
    MAMEJPEG_NULL_CHECK( context );
    MAMEJPEG_NULL_CHECK( value );
    MAMEJPEG_CHECK( 0 < code_length );

    uint16_t from_index = ( code_length == 0 ) ? 0 : context->info.huff_table[ac_dc][table_index].offsets[ code_length - 1 ];
    uint16_t to_index = context->info.huff_table[ac_dc][table_index].offsets[ code_length ];
    for( uint16_t i = from_index; i < to_index; i++ )
    {
        if( context->info.huff_table[ac_dc][table_index].elements[i].code == code )
        {
            *value = context->info.huff_table[ac_dc][table_index].elements[i].value;
            return true;
        }
    }

    return false;
}

static
bool mameJpeg_getNextDecodedValue( mameJpeg_context* context, uint8_t ac_dc, uint8_t component_index, uint8_t* decoded_value )
{
    MAMEJPEG_NULL_CHECK( context );
    MAMEJPEG_NULL_CHECK( decoded_value );

    uint16_t code = 0;
    for( uint8_t code_length = 1; code_length <= 16; code_length++ )
    {
        uint8_t tmp;
        MAMEJPEG_CHECK( mameJpeg_stream_readBits( context->input_stream, &tmp, 1, 1 ) );
        code = ( code << 1 ) | tmp;

        bool ret = mameJpeg_decodeHuffmanCode( context, ac_dc, context->info.component[ component_index ].huff_table_index[ac_dc], code, code_length, decoded_value );
        if( ret )
        {
            return true;
        }
    }

    return false;
}

static
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

static
bool mameJpeg_decodeDC( mameJpeg_context* context, uint8_t component_index )
{
    MAMEJPEG_NULL_CHECK( context );

    uint8_t length = 0;
    MAMEJPEG_CHECK( mameJpeg_getNextDecodedValue( context, 0, component_index, &length ) );
    if( length == 0 )
    {
        context->info.dct_work_buffer[0] = context->info.component[ component_index ].prev_dc_value;
        return true;
    }

    uint16_t huffman_dc_value = 0;
    MAMEJPEG_CHECK( mameJpeg_stream_readBits( context->input_stream, &huffman_dc_value, 2, length ) );
    int16_t diff_value = 0;
    MAMEJPEG_CHECK( mameJpeg_calcDCValue( context, huffman_dc_value, length, &diff_value ) );

    context->info.component[ component_index ].prev_dc_value += diff_value;
    context->info.dct_work_buffer[0] = context->info.component[ component_index ].prev_dc_value;
    return true;
}

static
bool mameJpeg_decodeAC( mameJpeg_context* context, uint8_t component_index )
{
    uint8_t elem_num = 1;
    while( elem_num < 64 )
    {
        uint8_t value;
        MAMEJPEG_CHECK( mameJpeg_getNextDecodedValue( context, 1, component_index, &value ) );

        uint8_t zero_run_length = value >> 4;
        uint8_t bit_length = value & 0x0f;
        if( zero_run_length == 0 && bit_length == 0 )
        {
            zero_run_length = 64 - elem_num;
        }
        else if( zero_run_length == 15 && bit_length == 0 )
        {
            zero_run_length = 16;
        }

        for( uint8_t i = 0; i < zero_run_length; i++ )
        {
            uint8_t zigzag_index = mameJpeg_getZigZagIndex( context, elem_num );
            context->info.dct_work_buffer[zigzag_index] = 0;
            elem_num++;
        }

        if( 0 < bit_length  )
        {
            uint16_t huffman_value = 0;
            MAMEJPEG_CHECK( mameJpeg_stream_readBits( context->input_stream, &huffman_value, 2, bit_length ) );
            int16_t ac_coeff = 0;
            MAMEJPEG_CHECK( mameJpeg_calcDCValue( context, huffman_value, bit_length, &ac_coeff ) );

            uint8_t zigzag_index = mameJpeg_getZigZagIndex( context, elem_num );
            context->info.dct_work_buffer[zigzag_index] = ac_coeff;

            elem_num++;
        }
    }

    return true;
}

static
bool mameJpeg_applyInverseQunatize( mameJpeg_context* context, uint8_t component_index  )
{
    MAMEJPEG_NULL_CHECK( context );

    uint8_t table_index = context->info.component[ component_index ].quant_table_index;
    for( int i = 0; i < 64; i++ )
    {
        uint8_t zigzag_index = mameJpeg_getZigZagIndex( context, i );
        context->info.dct_work_buffer[zigzag_index] *= context->info.quant_table[table_index][i];
    }

    return true;
}

static
bool mameJpeg_applyColorConvert( mameJpeg_context* context, uint16_t hor_block, uint16_t ver_block, uint8_t component_index )
{
    MAMEJPEG_NULL_CHECK( context );

    int diff_hor_sampling = context->info.component[0].hor_sampling - context->info.component[ component_index ].hor_sampling;
    int diff_ver_sampling = context->info.component[0].ver_sampling - context->info.component[ component_index ].ver_sampling;
    int block_width = 8 * ( diff_hor_sampling + 1 );
    int block_height = 8 * ( diff_ver_sampling + 1 );

    int y_offset = 8 * ver_block;
    int x_offset = 8 * hor_block;
    int mcu_width = 8 * context->info.component[0].hor_sampling;

    for( int y = 0; y < block_height; y++ )
    {
        uint16_t dst_index = context->info.component_num * ( ( y + y_offset ) * mcu_width + x_offset );
        for( int x = 0; x < block_width; x++ )
        {
            uint16_t src_index = 8 * ( y >> diff_ver_sampling ) + ( x >> diff_hor_sampling );
            for( int i = 0; i < context->info.component_num; i++ )
            {
                context->info.mcu_pixels[ dst_index ] += MAMEJPEG_YCBCR_TO_RGB_COEFF[component_index][i] * ( context->info.dct_work_buffer[ src_index ] + MAMEJPEG_YCBCR_TO_RGB_COEFF[component_index][3] );
                dst_index++;
            }
        }
    }

    return true;
}

static
bool mameJpeg_moveMCUToBuffer( mameJpeg_context* context, uint16_t hor_mcu_index, uint16_t ver_mcu_index )
{
    MAMEJPEG_CHECK( context->info.component_num == 1 || context->info.component_num == 3 );

    uint16_t mcu_width = 8 * context->info.component[0].hor_sampling;
    uint16_t mcu_height = 8 * context->info.component[0].ver_sampling;
    uint16_t remain_width = MAMEJPEG_MIN( mcu_width, context->info.width - hor_mcu_index * mcu_width );
    uint16_t remain_height = MAMEJPEG_MIN( mcu_height, context->info.height - ver_mcu_index * mcu_height );
    for( uint16_t y = 0; y < remain_height; y++ )
    {
        uint16_t hor_bytes = context->info.component_num * remain_width;
        uint16_t src_index = y * context->info.component_num * mcu_width;
        uint16_t dst_index = context->info.component_num * ( y * context->info.width + mcu_width * hor_mcu_index );
        for( uint16_t x = 0; x < hor_bytes; x++ )
        {
            context->line_buffer[ dst_index ] = (uint8_t)MAMEJPEG_CLIP( context->info.mcu_pixels[ src_index ], 0.0, 255.0 );
            context->info.mcu_pixels[ src_index ] = 0.0;
            src_index++;
            dst_index++;
        }
    }

    for( uint16_t y = 0; y < mcu_height; y++ )
    {
        uint16_t hor_bytes = context->info.component_num * mcu_width;
        uint16_t src_index = y * hor_bytes;
        for( uint16_t x = 0; x < hor_bytes; x++ )
        {
            context->info.mcu_pixels[ src_index ] = 0.0;
            src_index++;
        }
    }

    return true;
}

static
bool mameJpeg_decodeMCU( mameJpeg_context* context, uint16_t hor_mcu_index, uint16_t ver_mcu_index )
{
    MAMEJPEG_NULL_CHECK( context );

    for( uint8_t i = 0; i < context->info.component_num; i++ )
    {
        uint16_t num_of_hor_8x8blocks = context->info.component[ i ].hor_sampling;
        uint16_t num_of_ver_8x8blocks = context->info.component[ i ].ver_sampling;
        for( uint16_t ver_8x8block_index = 0; ver_8x8block_index < num_of_ver_8x8blocks; ver_8x8block_index++ )
        {
            for( uint16_t hor_8x8block_index = 0; hor_8x8block_index < num_of_hor_8x8blocks; hor_8x8block_index++ )
            {
                MAMEJPEG_CHECK( mameJpeg_decodeDC( context, i ) );
                MAMEJPEG_CHECK( mameJpeg_decodeAC( context, i ) );
                MAMEJPEG_CHECK( mameJpeg_applyInverseQunatize( context, i ) );
                MAMEJPEG_CHECK( mameJpeg_applyDCT( context, false ) );
                MAMEJPEG_CHECK( mameJpeg_applyColorConvert( context, hor_8x8block_index, ver_8x8block_index, i ) );
            }
        }
    }

    MAMEJPEG_CHECK( mameJpeg_moveMCUToBuffer( context, hor_mcu_index, ver_mcu_index ) );

    return true;
}

static
bool mameJpeg_decodeImage( mameJpeg_context* context )
{
    MAMEJPEG_NULL_CHECK( context );

    size_t dct_buffer_size = 64 * sizeof( mameJpeg_dct_element );
    MAMEJPEG_CHECK( mameJpeg_assignBuffer( context, dct_buffer_size, (void**)&context->info.dct_work_buffer ) );

    size_t mcu_buffer_size = mameJpeg_getMCUBufferSize( context->info.width,
            context->info.component_num,
            context->info.component[0].hor_sampling,
            context->info.component[0].ver_sampling );
    MAMEJPEG_CHECK( mameJpeg_assignBuffer( context, mcu_buffer_size, (void**)&context->info.mcu_pixels ) );

    context->line_buffer_length = mameJpeg_getLineBufferSize( context->info.width,
            context->info.component_num,
            context->info.component[0].ver_sampling );
    MAMEJPEG_CHECK( mameJpeg_assignBuffer( context, context->line_buffer_length, (void**)&context->line_buffer ) );

    bool useDRI = 0 < context->info.restart_interval;
    uint16_t restart_interval = context->info.restart_interval;

    uint16_t hor_mcu_num = 0;
    uint16_t ver_mcu_num = 0;
    MAMEJPEG_CHECK( mameJpeg_getNumOfMCU( context, &hor_mcu_num, &ver_mcu_num ) );

    for( uint16_t ver_mcu_index = 0; ver_mcu_index < ver_mcu_num; ver_mcu_index++ )
    {
        for( uint16_t hor_mcu_index = 0; hor_mcu_index < hor_mcu_num; hor_mcu_index++)
        {
            MAMEJPEG_CHECK( mameJpeg_decodeMCU( context, hor_mcu_index, ver_mcu_index ) );

            if( useDRI )
            {
                restart_interval--;
                if( restart_interval == 0 )
                {
                    MAMEJPEG_CHECK( mameJpeg_restartDecode( context ) );
                    restart_interval = context->info.restart_interval;
                }
            }
        }

        MAMEJPEG_CHECK( mameJpeg_writeBuffer( context ) );
    }

    /* cache invalidate */
    context->input_stream->cache = 0;
    context->input_stream->cache_use_bits = 0;

    return true;
}

static
bool mameJpeg_decodeSOISegment( mameJpeg_context* context )
{
    MAMEJPEG_NULL_CHECK( context );
    return true;
}

static
bool mameJpeg_decodeDQTSegment( mameJpeg_context* context )
{
    MAMEJPEG_NULL_CHECK( context );

    uint16_t dqt_size;
    MAMEJPEG_CHECK( mameJpeg_getSegmentSize( context, &dqt_size ) );
    MAMEJPEG_CHECK( ( dqt_size % ( 64 + 1 ) ) == 0 );

    uint8_t remain_bytes = dqt_size;
    while( 0 < remain_bytes )
    {
        uint8_t info;
        MAMEJPEG_CHECK( mameJpeg_stream_readByte( context->input_stream, &info ) );
        MAMEJPEG_CHECK( ( info >> 4 ) == 0 );

        uint8_t table_no = info & 0x0f;
        MAMEJPEG_CHECK( mameJpeg_assignBuffer( context, 64, (void**)&(context->info.quant_table[table_no]) ) );
        for( int i = 0; i < 64; i++ )
        {
            uint8_t* quant_table_ptr = &( context->info.quant_table[table_no][i] );
            MAMEJPEG_CHECK( mameJpeg_stream_readByte( context->input_stream, quant_table_ptr ) );
        }

        remain_bytes -= 65;
    }

    return true;
}

static
bool mameJpeg_decodeSOF0Segment( mameJpeg_context* context )
{
    MAMEJPEG_NULL_CHECK( context );

    uint16_t sof0_size;
    MAMEJPEG_CHECK( mameJpeg_getSegmentSize( context, &sof0_size ) );
    MAMEJPEG_CHECK( ( ( sof0_size - 6 ) % 3 ) == 0 );

    MAMEJPEG_CHECK( mameJpeg_stream_readByte( context->input_stream, &(context->info.accuracy) ) );
    MAMEJPEG_CHECK( mameJpeg_stream_readTwoBytes( context->input_stream, &(context->info.height) ) );
    MAMEJPEG_CHECK( mameJpeg_stream_readTwoBytes( context->input_stream, &(context->info.width) ) );
    MAMEJPEG_CHECK( mameJpeg_stream_readByte( context->input_stream, &(context->info.component_num) ) );
    MAMEJPEG_CHECK( context->info.component_num < 4 );

    for( int i = 0; i < context->info.component_num; i++ )
    {
        uint8_t component_index;
        MAMEJPEG_CHECK( mameJpeg_stream_readByte( context->input_stream, &component_index ) );
        component_index--; 
        MAMEJPEG_CHECK( component_index < 3 );

        uint8_t sampling;
        MAMEJPEG_CHECK( mameJpeg_stream_readByte( context->input_stream, &sampling ) );
        context->info.component[ component_index ].hor_sampling = sampling >> 4;
        context->info.component[ component_index ].ver_sampling = sampling & 0xf;

        uint8_t quant_table_index;
        MAMEJPEG_CHECK( mameJpeg_stream_readByte( context->input_stream, &quant_table_index ) );
        context->info.component[ component_index ].quant_table_index = quant_table_index;
    }

    return true;
}

static
bool mameJpeg_decodeDHTSegment( mameJpeg_context* context )
{
    MAMEJPEG_NULL_CHECK( context );

    uint16_t dht_size;
    MAMEJPEG_CHECK( mameJpeg_getSegmentSize( context, &dht_size ) );
    MAMEJPEG_CHECK( 17 <= dht_size );

    uint8_t info;
    MAMEJPEG_CHECK( mameJpeg_stream_readByte( context->input_stream, &info ) );
    uint8_t ac_dc = info >> 4;
    uint8_t luma_chroma = info & 0x0f;

    size_t huffman_table_buffer_size = ( dht_size - 17 ) * sizeof(huffman_element);
    void** huffman_code_table_ptr = (void**)&context->info.huff_table[ac_dc][luma_chroma].elements;
    MAMEJPEG_CHECK( mameJpeg_assignBuffer( context, huffman_table_buffer_size, huffman_code_table_ptr ) );

    uint16_t code = 0;
    uint16_t total_codes = 0;
    for( int i = 0; i <  16; i++ )
    {
        uint8_t num_of_codes;
        MAMEJPEG_CHECK( mameJpeg_stream_readByte( context->input_stream, &num_of_codes ) );
        context->info.huff_table[ac_dc][luma_chroma].offsets[i] = total_codes;
        for( int j = 0; j < num_of_codes; j++ )
        {
            if( context->info.huff_table[ac_dc][luma_chroma].elements != NULL )
            {
                context->info.huff_table[ac_dc][luma_chroma].elements[ total_codes + j ].code = code;
            }
            code++;
        }

        code <<= 1;
        total_codes += num_of_codes;
    }
    context->info.huff_table[ac_dc][luma_chroma].offsets[16] = total_codes;
    MAMEJPEG_CHECK( 17 + total_codes == dht_size );

    for( int i = 0; i < total_codes; i++ )
    {
        uint8_t zero_length;
        MAMEJPEG_CHECK( mameJpeg_stream_readByte( context->input_stream, &zero_length ) );
        if( context->info.huff_table[ac_dc][luma_chroma].elements != NULL )
        {
            context->info.huff_table[ac_dc][luma_chroma].elements[ i ].value = zero_length;
        }
    }

    return true;
}

static
bool mameJpeg_decodeSOSSegment( mameJpeg_context* context )
{
    MAMEJPEG_NULL_CHECK( context );

    uint16_t sos_size;
    MAMEJPEG_CHECK( mameJpeg_getSegmentSize( context, &sos_size ) );

    uint8_t num_of_components;
    MAMEJPEG_CHECK( mameJpeg_stream_readByte( context->input_stream, &num_of_components ) );

    for( int i = 0; i < num_of_components; i++ )
    {
        uint8_t component_index;
        MAMEJPEG_CHECK( mameJpeg_stream_readByte( context->input_stream, &component_index ) );
        uint8_t info;
        MAMEJPEG_CHECK( mameJpeg_stream_readByte( context->input_stream, &info ) );
        context->info.component[component_index - 1].huff_table_index[0] = info & 0xf;
        context->info.component[component_index - 1].huff_table_index[1] = info >> 4;
    }

    uint8_t dummy;
    MAMEJPEG_CHECK( mameJpeg_stream_readByte( context->input_stream, &dummy ) );
    MAMEJPEG_CHECK( dummy == 0 );
    MAMEJPEG_CHECK( mameJpeg_stream_readByte( context->input_stream, &dummy ) );
    MAMEJPEG_CHECK( dummy == 63 );
    MAMEJPEG_CHECK( mameJpeg_stream_readByte( context->input_stream, &dummy ) );
    MAMEJPEG_CHECK( dummy == 0 );

    MAMEJPEG_CHECK( mameJpeg_decodeImage( context ) );
    return true;
}

static
bool mameJpeg_decodeEOISegment( mameJpeg_context* context )
{
    MAMEJPEG_NULL_CHECK( context );
    return true;
}

static
bool mameJpeg_decodeDRISegment( mameJpeg_context* context )
{
    MAMEJPEG_NULL_CHECK( context );

    uint16_t dri_size;
    MAMEJPEG_CHECK( mameJpeg_getSegmentSize( context, &dri_size ) );
    MAMEJPEG_CHECK( dri_size == 2 );

    MAMEJPEG_CHECK( mameJpeg_stream_readTwoBytes( context->input_stream, &context->info.restart_interval ) );

    return true;
}

static
bool mameJpeg_decodeUnknownSegment( mameJpeg_context* context )
{
    MAMEJPEG_NULL_CHECK( context );
    return true;
}

typedef bool (*mameJpeg_decodeSegmentFuncPtr)(mameJpeg_context*);

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

bool mameJpeg_initializeDecode( mameJpeg_context* context,
        mameJpeg_stream_read_callback_ptr input_callback,
        void* input_callback_param,
        mameJpeg_stream_write_callback_ptr output_callback,
        void* output_callback_param,
        uint8_t* work_buffer,
        size_t work_buffer_size )
{
    MAMEJPEG_NULL_CHECK( context );
    MAMEJPEG_NULL_CHECK( input_callback );

    memset( context, 0x00, sizeof( mameJpeg_context ));
    mameJpeg_stream_input_initialize( context->input_stream, input_callback, input_callback_param );
    mameJpeg_stream_output_initialize( context->output_stream, output_callback, output_callback_param );
    context->mode = MAMEJPEG_MODE_DECODE;

    context->work_buffer_beg = work_buffer;
    context->work_buffer_ptr = work_buffer;
    context->work_buffer_end = work_buffer + work_buffer_size;

    return true;
}

bool mameJpeg_getDecodeBufferSize( mameJpeg_stream_read_callback_ptr read_callback_ptr,
        void* read_callback_param,
        uint16_t* width_ptr,
        uint16_t* height_ptr,
        uint8_t* component_num_ptr,
        size_t* decode_buffer_size_ptr )
{
    MAMEJPEG_NULL_CHECK( read_callback_ptr );
    MAMEJPEG_NULL_CHECK( read_callback_param );
    MAMEJPEG_NULL_CHECK( decode_buffer_size_ptr );

    mameJpeg_context context[1];
    mameJpeg_initializeDecode( context,
            read_callback_ptr,
            read_callback_param,
            NULL,
            NULL,
            NULL,
            0 );

    size_t buffer_size = 0;
    mameJpeg_marker marker;
    while( mameJpeg_getNextMarker( context, &marker ) )
    {
        if( marker == MAMEJPEG_MARKER_SOF0 )
        {
            MAMEJPEG_CHECK( mameJpeg_decodeSOF0Segment( context ) );
        }
        else if( marker == MAMEJPEG_MARKER_DHT )
        {
            uint16_t dht_size;
            MAMEJPEG_CHECK( mameJpeg_getSegmentSize( context, &dht_size ) );
            buffer_size += ( dht_size - 17 ) * sizeof( huffman_element );

            uint8_t dummy;
            for( uint16_t i = 0; i < dht_size; i++ )
            {
                MAMEJPEG_CHECK( mameJpeg_stream_readByte( context->input_stream, &dummy ) );
            }
        }
        else if( marker == MAMEJPEG_MARKER_DQT )
        {
            uint16_t dqt_size;
            MAMEJPEG_CHECK( mameJpeg_getSegmentSize( context, &dqt_size ) );
            buffer_size += 64 * dqt_size / 65;


            uint8_t dummy;
            for( uint16_t i = 0; i < dqt_size; i++ )
            {
                MAMEJPEG_CHECK( mameJpeg_stream_readByte( context->input_stream, &dummy ) );
            }
        }
        else if( marker == MAMEJPEG_MARKER_SOS )
        {
            break;
        }
    }

    size_t dct_buffer_size = 64 * sizeof( mameJpeg_dct_element );
    size_t mcu_buffer_size = mameJpeg_getMCUBufferSize( context->info.width,
            context->info.component_num,
            context->info.component[0].hor_sampling,
            context->info.component[0].ver_sampling );
    size_t line_buffer_size = mameJpeg_getLineBufferSize( context->info.width,
            context->info.component_num,
            context->info.component[0].ver_sampling );
    buffer_size += dct_buffer_size + mcu_buffer_size + line_buffer_size;

    if( width_ptr != NULL )
    {
        *width_ptr = context->info.width;
    }

    if( height_ptr != NULL )
    {
        *height_ptr = context->info.height;
    }

    if( component_num_ptr != NULL )
    {
        *component_num_ptr = context->info.component_num;
    }

    if( decode_buffer_size_ptr != NULL )
    {
        *decode_buffer_size_ptr = buffer_size;
    }

    return true;
}

bool mameJpeg_decode( mameJpeg_context* context )
{
    MAMEJPEG_NULL_CHECK( context );
    MAMEJPEG_CHECK( context->mode == MAMEJPEG_MODE_DECODE );

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

static const double MAMEJPEG_RGB_TO_YCBCR_COEFF[4][3] = { 
    { 0.299, -0.169,  0.500 },
    { 0.587, -0.331, -0.419 },
    { 0.114,  0.500, -0.081 },
    { -128 ,      0,      0},

};

static const uint8_t MAMEJPEG_DEFAULT_QUANT_TABLE[2][64] = {
    {
        /*
           16,  11,  10,  16,  24,  40,  51,  61,
           12,  12,  14,  19,  26,  58,  60,  55,
           14,  13,  16,  24,  40,  57,  69,  56,
           14,  17,  22,  29,  51,  87,  80,  62,
           18,  22,  37,  56,  68, 109, 103,  77,
           24,  35,  55,  64,  81, 104, 113,  92,
           49,  64,  78,  87, 103, 121, 120, 101,
           72,  92,  95,  98, 112, 100, 103,  99
           */
        1,  1, 1, 1, 1, 1, 1, 1,
        1,  1, 1, 1, 1, 1, 1, 1,
        1,  1, 1, 1, 1, 1, 1, 1,
        1,  1, 1, 1, 1, 1, 1, 1,
        1,  1, 1, 1, 1, 1, 1, 1,
        1,  1, 1, 1, 1, 1, 1, 1,
        1,  1, 1, 1, 1, 1, 1, 1,
        1,  1, 1, 1, 1, 1, 1, 1
    },
    {
        /*
           17, 18, 24, 47, 99, 99, 99, 99,
           18, 21, 26, 66, 99, 99, 99, 99,
           24, 26, 56, 99, 99, 99, 99, 99,
           47, 66, 99, 99, 99, 99, 99, 99,
           99, 99, 99, 99, 99, 99, 99, 99,
           99, 99, 99, 99, 99, 99, 99, 99,
           99, 99, 99, 99, 99, 99, 99, 99,
           99, 99, 99, 99, 99, 99, 99, 99
           */
        1,  1, 1, 1, 1, 1, 1, 1,
        1,  1, 1, 1, 1, 1, 1, 1,
        1,  1, 1, 1, 1, 1, 1, 1,
        1,  1, 1, 1, 1, 1, 1, 1,
        1,  1, 1, 1, 1, 1, 1, 1,
        1,  1, 1, 1, 1, 1, 1, 1,
        1,  1, 1, 1, 1, 1, 1, 1,
        1,  1, 1, 1, 1, 1, 1, 1
    }
};

static const uint8_t MAMEJPEG_DEFAULT_HUFFMAN_CODE_BITS[2][2][16] = {
    {
        { 0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0 },
    },
    {
        { 0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7d },
        { 0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 0x77 },
    }
};

static const uint8_t MAMEJPEG_DEFAULT_DC_LUMA_HUFFMAN_CODE_VALUE[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11
};

static const uint8_t MAMEJPEG_DEFAULT_DC_CHROMA_HUFFMAN_CODE_VALUE[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11
};

static const uint8_t MAMEJPEG_DEFAULT_AC_LUMA_HUFFMAN_CODE_VALUE[] = {
    0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12,
    0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
    0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08,
    0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0,
    0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16,
    0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
    0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
    0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
    0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
    0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
    0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
    0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
    0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
    0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
    0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5,
    0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4,
    0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
    0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea,
    0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
    0xf9, 0xfa
};

static const uint8_t MAMEJPEG_DEFAULT_AC_CHROMA_HUFFMAN_CODE_VALUE[] = {
    0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21,
    0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
    0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91,
    0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0,
    0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34,
    0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26,
    0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38,
    0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
    0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
    0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
    0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
    0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
    0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96,
    0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5,
    0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4,
    0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3,
    0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2,
    0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
    0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9,
    0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
    0xf9, 0xfa
};

static const uint8_t* MAMEJPEG_DEFAULT_HUFFMAN_CODE_VALUE[2][2] = {
    {
        MAMEJPEG_DEFAULT_DC_LUMA_HUFFMAN_CODE_VALUE,
        MAMEJPEG_DEFAULT_DC_CHROMA_HUFFMAN_CODE_VALUE
    },
    {
        MAMEJPEG_DEFAULT_AC_LUMA_HUFFMAN_CODE_VALUE,
        MAMEJPEG_DEFAULT_AC_CHROMA_HUFFMAN_CODE_VALUE
    }
};

static
bool mameJpeg_restartEncode( mameJpeg_context* context )
{
    MAMEJPEG_NULL_CHECK( context );

    context->info.component[0].prev_dc_value = 0;
    context->info.component[1].prev_dc_value = 0;
    context->info.component[2].prev_dc_value = 0;
    return true;
}

static
bool mameJpeg_getInputBytes( mameJpeg_context* context, uint16_t* output_bytes )
{
    MAMEJPEG_NULL_CHECK( context );
    MAMEJPEG_NULL_CHECK( output_bytes );

    *output_bytes = context->info.component_num * 8 * context->info.component[0].ver_sampling * context->info.width;
    return true;
}

static
bool mameJpeg_readIntoBuffer( mameJpeg_context* context )
{
    uint16_t output_bytes = 0;
    MAMEJPEG_CHECK( mameJpeg_getInputBytes( context, &output_bytes ) );

    for( uint16_t i = 0; i < output_bytes; i++ )
    {
        mameJpeg_stream_readByte( context->input_stream, &(context->line_buffer[i]) );
    }

    return true;
}

static
bool mameJpeg_applyEncodeColorConvert( mameJpeg_context* context, uint16_t hor_block, uint16_t ver_block, uint8_t component_index )
{
    MAMEJPEG_NULL_CHECK( context );

    int diff_hor_sampling = context->info.component[0].hor_sampling - context->info.component[ component_index ].hor_sampling;
    int diff_ver_sampling = context->info.component[0].ver_sampling - context->info.component[ component_index ].ver_sampling;
    int block_width = 8 * ( diff_hor_sampling + 1 );
    int block_height = 8 * ( diff_ver_sampling + 1 );

    int y_offset = 8 * ver_block;
    int x_offset = 8 * hor_block;
    int mcu_width = 8 * context->info.component[0].hor_sampling;

    for( int i = 0; i < 64; i++ )
    {
        context->info.dct_work_buffer[i] = 0.0;
    }

    for( int y = 0; y < block_height; y += ( 1 + diff_ver_sampling ) )
    {
        uint16_t src_index = context->info.component_num * ( ( y + y_offset ) * mcu_width + x_offset );
        for( int x = 0; x < block_width; x++ )
        {
            uint16_t dst_index = 8 * ( y >> diff_ver_sampling ) + ( x >> diff_hor_sampling );
            for( int i = 0; i < context->info.component_num; i++ )
            {
                if( context->info.component_num == 1 )
                {
                    context->info.dct_work_buffer[ dst_index ] += context->info.mcu_pixels[ src_index ];
                }
                else
                {
                    context->info.dct_work_buffer[ dst_index ] += MAMEJPEG_RGB_TO_YCBCR_COEFF[i][component_index] * ( context->info.mcu_pixels[ src_index ] ) ;
                }
                src_index++;
            }
            context->info.dct_work_buffer[ dst_index ] += MAMEJPEG_RGB_TO_YCBCR_COEFF[3][component_index];
        }
    }

    return true;
}

static
bool mameJpeg_applyQuantize( mameJpeg_context* context, uint8_t component_index  )
{
    MAMEJPEG_NULL_CHECK( context );

    uint8_t table_index = context->info.component[ component_index ].quant_table_index;
    for( int i = 0; i < 64; i++ )
    {
        uint8_t zigzag_index = mameJpeg_getZigZagIndex( context, i );
        context->info.dct_work_buffer[zigzag_index] /= context->info.quant_table[table_index][i];
    }

    return true;
}

static
bool mameJpeg_encodeHuffmanCode( mameJpeg_context* context, uint8_t ac_dc, uint8_t table_index, uint16_t value, uint8_t code_length, uint16_t* code )
{
    MAMEJPEG_NULL_CHECK( context );
    MAMEJPEG_CHECK( 0 < code_length );

    uint16_t from_index = ( code_length == 0 ) ? 0 : context->info.huff_table[ac_dc][table_index].offsets[ code_length - 1 ];
    uint16_t to_index = context->info.huff_table[ac_dc][table_index].offsets[ code_length ];
    for( uint16_t i = from_index; i < to_index; i++ )
    {
        if( context->info.huff_table[ac_dc][table_index].elements[i].value == value )
        {
            *code = context->info.huff_table[ac_dc][table_index].elements[i].code;
            return true;
        }
    }

    return false;
}

static
bool mameJpeg_putNextEncodedValue( mameJpeg_context* context, uint8_t ac_dc, uint8_t component_index, uint16_t raw_value )
{
    MAMEJPEG_NULL_CHECK( context );

    for( uint8_t code_length = 1; code_length <= 16; code_length++ )
    {
        uint16_t code = 0;
        bool ret = mameJpeg_encodeHuffmanCode( context, ac_dc, context->info.component[ component_index ].huff_table_index[ac_dc], raw_value, code_length, &code );
        if( ret )
        {
            MAMEJPEG_CHECK( mameJpeg_stream_writeBits( context->output_stream, &code, code_length ) );
            return true;
        }
    }

    return false;
}

static
bool mameJpeg_calcEncodeDCValue( mameJpeg_context* context, int16_t diff_value, uint16_t* dc_value,  uint8_t* length )
{
    MAMEJPEG_NULL_CHECK( context );
    MAMEJPEG_NULL_CHECK( dc_value );
    MAMEJPEG_NULL_CHECK( length );

    *length = 0;
    int16_t abs_value = abs( diff_value );
    while( 0 < abs_value )
    {
        (*length)++;
        abs_value >>= 1;
    }

    int base_value = ( 0 < diff_value ) ? 0 : ( ( 1 << *length ) - 1 );
    *dc_value = diff_value + base_value;
    return true;
}

static
bool mameJpeg_encodeDC( mameJpeg_context* context, uint8_t component_index )
{
    MAMEJPEG_NULL_CHECK( context );

    int16_t diff_value = context->info.dct_work_buffer[0] - context->info.component[ component_index ].prev_dc_value ;
    context->info.component[ component_index ].prev_dc_value += diff_value;

    uint16_t huffman_dc_value;
    uint8_t length;
    MAMEJPEG_CHECK( mameJpeg_calcEncodeDCValue( context, diff_value, &huffman_dc_value, &length ) );

    MAMEJPEG_CHECK( mameJpeg_putNextEncodedValue( context, 0, component_index, length ) );
    MAMEJPEG_CHECK( mameJpeg_stream_writeBits( context->output_stream, &huffman_dc_value, length ) );

    return true;
}

static
bool mameJpeg_encodeAC( mameJpeg_context* context, uint8_t component_index )
{
    uint8_t elem_num = 1;
    while( elem_num < 64 )
    {
        uint8_t zero_run_length = 0;
        uint8_t zigzag_index = mameJpeg_getZigZagIndex( context, elem_num );
        int16_t current_value = (int16_t)context->info.dct_work_buffer[zigzag_index];
        while( current_value == 0 && elem_num < 64 )
        {
            zero_run_length++;
            elem_num++;
            zigzag_index = mameJpeg_getZigZagIndex( context, elem_num );
            current_value = context->info.dct_work_buffer[zigzag_index];
        }

        uint8_t bit_length = 0;
        if( elem_num == 64 &&  0 < zero_run_length )
        {
            zero_run_length = 0;
            bit_length = 0;

            uint8_t value = ( zero_run_length << 4 ) | bit_length;
            MAMEJPEG_CHECK( mameJpeg_putNextEncodedValue( context, 1, component_index, value ) );
            break;
        }
        else
        {
            while( 16 <= zero_run_length )
            {
                uint8_t value = ( 15 << 4 ) | 0;
                MAMEJPEG_CHECK( mameJpeg_putNextEncodedValue( context, 1, component_index, value ) );

                zero_run_length -= 16;
            }
        }

        int16_t raw_value = current_value;
        uint16_t huff_value = 0;
        MAMEJPEG_CHECK( mameJpeg_calcEncodeDCValue( context, raw_value, &huff_value, &bit_length ) );

        uint8_t value = ( zero_run_length << 4 ) | bit_length;
        MAMEJPEG_CHECK( mameJpeg_putNextEncodedValue( context, 1, component_index, value ) );

        MAMEJPEG_CHECK( mameJpeg_stream_writeBits( context->output_stream, &huff_value, bit_length ) );
        elem_num++;
    }
    return true;
}

static
bool mameJpeg_moveBufferToMCU( mameJpeg_context* context, uint16_t hor_mcu_index, uint16_t ver_mcu_index )
{
    MAMEJPEG_CHECK( context->info.component_num == 1 || context->info.component_num == 3 );

    uint16_t mcu_width = 8 * context->info.component[0].hor_sampling;
    uint16_t mcu_height = 8 * context->info.component[0].ver_sampling;
    for( uint16_t y = 0; y < mcu_height; y++ )
    {
        uint16_t hor_bytes = context->info.component_num * mcu_width;
        uint16_t src_index = context->info.component_num * ( y * context->info.width + mcu_width * hor_mcu_index );
        uint16_t dst_index = y * hor_bytes;
        for( uint16_t x = 0; x < hor_bytes; x++ )
        {
            context->info.mcu_pixels[ dst_index ] = context->line_buffer[ src_index ];

            src_index++;
            dst_index++;
        }
    }

    return true;
}

static
bool mameJpeg_encodeMCU( mameJpeg_context* context, uint16_t hor_mcu_index, uint16_t ver_mcu_index )
{
    MAMEJPEG_NULL_CHECK( context );

    MAMEJPEG_CHECK( mameJpeg_moveBufferToMCU( context, hor_mcu_index, ver_mcu_index ) );

    for( uint8_t i = 0; i < context->info.component_num; i++ )
    {
        uint16_t num_of_hor_8x8blocks = context->info.component[ i ].hor_sampling;
        uint16_t num_of_ver_8x8blocks = context->info.component[ i ].ver_sampling;
        for( uint16_t ver_8x8block_index = 0; ver_8x8block_index < num_of_ver_8x8blocks; ver_8x8block_index++ )
        {
            for( uint16_t hor_8x8block_index = 0; hor_8x8block_index < num_of_hor_8x8blocks; hor_8x8block_index++ )
            {
                MAMEJPEG_CHECK( mameJpeg_applyEncodeColorConvert( context, hor_8x8block_index, ver_8x8block_index, i ) );
                MAMEJPEG_CHECK( mameJpeg_applyDCT( context, true ) );
                MAMEJPEG_CHECK( mameJpeg_applyQuantize( context, i ) );
                MAMEJPEG_CHECK( mameJpeg_encodeDC( context, i ) );
                MAMEJPEG_CHECK( mameJpeg_encodeAC( context, i ) );
            }
        }
    }

    return true;
}

static
bool mameJpeg_encodeImage( mameJpeg_context* context )
{
    MAMEJPEG_NULL_CHECK( context );

    size_t dct_buffer_size = 64 * sizeof( mameJpeg_dct_element );
    MAMEJPEG_CHECK( mameJpeg_assignBuffer( context, dct_buffer_size, (void**)&context->info.dct_work_buffer ) );

    size_t mcu_buffer_size = mameJpeg_getMCUBufferSize( context->info.width,
            context->info.component_num,
            context->info.component[0].hor_sampling,
            context->info.component[0].ver_sampling );
    MAMEJPEG_CHECK( mameJpeg_assignBuffer( context, mcu_buffer_size, (void**)&context->info.mcu_pixels ) );

    context->line_buffer_length = mameJpeg_getLineBufferSize( context->info.width,
            context->info.component_num,
            context->info.component[0].ver_sampling );
    MAMEJPEG_CHECK( mameJpeg_assignBuffer( context, context->line_buffer_length, (void**)&context->line_buffer ) );

    bool useDRI = 0 < context->info.restart_interval;
    uint16_t restart_interval = context->info.restart_interval;

    uint16_t hor_mcu_num = 0;
    uint16_t ver_mcu_num = 0;
    MAMEJPEG_CHECK( mameJpeg_getNumOfMCU( context, &hor_mcu_num, &ver_mcu_num ) );

    for( uint16_t ver_mcu_index = 0; ver_mcu_index < ver_mcu_num; ver_mcu_index++ )
    {
        MAMEJPEG_CHECK( mameJpeg_readIntoBuffer( context ) );
        for( uint16_t hor_mcu_index = 0; hor_mcu_index < hor_mcu_num; hor_mcu_index++)
        {
            MAMEJPEG_CHECK( mameJpeg_encodeMCU( context, hor_mcu_index, ver_mcu_index ) );

            if( useDRI )
            {
                restart_interval--;
                if( restart_interval == 0 )
                {
                    MAMEJPEG_CHECK( mameJpeg_restartEncode( context ) );
                    restart_interval = context->info.restart_interval;
                }
            }
        }
    }

    mameJpeg_stream_flushBits( context->output_stream );
    return true;
}

static
bool mameJpeg_encodeSOISegment( mameJpeg_context* context )
{
    MAMEJPEG_NULL_CHECK( context );

    MAMEJPEG_CHECK( mameJpeg_stream_writeTwoBytes( context->output_stream, MAMEJPEG_MARKER_SOI ) );

    return true;
}

static
bool mameJpeg_encodeDQTSegment( mameJpeg_context* context )
{
    MAMEJPEG_NULL_CHECK( context );

    uint8_t max_luma_chroma = ( context->info.component_num == 1 ) ? 1 : 2;
    for( uint8_t luma_chroma = 0; luma_chroma < max_luma_chroma; luma_chroma++ )
    {
        context->info.quant_table[luma_chroma] = (uint8_t*)MAMEJPEG_DEFAULT_QUANT_TABLE[luma_chroma];
    }

    MAMEJPEG_CHECK( mameJpeg_stream_writeTwoBytes( context->output_stream, MAMEJPEG_MARKER_DQT ) );

    uint8_t quant_table_num = 0;
    for( int i = 0; i < 4; i++ )
    {
        quant_table_num += ( context->info.quant_table[i] != NULL ) ? 1 : 0;
    }

    uint16_t dqt_size = quant_table_num * 65 + 2;
    MAMEJPEG_CHECK( mameJpeg_stream_writeTwoBytes( context->output_stream, dqt_size ) );

    for( uint8_t i = 0; i < quant_table_num; i++ )
    {
        MAMEJPEG_CHECK( mameJpeg_stream_writeByte( context->output_stream, i ) );
        for( uint8_t j = 0; j < 64; j++ )
        {
            MAMEJPEG_CHECK( mameJpeg_stream_writeByte( context->output_stream, context->info.quant_table[i][j] ) );
        }
    }

    return true;
}

static
bool mameJpeg_encodeSOF0Segment( mameJpeg_context* context )
{
    MAMEJPEG_NULL_CHECK( context );

    MAMEJPEG_CHECK( mameJpeg_stream_writeTwoBytes( context->output_stream, MAMEJPEG_MARKER_SOF0 ) );

    uint16_t sfo0_size = 6 + context->info.component_num * 3 + 2;
    MAMEJPEG_CHECK( mameJpeg_stream_writeTwoBytes( context->output_stream, sfo0_size ) );

    MAMEJPEG_CHECK( mameJpeg_stream_writeByte( context->output_stream, context->info.accuracy ) );
    MAMEJPEG_CHECK( mameJpeg_stream_writeTwoBytes( context->output_stream, context->info.height ) );
    MAMEJPEG_CHECK( mameJpeg_stream_writeTwoBytes( context->output_stream, context->info.width ) );
    MAMEJPEG_CHECK( mameJpeg_stream_writeByte( context->output_stream, context->info.component_num ) );

    for( uint8_t i = 0; i < context->info.component_num; i++ )
    {
        uint8_t component_index = i + 1;
        MAMEJPEG_CHECK( mameJpeg_stream_writeByte( context->output_stream, component_index ) );
        uint8_t sampling = ( context->info.component[i].hor_sampling << 4 ) | context->info.component[i].ver_sampling;
        MAMEJPEG_CHECK( mameJpeg_stream_writeByte( context->output_stream, sampling ) );
        MAMEJPEG_CHECK( mameJpeg_stream_writeByte( context->output_stream, context->info.component[i].quant_table_index ) );
    }

    return true;
}

static
bool mameJpeg_encodeDHTSegment( mameJpeg_context* context )
{
    MAMEJPEG_NULL_CHECK( context );

    uint8_t max_luma_chroma = ( context->info.component_num == 1 ) ? 1 : 2;
    for( uint8_t luma_chroma = 0; luma_chroma < max_luma_chroma; luma_chroma++ )
    {
        for( uint8_t ac_dc = 0; ac_dc < 2; ac_dc++ )
        {
            MAMEJPEG_CHECK( mameJpeg_stream_writeTwoBytes( context->output_stream, MAMEJPEG_MARKER_DHT ) );

            int code_num = 0;
            for( int i = 0; i < 16; i++ )
            {
                code_num += MAMEJPEG_DEFAULT_HUFFMAN_CODE_BITS[ac_dc][luma_chroma][i];
            }
            uint16_t dht_size = code_num + 16 + 1 + 2;
            MAMEJPEG_CHECK( mameJpeg_stream_writeTwoBytes( context->output_stream, dht_size ) );

            uint8_t huff_type = ( ac_dc << 4 ) | luma_chroma;
            MAMEJPEG_CHECK( mameJpeg_stream_writeByte( context->output_stream, huff_type ) );

            size_t huffman_table_buffer_size = sizeof( huffman_element ) * code_num;
            void** huffman_code_table_ptr = (void**)&context->info.huff_table[ac_dc][luma_chroma].elements;
            MAMEJPEG_CHECK( mameJpeg_assignBuffer( context, huffman_table_buffer_size, huffman_code_table_ptr ) );

            uint16_t code = 0;
            uint16_t total_codes = 0;
            for( int i = 0; i < 16; i++ )
            {
                uint16_t num_of_codes = MAMEJPEG_DEFAULT_HUFFMAN_CODE_BITS[ac_dc][luma_chroma][i];
                MAMEJPEG_CHECK( mameJpeg_stream_writeByte( context->output_stream, num_of_codes  ) );

                context->info.huff_table[ac_dc][luma_chroma].offsets[i] = total_codes;
                for( int j = 0; j < num_of_codes; j++ )
                {
                    uint16_t element_index = total_codes + j;
                    uint16_t value = MAMEJPEG_DEFAULT_HUFFMAN_CODE_VALUE[ac_dc][luma_chroma][element_index];
                    context->info.huff_table[ac_dc][luma_chroma].elements[ element_index ].code = code;
                    context->info.huff_table[ac_dc][luma_chroma].elements[ element_index ].value = value;
                    code++;
                }

                code <<= 1;
                total_codes += num_of_codes;
            }
            context->info.huff_table[ac_dc][luma_chroma].offsets[16] = total_codes;

            for( int i = 0; i < total_codes; i++ )
            {
                MAMEJPEG_CHECK( mameJpeg_stream_writeByte( context->output_stream, MAMEJPEG_DEFAULT_HUFFMAN_CODE_VALUE[ac_dc][luma_chroma][i] ) );
            }

        }

        context->info.quant_table[luma_chroma] = (uint8_t*)MAMEJPEG_DEFAULT_QUANT_TABLE[luma_chroma];
    }
    return true;
}

static
bool mameJpeg_encodeSOSSegment( mameJpeg_context* context )
{
    MAMEJPEG_NULL_CHECK( context );

    MAMEJPEG_CHECK( mameJpeg_stream_writeTwoBytes( context->output_stream, MAMEJPEG_MARKER_SOS ) );

    uint16_t sos_size = 4 + context->info.component_num * 2 + 2;
    MAMEJPEG_CHECK( mameJpeg_stream_writeTwoBytes( context->output_stream, sos_size ) );

    MAMEJPEG_CHECK( mameJpeg_stream_writeByte( context->output_stream, context->info.component_num ) );
    for( uint8_t i = 0; i < context->info.component_num; i++ )
    {
        uint8_t component_index = i + 1;
        MAMEJPEG_CHECK( mameJpeg_stream_writeByte( context->output_stream, component_index ) );

        uint8_t table_index = ( context->info.component[i].huff_table_index[1] << 4 ) | context->info.component[i].huff_table_index[0];
        MAMEJPEG_CHECK( mameJpeg_stream_writeByte( context->output_stream, table_index ) );
    }

    MAMEJPEG_CHECK( mameJpeg_stream_writeByte( context->output_stream, 0 ) );
    MAMEJPEG_CHECK( mameJpeg_stream_writeByte( context->output_stream, 63 ) );
    MAMEJPEG_CHECK( mameJpeg_stream_writeByte( context->output_stream, 0 ) );

    MAMEJPEG_CHECK( mameJpeg_encodeImage( context ) );
    return true;
}

static
bool mameJpeg_encodeEOISegment( mameJpeg_context* context )
{
    MAMEJPEG_NULL_CHECK( context );

    MAMEJPEG_CHECK( mameJpeg_stream_writeTwoBytes( context->output_stream, MAMEJPEG_MARKER_EOI ) );

    return true;
}

bool mameJpeg_getEncodeBufferSize( size_t width, size_t height, mameJpeg_format format, size_t* encode_buffer_size )
{
    MAMEJPEG_NULL_CHECK( encode_buffer_size );

    uint8_t component_num = mameJpeg_getComponentNum( format );
    uint8_t luma_hor_sampling = mameJpeg_getLumaHorSampling( format );
    uint8_t luma_ver_sampling = mameJpeg_getLumaVerSampling( format );

    size_t dct_buffer_size = 64 * sizeof( mameJpeg_dct_element );
    size_t mcu_buffer_size = mameJpeg_getMCUBufferSize( width,
            component_num,
            luma_hor_sampling,
            luma_ver_sampling );
    size_t line_buffer_size = mameJpeg_getLineBufferSize( width,
            component_num,
            luma_ver_sampling );

    uint16_t huffman_code_num = 0;
    uint8_t max_luma_chroma = ( component_num == 1 ) ? 1 : 2;
    for( uint8_t luma_chroma = 0; luma_chroma < max_luma_chroma; ++luma_chroma )
    {
        for( uint8_t ac_dc = 0; ac_dc < 2; ++ac_dc )
        {
            for( int i = 0; i < 16; i++ )
            {
                huffman_code_num += MAMEJPEG_DEFAULT_HUFFMAN_CODE_BITS[ac_dc][luma_chroma][i];
            }
        }
    }
    size_t huffman_table_size = sizeof( huffman_element ) * huffman_code_num;

    *encode_buffer_size = dct_buffer_size + mcu_buffer_size + line_buffer_size + huffman_table_size + 1;
    return true;
}

bool mameJpeg_initializeEncode( mameJpeg_context* context,
        mameJpeg_stream_read_callback_ptr input_callback,
        void* input_callback_param,
        mameJpeg_stream_write_callback_ptr output_callback,
        void* output_callback_param,
        size_t width,
        size_t height,
        mameJpeg_format format,
        uint8_t* work_buffer,
        size_t work_buffer_size
        )
{
    MAMEJPEG_NULL_CHECK( context );
    MAMEJPEG_NULL_CHECK( input_callback );
    MAMEJPEG_NULL_CHECK( input_callback_param );
    MAMEJPEG_NULL_CHECK( output_callback );
    MAMEJPEG_NULL_CHECK( output_callback_param );
    MAMEJPEG_NULL_CHECK( work_buffer );
    MAMEJPEG_CHECK( 0 < width );
    MAMEJPEG_CHECK( 0 < height );
    MAMEJPEG_CHECK( 0 < work_buffer_size );

    memset( context, 0x00, sizeof( mameJpeg_context ));
    mameJpeg_stream_input_initialize( context->input_stream, input_callback, input_callback_param );
    mameJpeg_stream_output_initialize( context->output_stream, output_callback, output_callback_param );
    context->mode = MAMEJPEG_MODE_ENCODE;

    context->work_buffer_beg = work_buffer;
    context->work_buffer_ptr = work_buffer;
    context->work_buffer_end = work_buffer + work_buffer_size;

    context->info.accuracy = 8;
    context->info.width = width;
    context->info.height = height;
    context->info.component_num = mameJpeg_getComponentNum( format );
    for( int i = 0; i < context->info.component_num; i++ )
    {
        context->info.component[i].hor_sampling = ( i == 0 ) ? mameJpeg_getLumaHorSampling( format ) : 1;
        context->info.component[i].ver_sampling = ( i == 0 ) ? mameJpeg_getLumaVerSampling( format ) : 1;
        context->info.component[i].quant_table_index = ( i == 0 ) ? 0 : 1;
        context->info.component[i].huff_table_index[0] = ( i == 0 ) ? 0 : 1;
        context->info.component[i].huff_table_index[1] = ( i == 0 ) ? 0 : 1;
        context->info.component[i].prev_dc_value = 0;
    }

    return true;
}

bool mameJpeg_encode( mameJpeg_context* context )
{
    MAMEJPEG_NULL_CHECK( context );
    MAMEJPEG_CHECK( context->mode == MAMEJPEG_MODE_ENCODE );

    MAMEJPEG_CHECK( mameJpeg_encodeSOISegment( context ) );
    MAMEJPEG_CHECK( mameJpeg_encodeDQTSegment( context ) );
    MAMEJPEG_CHECK( mameJpeg_encodeSOF0Segment( context ) );
    MAMEJPEG_CHECK( mameJpeg_encodeDHTSegment( context ) );
    MAMEJPEG_CHECK( mameJpeg_encodeSOSSegment( context ) );
    MAMEJPEG_CHECK( mameJpeg_encodeEOISegment( context ) );
    return true;
}

bool mameJpeg_input_from_memory_callback( void* param, uint8_t* byte )
{
    MAMEJPEG_NULL_CHECK( param );
    MAMEJPEG_NULL_CHECK( byte );

    mameJpeg_memory_callback_param *callback_param = (mameJpeg_memory_callback_param*)param;
    MAMEJPEG_NULL_CHECK( callback_param->buffer_ptr );
    MAMEJPEG_CHECK( callback_param->buffer_pos < callback_param->buffer_size );

    *byte = ((uint8_t*)callback_param->buffer_ptr)[ callback_param->buffer_pos ];
    callback_param->buffer_pos++;

    return true;
}

bool mameJpeg_output_to_memory_callback( void* param, uint8_t byte )
{
    MAMEJPEG_NULL_CHECK( param );

    mameJpeg_memory_callback_param *callback_param = (mameJpeg_memory_callback_param*)param;
    MAMEJPEG_NULL_CHECK( callback_param->buffer_ptr );
    MAMEJPEG_CHECK( callback_param->buffer_pos < callback_param->buffer_size );

    ((uint8_t*)callback_param->buffer_ptr)[ callback_param->buffer_pos ] = byte;
    callback_param->buffer_pos++;

    return true;
}

bool mameJpeg_input_from_file_callback( void* param, uint8_t* byte )
{
    MAMEJPEG_NULL_CHECK( param );
    MAMEJPEG_NULL_CHECK( byte );

    FILE* fp = (FILE*)param;
    size_t n = fread( byte, 1, 1, fp );

    return ( n == 1 );
}

bool mameJpeg_output_to_file_callback( void* param, uint8_t byte )
{
    MAMEJPEG_NULL_CHECK( param );

    FILE* fp = (FILE*)param;
    size_t n = fwrite( &byte, 1, 1, fp );

    return ( n == 1 );
}

# ifdef __cplusplus
}
# endif /* __cplusplus */

#endif /* __MAME_JPEG_HEADER__ */
