// protocol buffers - google's data interchange format
// copyright 2008 google inc.  all rights reserved.
// http://code.google.com/p/protobuf/
//
// redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * neither the name of google inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// this software is provided by the copyright holders and contributors
// "as is" and any express or implied warranties, including, but not
// limited to, the implied warranties of merchantability and fitness for
// a particular purpose are disclaimed. in no event shall the copyright
// owner or contributors be liable for any direct, indirect, incidental,
// special, exemplary, or consequential damages (including, but not
// limited to, procurement of substitute goods or services; loss of use,
// data, or profits; or business interruption) however caused and on any
// theory of liability, whether in contract, strict liability, or tort
// (including negligence or otherwise) arising in any way out of the use
// of this software, even if advised of the possibility of such damage.

// author: jschorr@google.com (joseph schorr)
//  based on original protocol buffers design by
//  sanjay ghemawat, jeff dean, and others.
//
// utilities for printing and parsing protocol messages in a human-readable,
// text-based format.

#ifndef google_protobuf_text_format_h__
#define google_protobuf_text_format_h__

#include <map>
#include <string>
#include <vector>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/message.h>
#include <google/protobuf/descriptor.h>

namespace google {
namespace protobuf {

namespace io {
  class errorcollector;      // tokenizer.h
}

// this class implements protocol buffer text format.  printing and parsing
// protocol messages in text format is useful for debugging and human editing
// of messages.
//
// this class is really a namespace that contains only static methods.
class libprotobuf_export textformat {
 public:
  // outputs a textual representation of the given message to the given
  // output stream.
  static bool print(const message& message, io::zerocopyoutputstream* output);

  // print the fields in an unknownfieldset.  they are printed by tag number
  // only.  embedded messages are heuristically identified by attempting to
  // parse them.
  static bool printunknownfields(const unknownfieldset& unknown_fields,
                                 io::zerocopyoutputstream* output);

  // like print(), but outputs directly to a string.
  static bool printtostring(const message& message, string* output);

  // like printunknownfields(), but outputs directly to a string.
  static bool printunknownfieldstostring(const unknownfieldset& unknown_fields,
                                         string* output);

  // outputs a textual representation of the value of the field supplied on
  // the message supplied. for non-repeated fields, an index of -1 must
  // be supplied. note that this method will print the default value for a
  // field if it is not set.
  static void printfieldvaluetostring(const message& message,
                                      const fielddescriptor* field,
                                      int index,
                                      string* output);

  // class for those users which require more fine-grained control over how
  // a protobuffer message is printed out.
  class libprotobuf_export printer {
   public:
    printer();
    ~printer();

    // like textformat::print
    bool print(const message& message, io::zerocopyoutputstream* output) const;
    // like textformat::printunknownfields
    bool printunknownfields(const unknownfieldset& unknown_fields,
                            io::zerocopyoutputstream* output) const;
    // like textformat::printtostring
    bool printtostring(const message& message, string* output) const;
    // like textformat::printunknownfieldstostring
    bool printunknownfieldstostring(const unknownfieldset& unknown_fields,
                                    string* output) const;
    // like textformat::printfieldvaluetostring
    void printfieldvaluetostring(const message& message,
                                 const fielddescriptor* field,
                                 int index,
                                 string* output) const;

    // adjust the initial indent level of all output.  each indent level is
    // equal to two spaces.
    void setinitialindentlevel(int indent_level) {
      initial_indent_level_ = indent_level;
    }

    // if printing in single line mode, then the entire message will be output
    // on a single line with no line breaks.
    void setsinglelinemode(bool single_line_mode) {
      single_line_mode_ = single_line_mode;
    }

    // set true to print repeated primitives in a format like:
    //   field_name: [1, 2, 3, 4]
    // instead of printing each value on its own line.  short format applies
    // only to primitive values -- i.e. everything except strings and
    // sub-messages/groups.
    void setuseshortrepeatedprimitives(bool use_short_repeated_primitives) {
      use_short_repeated_primitives_ = use_short_repeated_primitives;
    }

    // set true to output utf-8 instead of ascii.  the only difference
    // is that bytes >= 0x80 in string fields will not be escaped,
    // because they are assumed to be part of utf-8 multi-byte
    // sequences.
    void setuseutf8stringescaping(bool as_utf8) {
      utf8_string_escaping_ = as_utf8;
    }

   private:
    // forward declaration of an internal class used to print the text
    // output to the outputstream (see text_format.cc for implementation).
    class textgenerator;

    // internal print method, used for writing to the outputstream via
    // the textgenerator class.
    void print(const message& message,
               textgenerator& generator) const;

    // print a single field.
    void printfield(const message& message,
                    const reflection* reflection,
                    const fielddescriptor* field,
                    textgenerator& generator) const;

    // print a repeated primitive field in short form.
    void printshortrepeatedfield(const message& message,
                                 const reflection* reflection,
                                 const fielddescriptor* field,
                                 textgenerator& generator) const;

    // print the name of a field -- i.e. everything that comes before the
    // ':' for a single name/value pair.
    void printfieldname(const message& message,
                        const reflection* reflection,
                        const fielddescriptor* field,
                        textgenerator& generator) const;

    // outputs a textual representation of the value of the field supplied on
    // the message supplied or the default value if not set.
    void printfieldvalue(const message& message,
                         const reflection* reflection,
                         const fielddescriptor* field,
                         int index,
                         textgenerator& generator) const;

    // print the fields in an unknownfieldset.  they are printed by tag number
    // only.  embedded messages are heuristically identified by attempting to
    // parse them.
    void printunknownfields(const unknownfieldset& unknown_fields,
                            textgenerator& generator) const;

    int initial_indent_level_;

