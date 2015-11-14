//------------------------------------------------------------------------------
/*
    this file is part of rippled: https://github.com/ripple/rippled
    copyright (c) 2012, 2013 ripple labs inc.

    permission to use, copy, modify, and/or distribute this software for any
    purpose  with  or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    the  software is provided "as is" and the author disclaims all warranties
    with  regard  to  this  software  including  all  implied  warranties  of
    merchantability  and  fitness. in no event shall the author be liable for
    any  special ,  direct, indirect, or consequential damages or any damages
    whatsoever  resulting  from  loss  of use, data or profits, whether in an
    action  of  contract, negligence or other tortious action, arising out of
    or in connection with the use or performance of this software.
*/
//==============================================================================

#ifndef ripple_protocol_txflags_h_included
#define ripple_protocol_txflags_h_included

namespace ripple {

//
// transaction flags.
//

/** transaction flags.

    these flags modify the behavior of an operation.

    @note changing these will create a hard fork
    @ingroup protocol
*/
class txflag
{
public:
    static std::uint32_t const requiredesttag = 0x00010000;
};
// vfalco todo move all flags into this container after some study.

// universal transaction flags:
const std::uint32_t tffullycanonicalsig    = 0x80000000;
const std::uint32_t tfuniversal            = tffullycanonicalsig;
const std::uint32_t tfuniversalmask        = ~ tfuniversal;

// accountset flags:
// vfalco todo javadoc comment every one of these constants
//const std::uint32_t txflag::requiredesttag       = 0x00010000;
const std::uint32_t tfoptionaldesttag      = 0x00020000;
const std::uint32_t tfrequireauth          = 0x00040000;
const std::uint32_t tfoptionalauth         = 0x00080000;
const std::uint32_t tfdisallowxrp          = 0x00100000;
const std::uint32_t tfallowxrp             = 0x00200000;
const std::uint32_t tfaccountsetmask       = ~ (tfuniversal | txflag::requiredesttag | tfoptionaldesttag
                                             | tfrequireauth | tfoptionalauth
                                             | tfdisallowxrp | tfallowxrp);

// accountset setflag/clearflag values
const std::uint32_t asfrequiredest         = 1;
const std::uint32_t asfrequireauth         = 2;
const std::uint32_t asfdisallowxrp         = 3;
const std::uint32_t asfdisablemaster       = 4;
const std::uint32_t asfaccounttxnid        = 5;
const std::uint32_t asfnofreeze            = 6;
const std::uint32_t asfglobalfreeze        = 7;

// offercreate flags:
const std::uint32_t tfpassive              = 0x00010000;
const std::uint32_t tfimmediateorcancel    = 0x00020000;
const std::uint32_t tffillorkill           = 0x00040000;
const std::uint32_t tfsell                 = 0x00080000;
const std::uint32_t tfoffercreatemask      = ~ (tfuniversal | tfpassive | tfimmediateorcancel | tffillorkill | tfsell);

// payment flags:
const std::uint32_t tfnorippledirect       = 0x00010000;
const std::uint32_t tfpartialpayment       = 0x00020000;
const std::uint32_t tflimitquality         = 0x00040000;
const std::uint32_t tfpaymentmask          = ~ (tfuniversal | tfpartialpayment | tflimitquality | tfnorippledirect);

// trustset flags:
const std::uint32_t tfsetfauth             = 0x00010000;
const std::uint32_t tfsetnoripple          = 0x00020000;
const std::uint32_t tfclearnoripple        = 0x00040000;
const std::uint32_t tfsetfreeze            = 0x00100000;
const std::uint32_t tfclearfreeze          = 0x00200000;
const std::uint32_t tftrustsetmask         = ~ (tfuniversal | tfsetfauth | tfsetnoripple | tfclearnoripple
                                             | tfsetfreeze | tfclearfreeze);

} // ripple

#endif
