#include<stdio.h>

#include "../src/mameJpeg.h"

int main(int argc, char** argv)
{
    if( argc != 3 )
    {
        fprintf(stderr, "usage:decode_encode input.jpg output.jpg\n");
        exit( -1 );
    }

    char* input_file_path = argv[1];
    char* output_file_path = argv[2];

    FILE* input_fp = fopen( input_file_path, "r" );
    if( input_fp == NULL )
    {
        fprintf(stderr, "cannot open %s\n", input_file_path);
        exit( -1 );
    }

    uint16_t width;
    uint16_t height;
    uint8_t component_num;
    size_t decode_work_buffer_size;
    bool ret = mameJpeg_getDecodeBufferSize( mameJpeg_input_from_file_callback,
                                             input_fp,
                                             &width,
                                             &height,
                                             &component_num,
                                             &decode_work_buffer_size );
    if( !ret )
    {
        fprintf(stderr, "cannot read jpeg info\n");
        fclose( input_fp );
        exit( -1 );
    }
    rewind( input_fp );

    printf("width         :%d\n", width );
    printf("height        :%d\n", height );
    printf("components    :%d\n", component_num );
    printf("working buffer:%lu\n", decode_work_buffer_size );

    size_t decode_image_buffer_size = component_num * width * height;
    uint8_t decode_image_buffer[ decode_image_buffer_size ];
    mameJpeg_memory_callback_param output_param = {
        .buffer_ptr = decode_image_buffer,
        .buffer_pos = 0,
        .buffer_size = decode_image_buffer_size
    };

    uint8_t decode_work_buffer[ decode_work_buffer_size ];
    mameJpeg_context decode_context[1];
    ret = mameJpeg_initializeDecode( decode_context,
                                     mameJpeg_input_from_file_callback,
                                     input_fp,
                                     mameJpeg_output_to_memory_callback,
                                     &output_param,
                                     decode_work_buffer,
                                     decode_work_buffer_size );
    if( !ret )
    {
        fprintf(stderr, "cannot initialize for decoding\n");
        fclose( input_fp );
        exit( -1 );
    }

    ret = mameJpeg_decode( decode_context );
    fclose( input_fp );
    if( !ret )
    {
        fprintf(stderr, "cannot decode\n");
        exit( -1 );
    }

    FILE* output_fp = fopen( output_file_path, "w" );
    if( output_fp == NULL )
    {
        fprintf(stderr, "cannot open %s\n", output_file_path);
        fclose( output_fp );
        exit( -1 );
    }

    mameJpeg_memory_callback_param input_param = {
        .buffer_ptr = decode_image_buffer,
        .buffer_pos = 0,
        .buffer_size = decode_image_buffer_size
    };

    mameJpeg_format format = ( component_num == 1 ) ? MAMEJPEG_FORMAT_Y_444 : MAMEJPEG_FORMAT_YCBCR_444;
    size_t encode_work_buffer_size;
    mameJpeg_getEncodeBufferSize( width, height, format, &encode_work_buffer_size );

    uint8_t encode_work_buffer[ encode_work_buffer_size ];
    mameJpeg_context encode_context[1];
    mameJpeg_initializeEncode( encode_context,
            mameJpeg_input_from_memory_callback,
            &input_param,
            mameJpeg_output_to_file_callback,
            output_fp,
            width,
            height,
            format,
            encode_work_buffer,
            encode_work_buffer_size);

    mameJpeg_encode( encode_context );

    fclose( output_fp );
    return 0;
}
