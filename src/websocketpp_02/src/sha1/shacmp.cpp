/*
 *  shacmp.cpp
 *
 *  copyright (c) 1998, 2009
 *  paul e. jones <paulej@packetizer.com>
 *  all rights reserved
 *
 *****************************************************************************
 *  $id: shacmp.cpp 12 2009-06-22 19:34:25z paulej $
 *****************************************************************************
 *
 *  description:
 *      this utility will compare two files by producing a message digest
 *      for each file using the secure hashing algorithm and comparing
 *      the message digests.  this function will return 0 if they
 *      compare or 1 if they do not or if there is an error.
 *      errors result in a return code higher than 1.
 *
 *  portability issues:
 *      none.
 *
 */

#include <stdio.h>
#include <string.h>
#include "sha1.h"

/*
 *  return codes
 */
#define sha1_compare        0
#define sha1_no_compare     1
#define sha1_usage_error    2
#define sha1_file_error     3

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
    unsigned    message_digest[2][5];       // message digest for files
    int         i;                          // counter
    bool        message_match;              // message digest match flag
    int         returncode;

    /*
     *  if we have two arguments, we will assume they are filenames.  if
     *  we do not have to arguments, call usage() and exit.
     */
    if (argc != 3)
    {
        usage();
        return sha1_usage_error;
    }

    /*
     *  get the message digests for each file
     */
    for(i = 1; i <= 2; i++)
    {
        sha.reset();

        if (!(fp = fopen(argv[i],"rb")))
        {
            fprintf(stderr, "sha: unable to open file %s\n", argv[i]);
            return sha1_file_error;
        }

        c = fgetc(fp);
        while(!feof(fp))
        {
            sha.input(c);
            c = fgetc(fp);
        }

        fclose(fp);

        if (!sha.result(message_digest[i-1]))
        {
            fprintf(stderr,"shacmp: could not compute message digest for %s\n",
                    argv[i]);
            return sha1_file_error;
        }
    }

    /*
     *  compare the message digest values
     */
    message_match = true;
    for(i = 0; i < 5; i++)
    {
        if (message_digest[0][i] != message_digest[1][i])
        {
            message_match = false;
            break;
        }
    }

    if (message_match)
    {
        printf("fingerprints match:\n");
        returncode = sha1_compare;
    }
    else
    {
        printf("fingerprints do not match:\n");
        returncode = sha1_no_compare;
    }

    printf( "\t%08x %08x %08x %08x %08x\n",
            message_digest[0][0],
            message_digest[0][1],
            message_digest[0][2],
            message_digest[0][3],
            message_digest[0][4]);
    printf( "\t%08x %08x %08x %08x %08x\n",
            message_digest[1][0],
            message_digest[1][1],
            message_digest[1][2],
            message_digest[1][3],
            message_digest[1][4]);

    return returncode;
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
    printf("usage: shacmp <file> <file>\n");
    printf("\tthis program will compare the message digests (fingerprints)\n");
    printf("\tfor two files using the secure hashing algorithm (sha-1).\n");
}
