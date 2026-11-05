#include "GlFuncs.h"

#define GL_DEFINE_FUNCTION(type, name) type name = nullptr;
GL_FUNCTION_LIST(GL_DEFINE_FUNCTION)
#undef GL_DEFINE_FUNCTION

bool loadGlFunctions(void* (*getProc)(const char*), std::string* missing) {
    bool all = true;
#define GL_LOAD_FUNCTION(type, name)                                   \
    name = reinterpret_cast<type>(getProc(#name));                     \
    if (name == nullptr) {                                             \
        all = false;                                                   \
        if (missing != nullptr) {                                      \
            *missing += std::string(#name) + " ";                      \
        }                                                              \
    }
    GL_FUNCTION_LIST(GL_LOAD_FUNCTION)
#undef GL_LOAD_FUNCTION
    return all;
}
