#ifndef COOP_UTILS_HPP
#define COOP_UTILS_HPP

#define COOP_PATH_NAME_S "COOP"
#define COOP_TEMPLATES_PATH_NAME_S "COOP_TEMPLATES"

// Declares clang::SyntaxOnlyAction.
#include"clang/Frontend/FrontendActions.h"
#include"clang/Tooling/CommonOptionsParser.h"
#include"clang/Tooling/Tooling.h"
// Declares llvm::cl::extrahelp.
#include"llvm/Support/CommandLine.h"
#include"clang/ASTMatchers/ASTMatchers.h"
#include"clang/ASTMatchers/ASTMatchFinder.h"
#include"clang/Frontend/CompilerInstance.h"
#include"clang/AST/ASTContext.h"
#include"clang/Rewrite/Core/Rewriter.h"

#include<set>
#include<ctime>
#include<string>
#include<stdlib.h>
#include<sstream>
#include<iostream>
#include<functional>

#include "Logger.hpp"

using namespace clang::tooling;
using namespace llvm;

using namespace clang;
using namespace clang::ast_matchers;

//macros that can easily be refactored
#define coop_class_s "class"
#define coop_access_s "access_decl"
#define coop_member_s "member"
#define coop_function_s "function"
#define coop_function_call_s "functionCall"

#define coop_loop_s "loop"
#define coop_for_loop_s "for_loop"
#define coop_while_loop_s "while_loop"

#define coop_child_s "child"
#define coop_parent_s "parent"

#define coop_parent_loop_s "parent_loop"
#define coop_child_for_loop_s "child_for_loop_s"
#define coop_child_while_loop_s "child_while_loop_s"

#define coop_destructor_s "destructor"
#define coop_new_instantiation_s "new_usage"
#define coop_deletion_s "delete"

#define coop_constructor_s "constructor"
#define coop_array_idx_s "array_idx"


namespace coop{

    //will return the sizeof value for a field in Bits
    size_t get_sizeof_in_bits(const FieldDecl* field);
    //will return the sizeof value for a field in Byte
    size_t get_sizeof_in_byte(const FieldDecl* field);

    //will return the value of an environmentvariable - or "" 
    std::string getEnvVar( std::string const &);

    template <class T, typename P>
    struct data_matrix {
        //the matrix mapping which T uses which P 
        /*the setup will be e.g.
            m_a, m_b, m_c, m_d
        f1  x    x        
        f2  x         x
        f3
        f4            x    x
        */
        float *mat;

        //the matrix's dimension
        size_t dim;

        //the reference to the relevant instances referring to this matrix
        std::map<const T*, P>
            *relevant_instances;
    private:
        size_t fields;


    public:
        void init(size_t num_fields, std::map<const T*, P> *ref){
            fields = num_fields;
            dim = fields * ref->size();
            mat = static_cast<float*>(calloc(dim, sizeof(float)));
            relevant_instances = ref;
        }

        inline float& at(int x, int y){
            return mat[y * fields + x];
        }
    };

    //POD for loop information
    struct loop_credentials{
        std::vector<const FunctionDecl*> funcs;
        std::vector<const MemberExpr*> member_usages;
        bool isForLoop;
        std::string identifier;
    };

    namespace record {

        /*will hold a lot of information on a record's members 
          specifically on: 
          - in which functions are my members mentioned?
          - in which loops are my members mentioned?
          
          so an analyzis of this information has a centralized reference
          It will hold node-pointers referring to the AST, so additional information or a rewriter instnace
          can easily access more data depending on what is needed*/
        struct record_info {
            void init(
                const clang::CXXRecordDecl* class_struct,
                std::set<const clang::FieldDecl*> *field_vector,
                std::map<const clang::FunctionDecl*, std::vector<const clang::MemberExpr*>> *rlvnt_funcs,
                std::map<const Stmt*, loop_credentials> *rlvnt_loops);

            ~record_info(){
                free(fun_mem.mat);
                free(loop_mem.mat);
            }


            //reference to the record node (class/struct) that is referred to by this struct
            const clang::CXXRecordDecl
                *record;
            //reference to all the member nodes that the referred record has
            std::set<const clang::FieldDecl*>
                fields;

