#include"naming.hpp"
#include"Logger.hpp"

namespace coop {
namespace naming{

    const char * get_from_end_until(const char *file, const char delimiter)
    {
        const char *relevant_token;
        size_t iterations = 0, file_length = strlen(file);
        for(relevant_token = file+strlen(file); *(relevant_token-1) != delimiter && iterations < file_length; --relevant_token){
            ++iterations;
        }
        return relevant_token;
    }

    const char * get_from_start_until(const char *file, const char delimiter)
    {
        const char *relevant_token;
        size_t iterations = 0, file_length = strlen(file);
        for(relevant_token = file; *relevant_token != delimiter && (iterations < file_length); ++relevant_token){
            ++iterations;
        }
        return ++relevant_token;
    }

    const char * get_relevant_token(const char *file)
    {
        return get_from_end_until(file, '/');
    }

    std::string get_without(std::string s, const char * without)
    {
        s.replace(s.find(without), strlen(without), "");
        return s;
    }

    std::string get_for_loop_identifier(const ForStmt* loop, SourceManager *srcMgr)
    {
        std::stringstream dest;
        const char *fileName = get_relevant_token(srcMgr->getFilename(loop->getForLoc()).str().c_str());
        dest << "[FLoop:" << fileName << ":" << srcMgr->getPresumedLineNumber(loop->getLocStart())
        << ":" << srcMgr->getPresumedColumnNumber(loop->getLocStart())
        << "]";
        return dest.str();
    }
    std::string get_while_loop_identifier(const WhileStmt* loop, SourceManager *srcMgr)
    {
        std::stringstream dest;
        const char *fileName = get_relevant_token(srcMgr->getFilename(loop->getWhileLoc()).str().c_str());
        dest << "[WLoop:" << fileName << ":" << srcMgr->getPresumedLineNumber(loop->getLocStart())
        << ":" << srcMgr->getPresumedColumnNumber(loop->getLocStart())
        << "]";
        return dest.str();
    }

}//namespace naming
}//namespace coop