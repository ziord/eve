// Generated automatically. Do not edit.

#ifndef EVE_INC_H
#define EVE_INC_H

// builtins 
#define BUILTINS_SRC_INC \
"struct Error {\n"  \
"    @compose message;\n"  \
"    @declare new => fn (msg) {\n"  \
"        return Error {message = msg};\n"  \
"    };\n"  \
"}\n"  \
"\n"  \
"struct StopIteration {}\n"   

#endif  //EVE_INC_H