            //reference to the function-member mapping
            std::map<const clang::FunctionDecl*, std::vector<const clang::MemberExpr*>>
                *relevant_functions;
            //reference to the loop-member mapping
            std::map<const clang::Stmt*, loop_credentials>
                *relevant_loops;

            //the matrix mapping which function uses which member of the class
            data_matrix<clang::FunctionDecl, std::vector<const MemberExpr*>>
                fun_mem;
            //the matrix mapping which loop uses which member of the class
            data_matrix<clang::Stmt, loop_credentials>
                loop_mem;

            //will associate each member with a consistent index
            std::map<const clang::FieldDecl*, int>
                field_idx_mapping;
            //will hold the field weight for each field
            std::vector<std::pair<const clang::FieldDecl*, float>>
                field_weights;
            //will hold the pointers to the cold fields
            std::vector<const clang::FieldDecl*>
                cold_fields;
            //will hold the pointers to the hot fields
            std::vector<const clang::FieldDecl*>
                hot_fields;

            //will hold the body of the record's destructor (if any) -> must be set manually!
            const CXXDestructorDecl *destructor_ptr = nullptr;

            //returns a list of members associated by a function for this record - if it does; else nullptr
            std::vector<const MemberExpr*>* isRelevantFunction(const clang::FunctionDecl* func);
            
            //returns the FieldDecl*s idx if the member is relevant to this record, else a negative value
            int isRelevantField(const MemberExpr* memExpr);

            void print_func_mem_mat(std::map<const FunctionDecl*, int> &mapping_reference);
            void print_loop_mem_mat(
                std::map<const Stmt*, coop::loop_credentials> &loop_reference,
                std::map<const Stmt*, int> &loop_idx_mapping_reference
            );

        private:
            template<typename T, typename P>
            void print_mat (data_matrix<T, P>* mat,
                std::function<const char* (const T*)>& getName,
                std::function<int (const T*)>& getIdx){
                coop::logger::depth++;
                for(auto f : fields){
                    logger::log_stream << " " << f->getNameAsString().c_str() << "\t";
                }
                logger::out();
                for(auto t : *mat->relevant_instances){
                    logger::log_stream << "[";
                    bool has_an_entry = false;
                    for(size_t o = 0; o < fields.size(); ++o){
                        float value_at = mat->at(o, getIdx(t.first));
                        if(value_at == 0)
                        {
                            logger::log_stream << " ";
                        }else{
                            logger::log_stream << value_at;
                            has_an_entry = true;
                        }
                        logger::log_stream << "\t";
                        if(o == fields.size()-1){
                            if(has_an_entry){
                                logger::log_stream << "] " << getName(t.first);
                                logger::out();
                            }else{
                                coop::logger::clear();
                            }
                        }else{
                            logger::log_stream << ",";
                        }
                    }
                }
                coop::logger::depth--;
            }
        };
    }
}

namespace coop{
struct weight_size {
    float weight;
    size_t size_in_byte;
    size_t alignment_requirement;
};

struct SGroup
{
    SGroup(unsigned int start, unsigned int end):start_idx(start), end_idx(end){}
    //actually copy the weight_size pairs from the container into the group so
    //forthgoing we no longer only work with the indices but the actual data
    void finalize(std::vector<coop::weight_size> &weights);
    void print(bool recursive = false);
    std::string get_string();

    std::vector<weight_size> weights_and_sizes;
    unsigned int start_idx = 0, end_idx = 0;
    unsigned int type_size = 0;
    float highest_field_weight = 0;
    //this field will hold the information to what extend the program would benefit from 
    //splitting the original record starting with this very group
    //The final split will be made at the group with the highest split-value if positive
    double split_value = -1;
    SGroup *next = nullptr, *prev=nullptr;
};

SGroup * find_significance_groups(coop::weight_size *elements, unsigned int offset, unsigned int number_elements);

//determine the size of a set of groups regarding structure padding
//until -> inclusive
size_t determine_size_with_optimal_padding(SGroup *begin, SGroup *until, size_t additional_field_size = 0);
size_t determine_size_with_padding(const clang::CXXRecordDecl *rec_decl);
}//namespace coop

#endif