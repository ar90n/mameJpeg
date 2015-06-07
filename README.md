What is mameJpeg ?
===============
mameJpeg is a header only library for encoding/decoding JPEG images.

Example
===============
```cpp
#include "mameJpeg.h"

int main( int argc, char** argv )
{
    const char* input_path = "input_path_to_your_env";
    const char* output_path = "output_path_to_your_env";
}
```

Features
===============
* only include mameJpeg.h.
* support encoding and decoding for YCBCR400, YCBCR420, YCBCR422 and YCBCR444.
* read and write data by input and output callback functions.
