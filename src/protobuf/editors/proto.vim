" protocol buffers - google's data interchange format
" copyright 2008 google inc.  all rights reserved.
" http://code.google.com/p/protobuf/
"
" redistribution and use in source and binary forms, with or without
" modification, are permitted provided that the following conditions are
" met:
"
"     * redistributions of source code must retain the above copyright
" notice, this list of conditions and the following disclaimer.
"     * redistributions in binary form must reproduce the above
" copyright notice, this list of conditions and the following disclaimer
" in the documentation and/or other materials provided with the
" distribution.
"     * neither the name of google inc. nor the names of its
" contributors may be used to endorse or promote products derived from
" this software without specific prior written permission.
"
" this software is provided by the copyright holders and contributors
" "as is" and any express or implied warranties, including, but not
" limited to, the implied warranties of merchantability and fitness for
" a particular purpose are disclaimed. in no event shall the copyright
" owner or contributors be liable for any direct, indirect, incidental,
" special, exemplary, or consequential damages (including, but not
" limited to, procurement of substitute goods or services; loss of use,
" data, or profits; or business interruption) however caused and on any
" theory of liability, whether in contract, strict liability, or tort
" (including negligence or otherwise) arising in any way out of the use
" of this software, even if advised of the possibility of such damage.

" this is the vim syntax file for google protocol buffers.
"
" usage:
"
" 1. cp proto.vim ~/.vim/syntax/
" 2. add the following to ~/.vimrc:
"
" augroup filetype
"   au! bufread,bufnewfile *.proto setfiletype proto
" augroup end
"
" or just create a new file called ~/.vim/ftdetect/proto.vim with the
" previous lines on it.

if version < 600
  syntax clear
elseif exists("b:current_syntax")
  finish
endif

syn case match

syn keyword pbtodo       contained todo fixme xxx
syn cluster pbcommentgrp contains=pbtodo

syn keyword pbsyntax     syntax import option
syn keyword pbstructure  package message group
syn keyword pbrepeat     optional required repeated
syn keyword pbdefault    default
syn keyword pbextend     extend extensions to max
syn keyword pbrpc        service rpc returns

syn keyword pbtype      int32 int64 uint32 uint64 sint32 sint64
syn keyword pbtype      fixed32 fixed64 sfixed32 sfixed64
syn keyword pbtype      float double bool string bytes
syn keyword pbtypedef   enum
syn keyword pbbool      true false

syn match   pbint     /-\?\<\d\+\>/
syn match   pbint     /\<0[xx]\x+\>/
syn match   pbfloat   /\<-\?\d*\(\.\d*\)\?/
syn region  pbcomment start="\/\*" end="\*\/" contains=@pbcommentgrp
syn region  pbcomment start="//" skip="\\$" end="$" keepend contains=@pbcommentgrp
syn region  pbstring  start=/"/ skip=/\\./ end=/"/
syn region  pbstring  start=/'/ skip=/\\./ end=/'/

if version >= 508 || !exists("did_proto_syn_inits")
  if version < 508
    let did_proto_syn_inits = 1
    command -nargs=+ hilink hi link <args>
  else
    command -nargs=+ hilink hi def link <args>
  endif

  hilink pbtodo         todo

  hilink pbsyntax       include
  hilink pbstructure    structure
  hilink pbrepeat       repeat
  hilink pbdefault      keyword
  hilink pbextend       keyword
  hilink pbrpc          keyword
  hilink pbtype         type
  hilink pbtypedef      typedef
  hilink pbbool         boolean

  hilink pbint          number
  hilink pbfloat        float
  hilink pbcomment      comment
  hilink pbstring       string

  delcommand hilink
endif

let b:current_syntax = "proto"
