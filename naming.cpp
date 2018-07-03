#include"naming.hpp"

namespace coop {
namespace naming{

    const char * get_relevant_token(const char *file)
    {
        const char *relevant_token;
        size_t iterations = 0;
        for(relevant_token = file+strlen(file); *(relevant_token-1) != '/' && *(relevant_token-1) != '\\' && iterations < strlen(file); --relevant_token){
            ++iterations;
        }
        return relevant_token;
    }
}//namespace naming
}//namespace coop