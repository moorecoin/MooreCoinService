// lz4 streaming api example : ring buffer
// based on sample code from takayuki matsuoka


/**************************************
 * compiler options
 **************************************/
#ifdef _msc_ver    /* visual studio */
#  define _crt_secure_no_warnings // for msvc
#  define snprintf sprintf_s
#endif
#ifdef __gnuc__
#  pragma gcc diagnostic ignored "-wmissing-braces"   /* gcc bug 53119 : doesn't accept { 0 } as initializer (https://gcc.gnu.org/bugzilla/show_bug.cgi?id=53119) */
#endif


/**************************************
 * includes
 **************************************/
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "lz4.h"


enum {
    message_max_bytes   = 1024,
    ring_buffer_bytes   = 1024 * 8 + message_max_bytes,
    decode_ring_buffer  = ring_buffer_bytes + message_max_bytes   // intentionally larger, to test unsynchronized ring buffers
};


size_t write_int32(file* fp, int32_t i) {
    return fwrite(&i, sizeof(i), 1, fp);
}

size_t write_bin(file* fp, const void* array, int arraybytes) {
    return fwrite(array, 1, arraybytes, fp);
}

size_t read_int32(file* fp, int32_t* i) {
    return fread(i, sizeof(*i), 1, fp);
}

size_t read_bin(file* fp, void* array, int arraybytes) {
    return fread(array, 1, arraybytes, fp);
}


void test_compress(file* outfp, file* inpfp)
{
    lz4_stream_t lz4stream_body = { 0 };
    lz4_stream_t* lz4stream = &lz4stream_body;

    static char inpbuf[ring_buffer_bytes];
    int inpoffset = 0;

    for(;;) {
        // read random length ([1,message_max_bytes]) data to the ring buffer.
        char* const inpptr = &inpbuf[inpoffset];
        const int randomlength = (rand() % message_max_bytes) + 1;
        const int inpbytes = (int) read_bin(inpfp, inpptr, randomlength);
        if (0 == inpbytes) break;

        {
            char cmpbuf[lz4_compressbound(message_max_bytes)];
            const int cmpbytes = lz4_compress_continue(lz4stream, inpptr, cmpbuf, inpbytes);
            if(cmpbytes <= 0) break;
            write_int32(outfp, cmpbytes);
            write_bin(outfp, cmpbuf, cmpbytes);

            inpoffset += inpbytes;

            // wraparound the ringbuffer offset
            if(inpoffset >= ring_buffer_bytes - message_max_bytes) inpoffset = 0;
        }
    }

    write_int32(outfp, 0);
}


void test_decompress(file* outfp, file* inpfp)
{
    static char decbuf[decode_ring_buffer];
    int   decoffset    = 0;
    lz4_streamdecode_t lz4streamdecode_body = { 0 };
    lz4_streamdecode_t* lz4streamdecode = &lz4streamdecode_body;

    for(;;) {
        int cmpbytes = 0;
        char cmpbuf[lz4_compressbound(message_max_bytes)];

        {
            const size_t r0 = read_int32(inpfp, &cmpbytes);
            if(r0 != 1 || cmpbytes <= 0) break;

            const size_t r1 = read_bin(inpfp, cmpbuf, cmpbytes);
            if(r1 != (size_t) cmpbytes) break;
        }

        {
            char* const decptr = &decbuf[decoffset];
            const int decbytes = lz4_decompress_safe_continue(
                lz4streamdecode, cmpbuf, decptr, cmpbytes, message_max_bytes);
            if(decbytes <= 0) break;
            decoffset += decbytes;
            write_bin(outfp, decptr, decbytes);

            // wraparound the ringbuffer offset
            if(decoffset >= decode_ring_buffer - message_max_bytes) decoffset = 0;
        }
    }
}


int compare(file* f0, file* f1)
{
    int result = 0;

    while(0 == result) {
        char b0[65536];
        char b1[65536];
        const size_t r0 = fread(b0, 1, sizeof(b0), f0);
        const size_t r1 = fread(b1, 1, sizeof(b1), f1);

        result = (int) r0 - (int) r1;

        if(0 == r0 || 0 == r1) {
            break;
        }
        if(0 == result) {
            result = memcmp(b0, b1, r0);
        }
    }

    return result;
}


int main(int argc, char** argv)
{
    char inpfilename[256] = { 0 };
    char lz4filename[256] = { 0 };
    char decfilename[256] = { 0 };

    if(argc < 2) {
        printf("please specify input filename\n");
        return 0;
    }

    snprintf(inpfilename, 256, "%s", argv[1]);
    snprintf(lz4filename, 256, "%s.lz4s-%d", argv[1], 0);
    snprintf(decfilename, 256, "%s.lz4s-%d.dec", argv[1], 0);

    printf("inp = [%s]\n", inpfilename);
    printf("lz4 = [%s]\n", lz4filename);
    printf("dec = [%s]\n", decfilename);

    // compress
    {
        file* inpfp = fopen(inpfilename, "rb");
        file* outfp = fopen(lz4filename, "wb");

        test_compress(outfp, inpfp);

        fclose(outfp);
        fclose(inpfp);
    }

    // decompress
    {
        file* inpfp = fopen(lz4filename, "rb");
        file* outfp = fopen(decfilename, "wb");

        test_decompress(outfp, inpfp);

        fclose(outfp);
        fclose(inpfp);
    }

    // verify
    {
        file* inpfp = fopen(inpfilename, "rb");
        file* decfp = fopen(decfilename, "rb");

        const int cmp = compare(inpfp, decfp);
        if(0 == cmp) {
            printf("verify : ok\n");
        } else {
            printf("verify : ng\n");
        }

        fclose(decfp);
        fclose(inpfp);
    }

    return 0;
}
