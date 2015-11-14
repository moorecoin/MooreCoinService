// lz4 streaming api example : double buffer
// copyright : takayuki matsuoka


#define _crt_secure_no_warnings // for msvc
#include "lz4.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum {
    block_bytes = 1024 * 8,
//  block_bytes = 1024 * 64,
};


size_t write_int(file* fp, int i) {
    return fwrite(&i, sizeof(i), 1, fp);
}

size_t write_bin(file* fp, const void* array, size_t arraybytes) {
    return fwrite(array, 1, arraybytes, fp);
}

size_t read_int(file* fp, int* i) {
    return fread(i, sizeof(*i), 1, fp);
}

size_t read_bin(file* fp, void* array, size_t arraybytes) {
    return fread(array, 1, arraybytes, fp);
}


void test_compress(file* outfp, file* inpfp)
{
    lz4_stream_t lz4stream_body = { 0 };
    lz4_stream_t* lz4stream = &lz4stream_body;

    char inpbuf[2][block_bytes];
    int  inpbufindex = 0;

    for(;;) {
        char* const inpptr = inpbuf[inpbufindex];
        const int inpbytes = (int) read_bin(inpfp, inpptr, block_bytes);
        if(0 == inpbytes) {
            break;
        }

        {
            char cmpbuf[lz4_compressbound(block_bytes)];
            const int cmpbytes = lz4_compress_continue(
                lz4stream, inpptr, cmpbuf, inpbytes);
            if(cmpbytes <= 0) {
                break;
            }
            write_int(outfp, cmpbytes);
            write_bin(outfp, cmpbuf, (size_t) cmpbytes);
        }

        inpbufindex = (inpbufindex + 1) % 2;
    }

    write_int(outfp, 0);
}


void test_decompress(file* outfp, file* inpfp)
{
    lz4_streamdecode_t lz4streamdecode_body = { 0 };
    lz4_streamdecode_t* lz4streamdecode = &lz4streamdecode_body;

    char decbuf[2][block_bytes];
    int  decbufindex = 0;

    for(;;) {
        char cmpbuf[lz4_compressbound(block_bytes)];
        int  cmpbytes = 0;

        {
            const size_t readcount0 = read_int(inpfp, &cmpbytes);
            if(readcount0 != 1 || cmpbytes <= 0) {
                break;
            }

            const size_t readcount1 = read_bin(inpfp, cmpbuf, (size_t) cmpbytes);
            if(readcount1 != (size_t) cmpbytes) {
                break;
            }
        }

        {
            char* const decptr = decbuf[decbufindex];
            const int decbytes = lz4_decompress_safe_continue(
                lz4streamdecode, cmpbuf, decptr, cmpbytes, block_bytes);
            if(decbytes <= 0) {
                break;
            }
            write_bin(outfp, decptr, (size_t) decbytes);
        }

        decbufindex = (decbufindex + 1) % 2;
    }
}


int compare(file* fp0, file* fp1)
{
    int result = 0;

    while(0 == result) {
        char b0[65536];
        char b1[65536];
        const size_t r0 = read_bin(fp0, b0, sizeof(b0));
        const size_t r1 = read_bin(fp1, b1, sizeof(b1));

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


int main(int argc, char* argv[])
{
    char inpfilename[256] = { 0 };
    char lz4filename[256] = { 0 };
    char decfilename[256] = { 0 };

    if(argc < 2) {
        printf("please specify input filename\n");
        return 0;
    }

    snprintf(inpfilename, 256, "%s", argv[1]);
    snprintf(lz4filename, 256, "%s.lz4s-%d", argv[1], block_bytes);
    snprintf(decfilename, 256, "%s.lz4s-%d.dec", argv[1], block_bytes);

    printf("inp = [%s]\n", inpfilename);
    printf("lz4 = [%s]\n", lz4filename);
    printf("dec = [%s]\n", decfilename);

    // compress
    {
        file* inpfp = fopen(inpfilename, "rb");
        file* outfp = fopen(lz4filename, "wb");

        printf("compress : %s -> %s\n", inpfilename, lz4filename);
        test_compress(outfp, inpfp);
        printf("compress : done\n");

        fclose(outfp);
        fclose(inpfp);
    }

    // decompress
    {
        file* inpfp = fopen(lz4filename, "rb");
        file* outfp = fopen(decfilename, "wb");

        printf("decompress : %s -> %s\n", lz4filename, decfilename);
        test_decompress(outfp, inpfp);
        printf("decompress : done\n");

        fclose(outfp);
        fclose(inpfp);
    }

    // verify
    {
        file* inpfp = fopen(inpfilename, "rb");
        file* decfp = fopen(decfilename, "rb");

        printf("verify : %s <-> %s\n", inpfilename, decfilename);
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
