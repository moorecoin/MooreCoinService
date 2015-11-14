/*
 * file:	sha2prog.c
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
 */

#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include "sha2.h"

void usage(char *prog, char *msg) {
	fprintf(stderr, "%s\nusage:\t%s [options] [<file>]\noptions:\n\t-256\tgenerate sha-256 hash\n\t-384\tgenerate sha-284 hash\n\t-512\tgenerate sha-512 hash\n\t-all\tgenerate all three hashes\n\t-q\tquiet mode - only output hexadecimal hashes, one per line\n\n", msg, prog);
	exit(-1);
}

#define buflen 16384

int main(int argc, char **argv) {
	int		kl, l, fd, ac;
	int		quiet = 0, hash = 0;
	char		*av, *file = (char*)0;
	file		*in = (file*)0;
	sha256_ctx	ctx256;
	sha384_ctx	ctx384;
	sha512_ctx	ctx512;
	unsigned char	buf[buflen];

	sha256_init(&ctx256);
	sha384_init(&ctx384);
	sha512_init(&ctx512);

	/* read data from stdin by default */
	fd = fileno(stdin);

	ac = 1;
	while (ac < argc) {
		if (*argv[ac] == '-') {
			av = argv[ac] + 1;
			if (!strcmp(av, "q")) {
				quiet = 1;
			} else if (!strcmp(av, "256")) {
				hash |= 1;
			} else if (!strcmp(av, "384")) {
				hash |= 2;
			} else if (!strcmp(av, "512")) {
				hash |= 4;
			} else if (!strcmp(av, "all")) {
				hash = 7;
			} else {
				usage(argv[0], "invalid option.");
			}
			ac++;
		} else {
			file = argv[ac++];
			if (ac != argc) {
				usage(argv[0], "too many arguments.");
			}
			if ((in = fopen(file, "r")) == null) {
				perror(argv[0]);
				exit(-1);
			}
			fd = fileno(in);
		}
	}
	if (hash == 0)
		hash = 7;	/* default to all */

	kl = 0;
	while ((l = read(fd,buf,buflen)) > 0) {
		kl += l;
		sha256_update(&ctx256, (unsigned char*)buf, l);
		sha384_update(&ctx384, (unsigned char*)buf, l);
		sha512_update(&ctx512, (unsigned char*)buf, l);
	}
	if (file) {
		fclose(in);
	}

	if (hash & 1) {
		sha256_end(&ctx256, buf);
		if (!quiet)
			printf("sha-256 (%s) = ", file);
		printf("%s\n", buf);
	}
	if (hash & 2) {
		sha384_end(&ctx384, buf);
		if (!quiet)
			printf("sha-384 (%s) = ", file);
		printf("%s\n", buf);
	}
	if (hash & 4) {
		sha512_end(&ctx512, buf);
		if (!quiet)
			printf("sha-512 (%s) = ", file);
		printf("%s\n", buf);
	}

	return 1;
}

