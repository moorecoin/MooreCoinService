// lz4 streaming api example : line-by-line logfile compression
// copyright : takayuki matsuoka


#define _crt_secure_no_warnings // for msvc
#include "lz4.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static size_t write_uint16(file* fp, uint16_t i)
{
    return fwrite(&i, sizeof(i), 1, fp);
}

static size_t write_bin(file* fp, const void* array, int arraybytes)
{
    return fwrite(array, 1, arraybytes, fp);
}

static size_t read_uint16(file* fp, uint16_t* i)
{
    return fread(i, sizeof(*i), 1, fp);
}

static size_t read_bin(file* fp, void* array, int arraybytes)
{
    return fread(array, 1, arraybytes, fp);
}


static void test_compress(
    file* outfp,
    file* inpfp,
    size_t messagemaxbytes,
    size_t ringbufferbytes)
{
    lz4_stream_t* const lz4stream = lz4_createstream();
    char* const cmpbuf = malloc(lz4_compressbound(messagemaxbytes));
    char* const inpbuf = malloc(ringbufferbytes);
    int inpoffset = 0;

    for ( ; ; )
    {
        char* const inpptr = &inpbuf[inpoffset];

#if 0
        // read random length data to the ring buffer.
        const int randomlength = (rand() % messagemaxbytes) + 1;
        const int inpbytes = (int) read_bin(inpfp, inpptr, randomlength);
        if (0 == inpbytes) break;
#else
        // read line to the ring buffer.
        int inpbytes = 0;
        if (!fgets(inpptr, (int) messagemaxbytes, inpfp))
            break;
        inpbytes = (int) strlen(inpptr);
#endif

        {
            const int cmpbytes = lz4_compress_continue(
                lz4stream, inpptr, cmpbuf, inpbytes);
            if (cmpbytes <= 0) break;
            write_uint16(outfp, (uint16_t) cmpbytes);
            write_bin(outfp, cmpbuf, cmpbytes);

            // add and wraparound the ringbuffer offset
            inpoffset += inpbytes;
            if ((size_t)inpoffset >= ringbufferbytes - messagemaxbytes) inpoffset = 0;
        }
    }
    write_uint16(outfp, 0);

    free(inpbuf);
    free(cmpbuf);
    lz4_freestream(lz4stream);
}


static void test_decompress(
    file* outfp,
    file* inpfp,
    size_t messagemaxbytes,
    size_t ringbufferbytes)
{
    lz4_streamdecode_t* const lz4streamdecode = lz4_createstreamdecode();
    char* const cmpbuf = malloc(lz4_compressbound(messagemaxbytes));
    char* const decbuf = malloc(ringbufferbytes);
    int decoffset = 0;

    for ( ; ; )
    {
        uint16_t cmpbytes = 0;

        if (read_uint16(inpfp, &cmpbytes) != 1) break;
        if (cmpbytes <= 0) break;
        if (read_bin(inpfp, cmpbuf, cmpbytes) != cmpbytes) break;

        {
            char* const decptr = &decbuf[decoffset];
            const int decbytes = lz4_decompress_safe_continue(
                lz4streamdecode, cmpbuf, decptr, cmpbytes, (int) messagemaxbytes);
            if (decbytes <= 0) break;
            write_bin(outfp, decptr, decbytes);

            // add and wraparound the ringbuffer offset
            decoffset += decbytes;
            if ((size_t)decoffset >= ringbufferbytes - messagemaxbytes) decoffset = 0;
        }
    }

    free(decbuf);
    free(cmpbuf);
    lz4_freestreamdecode(lz4streamdecode);
}


static int compare(file* f0, file* f1)
{
    int result = 0;
    const size_t tempbufferbytes = 65536;
    char* const b0 = malloc(tempbufferbytes);
    char* const b1 = malloc(tempbufferbytes);

    while(0 == result)
    {
        const size_t r0 = fread(b0, 1, tempbufferbytes, f0);
        const size_t r1 = fread(b1, 1, tempbufferbytes, f1);

        result = (int) r0 - (int) r1;

        if (0 == r0 || 0 == r1) break;
        if (0 == result) result = memcmp(b0, b1, r0);
    }

    free(b1);
    free(b0);
    return result;
}


int main(int argc, char* argv[])
{
    enum {
        message_max_bytes   = 1024,
        ring_buffer_bytes   = 1024 * 256 + message_max_bytes,
    };

    char inpfilename[256] = { 0 };
    char lz4filename[256] = { 0 };
    char decfilename[256] = { 0 };

    if (argc < 2)
    {
        printf("please specify input filename\n");
        return 0;
    }

    snprintf(inpfilename, 256, "%s", argv[1]);
    snprintf(lz4filename, 256, "%s.lz4s", argv[1]);
    snprintf(decfilename, 256, "%s.lz4s.dec", argv[1]);

    printf("inp = [%s]\n", inpfilename);
    printf("lz4 = [%s]\n", lz4filename);
    printf("dec = [%s]\n", decfilename);

    // compress
    {
        file* inpfp = fopen(inpfilename, "rb");
        file* outfp = fopen(lz4filename, "wb");

        test_compress(outfp, inpfp, message_max_bytes, ring_buffer_bytes);

        fclose(outfp);
        fclose(inpfp);
    }

    // decompress
    {
        file* inpfp = fopen(lz4filename, "rb");
        file* outfp = fopen(decfilename, "wb");

        test_decompress(outfp, inpfp, message_max_bytes, ring_buffer_bytes);

        fclose(outfp);
        fclose(inpfp);
    }

    // verify
    {
        file* inpfp = fopen(inpfilename, "rb");
        file* decfp = fopen(decfilename, "rb");

        const int cmp = compare(inpfp, decfp);
        if (0 == cmp)
            printf("verify : ok\n");
        else
            printf("verify : ng\n");

        fclose(decfp);
        fclose(inpfp);
    }

    return 0;
}
