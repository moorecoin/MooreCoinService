/*
 *  sha1.cpp
 *
 *  copyright (c) 1998, 2009
 *  paul e. jones <paulej@packetizer.com>
 *  all rights reserved.
 *
 *****************************************************************************
 *  $id: sha1.cpp 12 2009-06-22 19:34:25z paulej $
 *****************************************************************************
 *
 *  description:
 *      this class implements the secure hashing standard as defined
 *      in fips pub 180-1 published april 17, 1995.
 *
 *      the secure hashing standard, which uses the secure hashing
 *      algorithm (sha), produces a 160-bit message digest for a
 *      given data stream.  in theory, it is highly improbable that
 *      two messages will produce the same message digest.  therefore,
 *      this algorithm can serve as a means of providing a "fingerprint"
 *      for a message.
 *
 *  portability issues:
 *      sha-1 is defined in terms of 32-bit "words".  this code was
 *      written with the expectation that the processor has at least
 *      a 32-bit machine word size.  if the machine word size is larger,
 *      the code should still function properly.  one caveat to that
 *      is that the input functions taking characters and character arrays
 *      assume that only 8 bits of information are stored in each character.
 *
 *  caveats:
 *      sha-1 is designed to work with messages less than 2^64 bits long.
 *      although sha-1 allows a message digest to be generated for
 *      messages of any number of bits less than 2^64, this implementation
 *      only works with messages with a length that is a multiple of 8
 *      bits.
 *
 */

