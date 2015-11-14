/*
 * copyright (c) 2011, peter thorson. all rights reserved.
 *
 * redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * neither the name of the websocket++ project nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 * 
 * this software is provided by the copyright holders and contributors "as is" 
 * and any express or implied warranties, including, but not limited to, the 
 * implied warranties of merchantability and fitness for a particular purpose 
 * are disclaimed. in no event shall peter thorson be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential damages
 * (including, but not limited to, procurement of substitute goods or services;
 * loss of use, data, or profits; or business interruption) however caused and
 * on any theory of liability, whether in contract, strict liability, or tort
 * (including negligence or otherwise) arising in any way out of the use of this
 * software, even if advised of the possibility of such damage.
 * 
 */

#ifndef wscmd_hpp
#define wscmd_hpp

#include <map>
#include <string>

namespace wscmd {
    // parses a wscmd string.
    
    // command structure
    // command:arg1=val1;arg2=val2;arg3=val3;
    
    // commands
    // ack: messages to ack
    // example: `ack:e3458d0aceff8b70a3e5c0afec632881=38;e3458d0aceff8b70a3e5c0afec632881=42;`
    
    // send: [vals]
    //       message; opcode=x; payload="x"
    //       frame; [fuzzer stuff]
    
    // close:code=1000;reason=msg;
    // (instructs the opposite end to close with given optional code/msg)
    typedef std::map<std::string,std::string> arg_list;
    
    struct cmd {
        // todo: move semantics
        std::string command;
        arg_list args;
    };
    
    wscmd::cmd parse(const std::string& m);
    
    wscmd::cmd parse(const std::string& m) {
        cmd command;
        std::string::size_type start;
        std::string::size_type end;
        
        start = m.find(":",0);
        
        if (start != std::string::npos) {
            command.command = m.substr(0,start);
            
            start++; // skip the colon
            end = m.find(";",start);
            
            // find all semicolons
            while (end != std::string::npos) {
                std::string arg;
                std::string val;
                
                std::string::size_type sep = m.find("=",start);
                
                if (sep != std::string::npos) {
                    arg = m.substr(start,sep-start);
                    val = m.substr(sep+1,end-sep-1);
                } else {
                    arg = m.substr(start,end-start);
                    val = "";
                }
                
                command.args[arg] = val;
                
                start = end+1;
                end = m.find(";",start);
            }
        }
        
        return command;
    }
} // namespace wscmd

#endif // wscmd_hpp
