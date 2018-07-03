#ifndef COOP_DATA
#define COOP_DATA

#include"clang/AST/Decl.h"
#include"clang/AST/Expr.h"
#include"clang/AST/Stmt.h"
#include"naming.hpp"

#include<vector>
#include<string>

using namespace clang;

namespace coop{

class unique {
    std::set<std::string> ids = {};
public:
    inline bool check(std::string id){
        if(ids.find(id) != ids.end()){ return false; }
        else{ ids.insert(id); return true; }
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
};

template<typename T>
class global {
    static std::vector<ptr_ID<T>> ptr_id;
public:
    //global(){ ptr_id = {}; }
    static const ptr_ID<T> * get_global(std::string id){
        for(auto &p_i : ptr_id){
            if(p_i.id == id){
                return &p_i;
            }
        }
        return nullptr;
    }
    static const ptr_ID<T> * get_global(const T *ptr){
        std::string id = coop::naming::get_decl_id<T>(ptr);
        return get_global(id);
    }
    static const ptr_ID<T> * set_global(const T *ptr, std::string id){
        ptr_ID<T> p;
        p.ptr = ptr;
        p.id = id;
        p.src_mgr = &ptr->getASTContext().getSourceManager();
        ptr_id.push_back(p);
        return get_global(id);
    }
    static const ptr_ID<T> * use(const T *ptr, std::string id){
        const ptr_ID<T> *ret = get_global(id);
        if(!ret){
            return set_global(ptr, id);
        }
        return ret;
    }
    static const ptr_ID<T> * use(const T *ptr){
        std::string id = coop::naming::get_decl_id<T>(ptr);
        return use(ptr, id);
    }
};

template<typename T>
std::vector<ptr_ID<T>> global<T>::ptr_id = {};

}//namespace coop

#endif