namespace websocketpp_02
{

/*
 *  sha1
 *
 *  description:
 *      this is the constructor for the sha1 class.
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
sha1::sha1()
{
    reset();
}

/*
 *  ~sha1
 *
 *  description:
 *      this is the destructor for the sha1 class
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
sha1::~sha1()
{
    // the destructor does nothing
}

/*
 *  reset
 *
 *  description:
 *      this function will initialize the sha1 class member variables
 *      in preparation for computing a new message digest.
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
void sha1::reset()
{
    length_low          = 0;
    length_high         = 0;
    message_block_index = 0;

    h[0]        = 0x67452301;
    h[1]        = 0xefcdab89;
    h[2]        = 0x98badcfe;
    h[3]        = 0x10325476;
    h[4]        = 0xc3d2e1f0;

    computed    = false;
    corrupted   = false;
}

/*
 *  result
 *
 *  description:
 *      this function will return the 160-bit message digest into the
 *      array provided.
 *
 *  parameters:
 *      message_digest_array: [out]
 *          this is an array of five unsigned integers which will be filled
 *          with the message digest that has been computed.
 *
 *  returns:
 *      true if successful, false if it failed.
 *
 *  comments:
 *
 */
bool sha1::result(unsigned *message_digest_array)
{
    int i;                                  // counter

    if (corrupted)
    {
        return false;
    }

    if (!computed)
    {
        padmessage();
        computed = true;
    }

    for(i = 0; i < 5; i++)
    {
        message_digest_array[i] = h[i];
    }

    return true;
}

/*
 *  input
 *
 *  description:
 *      this function accepts an array of octets as the next portion of
 *      the message.
 *
 *  parameters:
 *      message_array: [in]
 *          an array of characters representing the next portion of the
 *          message.
 *
 *  returns:
 *      nothing.
 *
 *  comments:
 *
 */
void sha1::input(   const unsigned char *message_array,
                    unsigned            length)
{
    if (!length)
    {
        return;
    }

    if (computed || corrupted)
    {
        corrupted = true;
        return;
    }

    while(length-- && !corrupted)
    {
        message_block[message_block_index++] = (*message_array & 0xff);

        length_low += 8;
        length_low &= 0xffffffff;               // force it to 32 bits
        if (length_low == 0)
        {
            length_high++;
            length_high &= 0xffffffff;          // force it to 32 bits
            if (length_high == 0)
            {
                corrupted = true;               // message is too long
            }
        }

        if (message_block_index == 64)
        {
            processmessageblock();
        }

        message_array++;
    }
}

/*
 *  input
 *
 *  description:
 *      this function accepts an array of octets as the next portion of
 *      the message.
 *
 *  parameters:
 *      message_array: [in]
 *          an array of characters representing the next portion of the
 *          message.
 *      length: [in]
 *          the length of the message_array
 *
 *  returns:
 *      nothing.
 *
 *  comments:
 *
 */
void sha1::input(   const char  *message_array,
                    unsigned    length)
{
    input((unsigned char *) message_array, length);
}

/*
 *  input
 *
 *  description:
 *      this function accepts a single octets as the next message element.
 *
 *  parameters:
 *      message_element: [in]
 *          the next octet in the message.
 *
 *  returns:
 *      nothing.
 *
 *  comments:
 *
 */
void sha1::input(unsigned char message_element)
{
    input(&message_element, 1);
}

/*
 *  input
 *
 *  description:
 *      this function accepts a single octet as the next message element.
 *
 *  parameters:
 *      message_element: [in]
 *          the next octet in the message.
 *
 *  returns:
 *      nothing.
 *
 *  comments:
 *
 */
void sha1::input(char message_element)
{
    input((unsigned char *) &message_element, 1);
}

/*
 *  operator<<
 *
 *  description:
 *      this operator makes it convenient to provide character strings to
 *      the sha1 object for processing.
 *
 *  parameters:
 *      message_array: [in]
 *          the character array to take as input.
 *
 *  returns:
 *      a reference to the sha1 object.
 *
 *  comments:
 *      each character is assumed to hold 8 bits of information.
 *
 */
sha1& sha1::operator<<(const char *message_array)
{
    const char *p = message_array;

    while(*p)
    {
        input(*p);
        p++;
    }

    return *this;
}

/*
 *  operator<<
 *
 *  description:
 *      this operator makes it convenient to provide character strings to
 *      the sha1 object for processing.
 *
 *  parameters:
 *      message_array: [in]
 *          the character array to take as input.
 *
 *  returns:
 *      a reference to the sha1 object.
 *
 *  comments:
 *      each character is assumed to hold 8 bits of information.
 *
 */
sha1& sha1::operator<<(const unsigned char *message_array)
{
    const unsigned char *p = message_array;

    while(*p)
    {
        input(*p);
        p++;
    }

    return *this;
}

/*
 *  operator<<
 *
 *  description:
 *      this function provides the next octet in the message.
 *
 *  parameters:
 *      message_element: [in]
 *          the next octet in the message
 *
 *  returns:
 *      a reference to the sha1 object.
 *
 *  comments:
 *      the character is assumed to hold 8 bits of information.
 *
 */
sha1& sha1::operator<<(const char message_element)
{
    input((unsigned char *) &message_element, 1);

    return *this;
}

/*
 *  operator<<
 *
 *  description:
 *      this function provides the next octet in the message.
 *
 *  parameters:
 *      message_element: [in]
 *          the next octet in the message
 *
 *  returns:
 *      a reference to the sha1 object.
 *
 *  comments:
 *      the character is assumed to hold 8 bits of information.
 *
 */
sha1& sha1::operator<<(const unsigned char message_element)
{
    input(&message_element, 1);

    return *this;
}

/*
 *  processmessageblock
 *
 *  description:
 *      this function will process the next 512 bits of the message
 *      stored in the message_block array.
 *
 *  parameters:
 *      none.
 *
 *  returns:
 *      nothing.
 *
 *  comments:
 *      many of the variable names in this function, especially the single
 *      character names, were used because those were the names used
 *      in the publication.
 *
 */
void sha1::processmessageblock()
{
    const unsigned k[] =    {               // constants defined for sha-1
                                0x5a827999,
                                0x6ed9eba1,
                                0x8f1bbcdc,
                                0xca62c1d6
                            };
    int         t;                          // loop counter
    unsigned    temp;                       // temporary word value
    unsigned    w[80];                      // word sequence
    unsigned    a, b, c, d, e;              // word buffers

    /*
     *  initialize the first 16 words in the array w
     */
    for(t = 0; t < 16; t++)
    {
        w[t] = ((unsigned) message_block[t * 4]) << 24;
        w[t] |= ((unsigned) message_block[t * 4 + 1]) << 16;
        w[t] |= ((unsigned) message_block[t * 4 + 2]) << 8;
        w[t] |= ((unsigned) message_block[t * 4 + 3]);
    }

    for(t = 16; t < 80; t++)
    {
       w[t] = circularshift(1,w[t-3] ^ w[t-8] ^ w[t-14] ^ w[t-16]);
    }

    a = h[0];
    b = h[1];
    c = h[2];
    d = h[3];
    e = h[4];

    for(t = 0; t < 20; t++)
    {
        temp = circularshift(5,a) + ((b & c) | ((~b) & d)) + e + w[t] + k[0];
        temp &= 0xffffffff;
        e = d;
        d = c;
        c = circularshift(30,b);
        b = a;
        a = temp;
    }

    for(t = 20; t < 40; t++)
    {
        temp = circularshift(5,a) + (b ^ c ^ d) + e + w[t] + k[1];
        temp &= 0xffffffff;
        e = d;
        d = c;
        c = circularshift(30,b);
        b = a;
        a = temp;
    }

    for(t = 40; t < 60; t++)
    {
        temp = circularshift(5,a) +
               ((b & c) | (b & d) | (c & d)) + e + w[t] + k[2];
        temp &= 0xffffffff;
        e = d;
        d = c;
        c = circularshift(30,b);
        b = a;
        a = temp;
    }

    for(t = 60; t < 80; t++)
    {
        temp = circularshift(5,a) + (b ^ c ^ d) + e + w[t] + k[3];
        temp &= 0xffffffff;
        e = d;
        d = c;
        c = circularshift(30,b);
        b = a;
        a = temp;
    }

    h[0] = (h[0] + a) & 0xffffffff;
    h[1] = (h[1] + b) & 0xffffffff;
    h[2] = (h[2] + c) & 0xffffffff;
    h[3] = (h[3] + d) & 0xffffffff;
    h[4] = (h[4] + e) & 0xffffffff;

    message_block_index = 0;
}

/*
 *  padmessage
 *
 *  description:
 *      according to the standard, the message must be padded to an even
 *      512 bits.  the first padding bit must be a '1'.  the last 64 bits
 *      represent the length of the original message.  all bits in between
 *      should be 0.  this function will pad the message according to those
 *      rules by filling the message_block array accordingly.  it will also
 *      call processmessageblock() appropriately.  when it returns, it
 *      can be assumed that the message digest has been computed.
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
void sha1::padmessage()
{
    /*
     *  check to see if the current message block is too small to hold
     *  the initial padding bits and length.  if so, we will pad the
     *  block, process it, and then continue padding into a second block.
     */
    if (message_block_index > 55)
    {
        message_block[message_block_index++] = 0x80;
        while(message_block_index < 64)
        {
            message_block[message_block_index++] = 0;
        }

        processmessageblock();

        while(message_block_index < 56)
        {
            message_block[message_block_index++] = 0;
        }
    }
    else
    {
        message_block[message_block_index++] = 0x80;
        while(message_block_index < 56)
        {
            message_block[message_block_index++] = 0;
        }

    }

    /*
     *  store the message length as the last 8 octets
     */
    message_block[56] = (length_high >> 24) & 0xff;
    message_block[57] = (length_high >> 16) & 0xff;
    message_block[58] = (length_high >> 8) & 0xff;
    message_block[59] = (length_high) & 0xff;
    message_block[60] = (length_low >> 24) & 0xff;
    message_block[61] = (length_low >> 16) & 0xff;
    message_block[62] = (length_low >> 8) & 0xff;
    message_block[63] = (length_low) & 0xff;

    processmessageblock();
}


/*
 *  circularshift
 *
 *  description:
 *      this member function will perform a circular shifting operation.
 *
 *  parameters:
 *      bits: [in]
 *          the number of bits to shift (1-31)
 *      word: [in]
 *          the value to shift (assumes a 32-bit integer)
 *
 *  returns:
 *      the shifted value.
 *
 *  comments:
 *
 */
unsigned sha1::circularshift(int bits, unsigned word)
{
    return ((word << bits) & 0xffffffff) | ((word & 0xffffffff) >> (32-bits));
}

}
