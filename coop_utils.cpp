#include "coop_utils.hpp"
#include "llvm-c/TargetMachine.h"


int coop::get_sizeof_in_bits(const FieldDecl* field){
    return field->getASTContext().getTypeSize(field->getType());
}
int coop::get_sizeof_in_byte(const FieldDecl* field){
    return get_sizeof_in_bits(field)/8;
}

void coop::record::record_info::init(
    const clang::RecordDecl* class_struct,
    std::vector<const clang::FieldDecl*> *field_vector,
    std::map<const clang::FunctionDecl*, std::vector<const clang::MemberExpr*>> *rlvnt_funcs,
    std::map<const Stmt*, loop_credentials> *rlvnt_loops)
    {

    record = class_struct;
    fields = *field_vector;

    fun_mem.init(fields.size(), rlvnt_funcs);
    loop_mem.init(fields.size(), rlvnt_loops);

    relevant_functions = rlvnt_funcs;
    relevant_loops = rlvnt_loops;

    //the fun_mem_mat will be written according to the indices the members are mapped to here
    //since a function can mention the same member several times, we need to make sure each
    //iteration over the same member associates with the same adress in the matrix (has the same index)
    int index_count = 0;
    field_weights.resize(fields.size());
    for(auto f : fields){
        field_idx_mapping[f] = index_count;

        std::pair<const FieldDecl*, float> f_w {f, 0};
        field_weights[index_count++] = f_w;
    }
}

std::vector<const MemberExpr*>* coop::record::record_info::isRelevantFunction(const clang::FunctionDecl* func){
    auto funcs_iter = relevant_functions->find(func);
    if(funcs_iter != relevant_functions->end()){
        return &funcs_iter->second;
    }
    return nullptr;
}

int coop::record::record_info::isRelevantField(const MemberExpr* memExpr){
    const FieldDecl* field = static_cast<const FieldDecl*>(memExpr->getMemberDecl());
    if(std::find(fields.begin(), fields.end(), field) != fields.end()){
        return field_idx_mapping[field];
    }
    return -1;
}

void coop::record::record_info::print_func_mem_mat(std::map<const FunctionDecl*, int> & mapping_reference){
    std::function<const char* (const FunctionDecl*)> getNam =
        [](const FunctionDecl* fd){ return fd->getNameAsString().c_str();};

    std::function<int (const FunctionDecl*)> getIdx =
        [&mapping_reference](const FunctionDecl* fd){
            return mapping_reference[fd];
    };
    print_mat(&fun_mem, getNam, getIdx);
}
void coop::record::record_info::print_loop_mem_mat(
                std::map<const Stmt*, coop::loop_credentials> &loop_reference,
                std::map<const Stmt*, int> &loop_idx_mapping_reference){

    std::function<const char* (const Stmt*)> getNam = [&loop_reference](const Stmt* ls){ 

        auto loop_iter = loop_reference.find(ls);
        if(loop_iter != loop_reference.end()){
            return loop_iter->second.identifier.c_str();
        }
        return "unidentified loop -> this is most likely a bug!";
    };

    std::function<int (const Stmt*)> getIdx = [&loop_idx_mapping_reference](const Stmt* ls){
        return loop_idx_mapping_reference[ls];
    };
    print_mat(&loop_mem, getNam, getIdx);
}