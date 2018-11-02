#include"data.hpp"

using namespace clang;

namespace coop{
    global<RecordDecl> g_records;
    global<FieldDecl> g_fields;
    global<FunctionDecl> g_functions;
    global<MemberExpr> g_memExprs;
    global<Stmt> g_stmts;

    std::map<const FunctionDecl *, fl_node *>
        fl_node::AST_abbreviation_func = {};

    std::map<const Stmt *, fl_node *>
        fl_node::AST_abbreviation_loop = {};

}//namespace coop