    bool single_line_mode_;

    bool use_short_repeated_primitives_;

    bool utf8_string_escaping_;
  };

  // parses a text-format protocol message from the given input stream to
  // the given message object.  this function parses the format written
  // by print().
  static bool parse(io::zerocopyinputstream* input, message* output);
  // like parse(), but reads directly from a string.
  static bool parsefromstring(const string& input, message* output);

  // like parse(), but the data is merged into the given message, as if
  // using message::mergefrom().
  static bool merge(io::zerocopyinputstream* input, message* output);
  // like merge(), but reads directly from a string.
  static bool mergefromstring(const string& input, message* output);

  // parse the given text as a single field value and store it into the
  // given field of the given message. if the field is a repeated field,
  // the new value will be added to the end
  static bool parsefieldvaluefromstring(const string& input,
                                        const fielddescriptor* field,
                                        message* message);

  // interface that textformat::parser can use to find extensions.
  // this class may be extended in the future to find more information
  // like fields, etc.
  class libprotobuf_export finder {
   public:
    virtual ~finder();

    // try to find an extension of *message by fully-qualified field
    // name.  returns null if no extension is known for this name or number.
    virtual const fielddescriptor* findextension(
        message* message,
        const string& name) const = 0;
  };

  // a location in the parsed text.
  struct parselocation {
    int line;
    int column;

    parselocation() : line(-1), column(-1) {}
    parselocation(int line_param, int column_param)
        : line(line_param), column(column_param) {}
  };

  // data structure which is populated with the locations of each field
  // value parsed from the text.
  class libprotobuf_export parseinfotree {
   public:
    parseinfotree();
    ~parseinfotree();

    // returns the parse location for index-th value of the field in the parsed
    // text. if none exists, returns a location with line = -1. index should be
    // -1 for not-repeated fields.
    parselocation getlocation(const fielddescriptor* field, int index) const;

    // returns the parse info tree for the given field, which must be a message
    // type. the nested information tree is owned by the root tree and will be
    // deleted when it is deleted.
    parseinfotree* gettreefornested(const fielddescriptor* field,
                                    int index) const;

   private:
    // allow the text format parser to record information into the tree.
    friend class textformat;

    // records the starting location of a single value for a field.
    void recordlocation(const fielddescriptor* field, parselocation location);

    // create and records a nested tree for a nested message field.
    parseinfotree* createnested(const fielddescriptor* field);

    // defines the map from the index-th field descriptor to its parse location.
    typedef map<const fielddescriptor*, vector<parselocation> > locationmap;

    // defines the map from the index-th field descriptor to the nested parse
    // info tree.
    typedef map<const fielddescriptor*, vector<parseinfotree*> > nestedmap;

    locationmap locations_;
    nestedmap nested_;

    google_disallow_evil_constructors(parseinfotree);
  };

  // for more control over parsing, use this class.
  class libprotobuf_export parser {
   public:
    parser();
    ~parser();

    // like textformat::parse().
    bool parse(io::zerocopyinputstream* input, message* output);
    // like textformat::parsefromstring().
    bool parsefromstring(const string& input, message* output);
    // like textformat::merge().
    bool merge(io::zerocopyinputstream* input, message* output);
    // like textformat::mergefromstring().
    bool mergefromstring(const string& input, message* output);

    // set where to report parse errors.  if null (the default), errors will
    // be printed to stderr.
    void recorderrorsto(io::errorcollector* error_collector) {
      error_collector_ = error_collector;
    }

    // set how parser finds extensions.  if null (the default), the
    // parser will use the standard reflection object associated with
    // the message being parsed.
    void setfinder(finder* finder) {
      finder_ = finder;
    }

    // sets where location information about the parse will be written. if null
    // (the default), then no location will be written.
    void writelocationsto(parseinfotree* tree) {
      parse_info_tree_ = tree;
    }

    // normally parsing fails if, after parsing, output->isinitialized()
    // returns false.  call allowpartialmessage(true) to skip this check.
    void allowpartialmessage(bool allow) {
      allow_partial_ = allow;
    }

    // like textformat::parsefieldvaluefromstring
    bool parsefieldvaluefromstring(const string& input,
                                   const fielddescriptor* field,
                                   message* output);


   private:
    // forward declaration of an internal class used to parse text
    // representations (see text_format.cc for implementation).
    class parserimpl;

    // like textformat::merge().  the provided implementation is used
    // to do the parsing.
    bool mergeusingimpl(io::zerocopyinputstream* input,
                        message* output,
                        parserimpl* parser_impl);

    io::errorcollector* error_collector_;
    finder* finder_;
    parseinfotree* parse_info_tree_;
    bool allow_partial_;
    bool allow_unknown_field_;
  };

 private:
  // hack: parseinfotree declares textformat as a friend which should extend
  // the friendship to textformat::parser::parserimpl, but unfortunately some
  // old compilers (e.g. gcc 3.4.6) don't implement this correctly. we provide
  // helpers for parserimpl to call methods of parseinfotree.
  static inline void recordlocation(parseinfotree* info_tree,
                                    const fielddescriptor* field,
                                    parselocation location);
  static inline parseinfotree* createnested(parseinfotree* info_tree,
                                            const fielddescriptor* field);

  google_disallow_evil_constructors(textformat);
};

inline void textformat::recordlocation(parseinfotree* info_tree,
                                       const fielddescriptor* field,
                                       parselocation location) {
  info_tree->recordlocation(field, location);
}

inline textformat::parseinfotree* textformat::createnested(
    parseinfotree* info_tree, const fielddescriptor* field) {
  return info_tree->createnested(field);
}

}  // namespace protobuf

}  // namespace google
#endif  // google_protobuf_text_format_h__
