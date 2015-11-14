/*
 * file:	sha2speed.c
 * author:	aaron d. gifford - http://www.aarongifford.com/
 * 
 * copyright (c) 2000-2001, aaron d. gifford
 * all rights reserved.
 *
 * redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. neither the name of the copyright holder nor the names of contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * this software is provided by the author and contributor(s) ``as is'' and
 * any express or implied warranties, including, but not limited to, the
 * implied warranties of merchantability and fitness for a particular purpose
 * are disclaimed.  in no event shall the author or contributor(s) be liable
 * for any direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute goods
 * or services; loss of use, data, or profits; or business interruption)
 * however caused and on any theory of liability, whether in contract, strict
 * liability, or tort (including negligence or otherwise) arising in any way
 * out of the use of this software, even if advised of the possibility of
 * such damage.
 *
 * $id: sha2speed.c,v 1.1 2001/11/08 00:02:23 adg exp adg $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "sha2.h"

#define bufsize	16384

void usage(char *prog) {
	fprintf(stderr, "usage:\t%s [<num-of-bytes>] [<num-of-loops>] [<fill-byte>]\n", prog);
	exit(-1);
}

void printspeed(char *caption, unsigned long bytes, double time) {
	if (bytes / 1073741824ul > 0) {
                printf("%s %.4f sec (%.3f gbps)\n", caption, time, (double)bytes/1073741824ul/time);
        } else if (bytes / 1048576 > 0) {
                printf("%s %.4f (%.3f mbps)\n", caption, time, (double)bytes/1048576/time);
        } else if (bytes / 1024 > 0) {
                printf("%s %.4f (%.3f kbps)\n", caption, time, (double)bytes/1024/time);
        } else {
		printf("%s %.4f (%f bps)\n", caption, time, (double)bytes/time);
	}
}


int main(int argc, char **argv) {
	sha256_ctx	c256;
	sha384_ctx	c384;
	sha512_ctx	c512;
	char		buf[bufsize];
	char		md[sha512_digest_string_length];
	int		bytes, blocks, rep, i, j;
	struct timeval	start, end;
	double		t, ave256, ave384, ave512;
	double		best256, best384, best512;

	if (argc > 4) {
		usage(argv[0]);
	}

	/* default to 1024 16k blocks (16 mb) */
	bytes = 1024 * 1024 * 16;
	if (argc > 1) {
		blocks = atoi(argv[1]);
	}
	blocks = bytes / bufsize;

	/* default to 10 repetitions */
	rep = 10;
	if (argc > 2) {
		rep = atoi(argv[2]);
	}

	/* set up the input data */
	if (argc > 3) {
		memset(buf, atoi(argv[2]), bufsize);
	} else {
		memset(buf, 0xb7, bufsize);
	}

	ave256 = ave384 = ave512 = 0;
	best256 = best384 = best512 = 100000;
	for (i = 0; i < rep; i++) {
		sha256_init(&c256);
		sha384_init(&c384);
		sha512_init(&c512);
	
		gettimeofday(&start, (struct timezone*)0);
		for (j = 0; j < blocks; j++) {
			sha256_update(&c256, (unsigned char*)buf, bufsize);
		}
		if (bytes % bufsize) {
			sha256_update(&c256, (unsigned char*)buf, bytes % bufsize);
		}
		sha256_end(&c256, md);
		gettimeofday(&end, (struct timezone*)0);
		t = ((end.tv_sec - start.tv_sec) * 1000000.0 + (end.tv_usec - start.tv_usec)) / 1000000.0;
		ave256 += t;
		if (t < best256) {
			best256 = t;
		}
		printf("sha-256[%d] (%.4f/%.4f/%.4f seconds) = 0x%s\n", i+1, t, ave256/(i+1), best256, md);

		gettimeofday(&start, (struct timezone*)0);
		for (j = 0; j < blocks; j++) {
			sha384_update(&c384, (unsigned char*)buf, bufsize);
		}
		if (bytes % bufsize) {
			sha384_update(&c384, (unsigned char*)buf, bytes % bufsize);
		}
		sha384_end(&c384, md);
		gettimeofday(&end, (struct timezone*)0);
		t = ((end.tv_sec - start.tv_sec) * 1000000.0 + (end.tv_usec - start.tv_usec)) / 1000000.0;
		ave384 += t;
		if (t < best384) {
			best384 = t;
		}
		printf("sha-384[%d] (%.4f/%.4f/%.4f seconds) = 0x%s\n", i+1, t, ave384/(i+1), best384, md);

		gettimeofday(&start, (struct timezone*)0);
		for (j = 0; j < blocks; j++) {
			sha512_update(&c512, (unsigned char*)buf, bufsize);
		}
		if (bytes % bufsize) {
			sha512_update(&c512, (unsigned char*)buf, bytes % bufsize);
		}
		sha512_end(&c512, md);
		gettimeofday(&end, (struct timezone*)0);
		t = ((end.tv_sec - start.tv_sec) * 1000000.0 + (end.tv_usec - start.tv_usec)) / 1000000.0;
		ave512 += t;
		if (t < best512) {
			best512 = t;
		}
		printf("sha-512[%d] (%.4f/%.4f/%.4f seconds) = 0x%s\n", i+1, t, ave512/(i+1), best512, md);
	}
	ave256 /= rep;
	ave384 /= rep;
	ave512 /= rep;
	printf("\ntest results summary:\ntest repetitions: %d\n", rep);
	if (bytes / 1073741824ul > 0) {
		printf("test set size: %.3f gb\n", (double)bytes/1073741824ul);
	} else if (bytes / 1048576 > 0) {
		printf("test set size: %.3f mb\n", (double)bytes/1048576);
	} else if (bytes /1024 > 0) {
		printf("test set size: %.3f kb\n", (double)bytes/1024);
	} else {
		printf("test set size: %d b\n", bytes);
	}
	printspeed("sha-256 average:", bytes, ave256);
	printspeed("sha-256 best:   ", bytes, best256);
	printspeed("sha-384 average:", bytes, ave384);
	printspeed("sha-384 best:   ", bytes, best384);
	printspeed("sha-512 average:", bytes, ave512);
	printspeed("sha-512 best:   ", bytes, best512);

	return 1;
}

