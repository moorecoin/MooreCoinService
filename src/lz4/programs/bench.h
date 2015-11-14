/*
    bench.h - demo program to benchmark open-source compression algorithm
    copyright (c) yann collet 2012-2014

    this program is free software; you can redistribute it and/or modify
    it under the terms of the gnu general public license as published by
    the free software foundation; either version 2 of the license, or
    (at your option) any later version.

    this program is distributed in the hope that it will be useful,
    but without any warranty; without even the implied warranty of
    merchantability or fitness for a particular purpose.  see the
    gnu general public license for more details.

    you should have received a copy of the gnu general public license along
    with this program; if not, write to the free software foundation, inc.,
    51 franklin street, fifth floor, boston, ma 02110-1301 usa.

    you can contact the author at :
    - lz4 source repository : http://code.google.com/p/lz4/
    - lz4 public forum : https://group.google.com/forum/#!forum/lz4c
*/
#pragma once

#if defined (__cplusplus)
extern "c" {
#endif


/* main function */
int bmk_benchfile(char** filenamestable, int nbfiles, int clevel);

/* set parameters */
void bmk_setblocksize(int bsize);
void bmk_setnbiterations(int nbloops);
void bmk_setpause(void);



#if defined (__cplusplus)
}
#endif
