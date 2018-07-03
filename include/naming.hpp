#ifndef COOP_NAMING
#define COOP_NAMING

#include"clang/AST/Decl.h"
#include"clang/AST/Stmt.h"
#include"clang/AST/Expr.h"
#include"clang/AST/ASTContext.h"
#include"clang/Basic/SourceManager.h"
#include<sstream>
#include<string>

using namespace clang;


namespace coop{
namespace naming {
    const char * get_relevant_token(const char *file);

    template<typename T>
    std::string get_decl_id(const T *d){
        SourceManager &src_mgr = d->getASTContext().getSourceManager();

        std::string file_name = get_relevant_token(src_mgr.getFilename(d->getLocation()).str().c_str());

        std::stringstream dest;
        dest << "[rec:" << file_name << ":"
            << src_mgr.getPresumedLineNumber(d->getLocStart()) << ":"
            << src_mgr.getPresumedColumnNumber(d->getLocStart()) << "]";
        return dest.str();
    }

    template<typename T>
    std::string get_stmt_id(const T *s, SourceManager *src_mgr){
        std::string file_name = get_relevant_token(src_mgr->getFilename(s->getLocStart()).str().c_str());

        std::stringstream dest;
        dest << "[stmt:" << file_name << ":"
            << src_mgr->getPresumedLineNumber(s->getLocStart()) << ":"
            << src_mgr->getPresumedColumnNumber(s->getLocStart()) << "]";
        return dest.str();

    }
}//namespace naming
}//namespace coop

#endif