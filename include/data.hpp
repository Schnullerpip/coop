#ifndef COOP_DATA
#define COOP_DATA

#include"clang/AST/Decl.h"
#include"clang/AST/Expr.h"
#include"clang/AST/Stmt.h"
#include"naming.hpp"
#include"Logger.hpp"

#include<set>
#include<string>

using namespace clang;

namespace coop{

class unique {
    std::set<std::string> ids = {};
public:
    //returns true if id was found, else false
    inline bool check(std::string id){
        if(ids.find(id) != ids.end()){ return true; }
        else{ ids.insert(id); return false; }
    }
};

template<typename T>
struct ptr_ID{
    const T *ptr = nullptr;
    std::string id = "";
    ASTContext *ast_context = nullptr;

    inline bool operator==(const ptr_ID<T> &other){
        return other.ptr == ptr;
    }
    //inline void take(const ptr_ID<T> &other){
    //    coop::logger::log_stream << "bending ptr:" << ptr << " to ptr:" << other.ptr;
    //    coop::logger::out();
    //    ptr = other.ptr;
    //    id = other.id;
    //    ast_context = other.ast_context;
    //}
    ptr_ID(const T *p, std::string p_id, ASTContext *ast_ctxt):ptr(p),id(p_id),ast_context(ast_ctxt){}
};

template<typename T>
class global {
public:
    static std::vector<ptr_ID<T>> ptr_id;
    static ptr_ID<T> * get_global(std::string id){
        for(size_t i = 0; i < ptr_id.size(); ++i){
            if(ptr_id[i].id == id){
                return &ptr_id[i];
            }
        }
        return nullptr;
    }
    static ptr_ID<T> * get_global(const T *ptr){
        return get_global(coop::naming::get_decl_id<T>(ptr));
    }
    static ptr_ID<T> * get_global_by_ptr(const T *ptr){
        for(size_t i = 0; i < ptr_id.size(); ++i){
            //coop::logger::log_stream << "comparing " << ptr << " and " << ptr_id[i].ptr;
            if(ptr_id[i].ptr == ptr){
                //coop::logger::log_stream << " - match";
                //coop::logger::out();
                return &ptr_id[i];
            }
            //coop::logger::out();
        }
        return nullptr;
    }


    static ptr_ID<T> * set_global(const T *ptr, std::string id, ASTContext *ast_context){
        ptr_id.push_back(ptr_ID<T>(ptr, id, ast_context));
        //coop::logger::log_stream << "[NEW ID]::" << id;
        //coop::logger::out();
        return get_global(id);
    }
    static ptr_ID<T> * set_global(const T *ptr, std::string id){
        return set_global(ptr, id, &ptr->getASTContext());
    }
    static ptr_ID<T> * set_global(const T *ptr){
        return set_global(ptr, coop::naming::get_decl_id<T>(ptr));
    }


    //use will look for a global version - return it if it exists and register it if not
    //so it will always return a pointer to an existing ptr_ID<T>
    static ptr_ID<T> * use(const T *ptr, std::string id, ASTContext *ast_context){
        ptr_ID<T> *ret = get_global(id);
        if(!ret){
            return set_global(ptr, id, ast_context);
        }
        return ret;
    }
    static ptr_ID<T> * use(const T *ptr, std::string id){
        return use(ptr, id, &ptr->getASTContext());
    }
    static ptr_ID<T> * use(const T *ptr){
        return use(ptr, coop::naming::get_decl_id<T>(ptr));
    }
};

template<typename T>
std::vector<ptr_ID<T>> global<T>::ptr_id = {};



//compositum to abbreviate the AST
//will be used to replicate the relevant parts of the AST in a simplified way, storing only functions/loops and their relevant children
struct fl_node {
    //CLEAN THESE UP!!!!!!! TODO!!!!
    static std::map<const FunctionDecl *, fl_node *> AST_abbreviation_func;
    static std::map<const Stmt *, fl_node *> AST_abbreviation_loop;

    fl_node(ptr_ID<FunctionDecl> *f):id(f->id), is_loop(false){}
    fl_node(ptr_ID<Stmt> *f):id(f->id), is_loop(true){}

    std::string ID(){return id;}
    bool isLoop(){return is_loop;}
    bool isFunc(){return !is_loop;}

    std::vector<fl_node*> children;
    std::vector<fl_node*> parents;

private:
    const std::string id;
    bool is_loop;
};



}//namespace coop

#endif