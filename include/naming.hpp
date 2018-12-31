#ifndef COOP_NAMING
#define COOP_NAMING

#include"clang/AST/Decl.h"
#include"clang/AST/Stmt.h"
#include"clang/AST/Expr.h"
#include"clang/AST/ASTContext.h"
#include"clang/Basic/SourceManager.h"
#include<sstream>
#include<string>

#include"Logger.hpp"

using namespace clang;


namespace coop{
namespace naming {
    const char * get_from_end_until(const char *file, const char delimiter);
    const char * get_from_start_until(const char *file, const char delimiter);
    const char * get_relevant_token(const char *file);
    std::string get_without(std::string, const char * without);

    template<typename T>
    std::string get_decl_id(const T *d){
        SourceManager &src_mgr = d->getASTContext().getSourceManager();

        std::string file_name = get_relevant_token(src_mgr.getFilename(d->getLocation()).str().c_str());

        std::stringstream dest;
        dest << "[" << d->getNameAsString() << ":" << file_name << ":"
            << src_mgr.getPresumedLineNumber(d->getLocStart()) << ":"
            << src_mgr.getPresumedColumnNumber(d->getLocStart()) << "]";
        //coop::logger::log_stream << "CREATED ID: " << dest.str();
        //coop::logger::out();
        return dest.str();
    }

    template<typename T>
    std::string get_stmt_id(const T *s, SourceManager *src_mgr){
        std::string file_name = get_relevant_token(src_mgr->getFilename(s->getLocStart()).str().c_str());

        std::stringstream dest;
        dest << "[stmt:" << file_name << ":"
            << src_mgr->getPresumedLineNumber(s->getLocStart()) << ":"
            << src_mgr->getPresumedColumnNumber(s->getLocStart()) << "]";
        //coop::logger::log_stream << "CREATED ID: " << dest.str();
        //coop::logger::out();
        return dest.str();

    }

    std::string get_for_loop_identifier(const ForStmt* loop, SourceManager *srcMgr);
    std::string get_while_loop_identifier(const WhileStmt* loop, SourceManager *srcMgr);

}//namespace naming
}//namespace coop

#endif