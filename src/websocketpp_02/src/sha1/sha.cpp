/*
 *  sha.cpp
 *
 *  copyright (c) 1998, 2009
 *  paul e. jones <paulej@packetizer.com>
 *  all rights reserved
 *
 *****************************************************************************
 *  $id: sha.cpp 13 2009-06-22 20:20:32z paulej $
 *****************************************************************************
 *
 *  description:
 *      this utility will display the message digest (fingerprint) for
 *      the specified file(s).
 *
 *  portability issues:
 *      none.
 */

#include <stdio.h>
#include <string.h>
#if beast_win32
#include <io.h>
#endif
#include <fcntl.h>
#include "sha1.h"

/*
 *  function prototype
 */
void usage();


/*  
 *  main
 *
 *  description:
 *      this is the entry point for the program
 *
 *  parameters:
 *      argc: [in]
 *          this is the count of arguments in the argv array
 *      argv: [in]
 *          this is an array of filenames for which to compute message digests
 *
 *  returns:
 *      nothing.
 *
 *  comments:
 *
 */
int main(int argc, char *argv[])
{
    sha1        sha;                        // sha-1 class
    file        *fp;                        // file pointer for reading files
    char        c;                          // character read from file
    unsigned    message_digest[5];          // message digest from "sha"
    int         i;                          // counter
    bool        reading_stdin;              // are we reading standard in?
    bool        read_stdin = false;         // have we read stdin?

    /*
     *  check the program arguments and print usage information if -?
     *  or --help is passed as the first argument.
     */
    if (argc > 1 && (!strcmp(argv[1],"-?") || !strcmp(argv[1],"--help")))
    {
        usage();
        return 1;
    }

    /*
     *  for each filename passed in on the command line, calculate the
     *  sha-1 value and display it.
     */
    for(i = 0; i < argc; i++)
    {
        /*
         *  we start the counter at 0 to guarantee entry into the for loop.
         *  so if 'i' is zero, we will increment it now.  if there is no
         *  argv[1], we will use stdin below.
         */
        if (i == 0)
        {
            i++;
        }

        if (argc == 1 || !strcmp(argv[i],"-"))
        {
#if beast_win32
            _setmode(_fileno(stdin), _o_binary);
#endif
            fp = stdin;
            reading_stdin = true;
        }
        else
        {
            if (!(fp = fopen(argv[i],"rb")))
            {
                fprintf(stderr, "sha: unable to open file %s\n", argv[i]);
                return 2;
            }
            reading_stdin = false;
        }

        /*
         *  we do not want to read stdin multiple times
         */
        if (reading_stdin)
        {
            if (read_stdin)
            {
                continue;
            }

            read_stdin = true;
        }

        /*
         *  reset the sha1 object and process input
         */
        sha.reset();

        c = fgetc(fp);
        while(!feof(fp))
        {
            sha.input(c);
            c = fgetc(fp);
        }

        if (!reading_stdin)
        {
            fclose(fp);
        }

        if (!sha.result(message_digest))
        {
            fprintf(stderr,"sha: could not compute message digest for %s\n",
                    reading_stdin?"stdin":argv[i]);
        }
        else
        {
            printf( "%08x %08x %08x %08x %08x - %s\n",
                    message_digest[0],
                    message_digest[1],
                    message_digest[2],
                    message_digest[3],
                    message_digest[4],
                    reading_stdin?"stdin":argv[i]);
        }
    }

    return 0;
}

/*  
 *  usage
 *
 *  description:
 *      this function will display program usage information to the user.
 *
 *  parameters:
 *      none.
 *
 *  returns:
 *      nothing.
 *
 *  comments:
 *
 */
void usage()
{
    printf("usage: sha <file> [<file> ...]\n");
    printf("\tthis program will display the message digest (fingerprint)\n");
    printf("\tfor files using the secure hashing algorithm (sha-1).\n");
}
