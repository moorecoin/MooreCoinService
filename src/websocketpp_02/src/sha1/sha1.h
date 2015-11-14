/*
 *  sha1.h
 *
 *  copyright (c) 1998, 2009
 *  paul e. jones <paulej@packetizer.com>
 *  all rights reserved.
 *
 *****************************************************************************
 *  $id: sha1.h 12 2009-06-22 19:34:25z paulej $
 *****************************************************************************
 *
 *  description:
 *      this class implements the secure hashing standard as defined
 *      in fips pub 180-1 published april 17, 1995.
 *
 *      many of the variable names in this class, especially the single
 *      character names, were used because those were the names used
 *      in the publication.
 *
 *      please read the file sha1.cpp for more information.
 *
 */

#ifndef _sha1_h_
#define _sha1_h_

namespace websocketpp_02 {

class sha1
{
    public:

        sha1();
        virtual ~sha1();

        /*
         *  re-initialize the class
         */
        void reset();

        /*
         *  returns the message digest
         */
        bool result(unsigned *message_digest_array);

        /*
         *  provide input to sha1
         */
        void input( const unsigned char *message_array,
                    unsigned            length);
        void input( const char  *message_array,
                    unsigned    length);
        void input(unsigned char message_element);
        void input(char message_element);
        sha1& operator<<(const char *message_array);
        sha1& operator<<(const unsigned char *message_array);
        sha1& operator<<(const char message_element);
        sha1& operator<<(const unsigned char message_element);

    private:

        /*
         *  process the next 512 bits of the message
         */
        void processmessageblock();

        /*
         *  pads the current message block to 512 bits
         */
        void padmessage();

        /*
         *  performs a circular left shift operation
         */
        inline unsigned circularshift(int bits, unsigned word);

        unsigned h[5];                      // message digest buffers

        unsigned length_low;                // message length in bits
        unsigned length_high;               // message length in bits

        unsigned char message_block[64];    // 512-bit message blocks
        int message_block_index;            // index into message block array

        bool computed;                      // is the digest computed?
        bool corrupted;                     // is the message digest corruped?

};

} // namespace websocketpp_02

#endif // _sha1_h_
