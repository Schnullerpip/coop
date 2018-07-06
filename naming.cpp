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

    std::string get_for_loop_identifier(const ForStmt* loop, SourceManager *srcMgr){
        std::stringstream dest;
        const char *fileName = get_relevant_token(srcMgr->getFilename(loop->getForLoc()).str().c_str());
        dest << "[FLoop:" << fileName << ":" << srcMgr->getPresumedLineNumber(loop->getLocStart())
        << ":" << srcMgr->getPresumedColumnNumber(loop->getLocStart())
        << "]";
        return dest.str();
    }
    std::string get_while_loop_identifier(const WhileStmt* loop, SourceManager *srcMgr){
        std::stringstream dest;
        const char *fileName = get_relevant_token(srcMgr->getFilename(loop->getWhileLoc()).str().c_str());
        dest << "[WLoop:" << fileName << ":" << srcMgr->getPresumedLineNumber(loop->getLocStart())
        << ":" << srcMgr->getPresumedColumnNumber(loop->getLocStart())
        << "]";
        return dest.str();
    }

}//namespace naming
}//namespace coop