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
    SourceManager *src_mgr = nullptr;
    inline bool operator==(const ptr_ID<T> &other){
        return other.ptr == ptr;
    }
    inline void take(const ptr_ID<T> &other){
        ptr = other.ptr;
        id = other.id;
        src_mgr = other.src_mgr;
    }
    ptr_ID(const T *p, std::string p_id, SourceManager *sm):ptr(p),id(p_id),src_mgr(sm){}
};

template<typename T>
class global {
public:
    static std::vector<ptr_ID<T>> ptr_id;
    static ptr_ID<T> * get_global(std::string id){
        for(auto &p_i : ptr_id){
            if(p_i.id == id){
                return &p_i;
            }
        }
        return nullptr;
    }
    static ptr_ID<T> * get_global(const T *ptr){
        return get_global(coop::naming::get_decl_id<T>(ptr));
    }
    static ptr_ID<T> * get_global_by_ptr(const T *ptr){
        for(auto &p_i : ptr_id){
            if(p_i.ptr == ptr){
                return &p_i;
            }
        }
        return nullptr;
    }


    static ptr_ID<T> * set_global(const T *ptr, std::string id, SourceManager *sm){
        ptr_id.push_back(ptr_ID<T>(ptr, id, sm));
        return get_global(id);
    }
    static ptr_ID<T> * set_global(const T *ptr, std::string id){
        return set_global(ptr, id, &ptr->getASTContext().getSourceManager());
    }
    static ptr_ID<T> * set_global(const T *ptr){
        return set_global(ptr, coop::naming::get_decl_id<T>(ptr));
    }


    //use will look for a global version - return it if it exists and register it if not
    //so it will always return a pointer to an existing ptr_ID<T>
    static ptr_ID<T> * use(const T *ptr, std::string id, SourceManager *src_mgr){
        ptr_ID<T> *ret = get_global(id);
        if(!ret){
            return set_global(ptr, id, src_mgr);
        }
        return ret;
    }
    static ptr_ID<T> * use(const T *ptr, std::string id){
        return use(ptr, id, &ptr->getASTContext().getSourceManager());
    }
    static ptr_ID<T> * use(const T *ptr){
        return use(ptr, coop::naming::get_decl_id<T>(ptr));
    }
};

template<typename T>
std::vector<ptr_ID<T>> global<T>::ptr_id = {};

}//namespace coop

#endif