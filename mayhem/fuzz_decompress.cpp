// Raw MPQ sector decompression via SCompDecompress2 (zlib, bzip2, PKWARE, LZMA, sparse, and the
// huffman+ADPCM combinations). Bytes 0-1 are the size of the output buffer (little endian, +1); the rest is
// the compressed sector, whose first byte is the compression mask. Both buffers are exactly sized heap
// blocks so ASan sees every overrun.
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include "StormLib.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t * data, size_t size)
{
    if(size < 4)
        return -1;
    int cbOut = ((int)data[0] | ((int)data[1] << 8)) + 1;
    int cbIn = (int)(size - 2);

    uint8_t * in = (uint8_t *)malloc(cbIn);
    uint8_t * out = (uint8_t *)malloc(cbOut);
    if(in == NULL || out == NULL)
    {
        free(in);
        free(out);
        return -1;
    }
    memcpy(in, data + 2, cbIn);
    memset(out, 0, cbOut);

    int outLen = cbOut;
    int ok = SCompDecompress2(out, &outLen, in, cbIn);
    if(ok)
    {
        // The reported length must stay inside the buffer the caller supplied.
        if(outLen < 0 || outLen > cbOut)
            abort();
    }
    free(in);
    free(out);
    return ok ? 0 : -1;
}
