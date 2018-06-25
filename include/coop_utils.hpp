#ifndef COOP_UTILS_HPP
#define COOP_UTILS_HPP


// Declares clang::SyntaxOnlyAction.
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
// Declares llvm::cl::extrahelp.
#include "llvm/Support/CommandLine.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/AST/ASTContext.h"
#include "clang/Rewrite/Core/Rewriter.h"

#include <ctime>
#include <string>
#include <stdlib.h>
#include <sstream>
#include <iostream>
#include <functional>

#include "Logger.hpp"

using namespace clang::tooling;
using namespace llvm;

using namespace clang;
using namespace clang::ast_matchers;

//macros that can easily be refactored
#define coop_class_s "class"
#define coop_member_s "member"
#define coop_function_s "function"
#define coop_function_call_s "functionCall"

#define coop_loop_s "loop"
#define coop_for_loop_s "for_loop"
#define coop_while_loop_s "while_loop"

#define coop_parent_loop_s "parent_loop"
#define coop_child_for_loop_s "child_for_loop_s"
#define coop_child_while_loop_s "child_while_loop_s"

#define coop_destructor_s "destructor"
#define coop_new_instantiation_s "new_usage"
#define coop_deletion_s "delete"


namespace coop{

    //will return the sizeof value for a field in Bits
    int get_sizeof_in_bits(const FieldDecl* field);
    //will return the sizeof value for a field in Byte
    int get_sizeof_in_byte(const FieldDecl* field);

    namespace match {
        extern DeclarationMatcher classes;
		extern DeclarationMatcher members;
		extern StatementMatcher members_used_in_functions;

        extern StatementMatcher loops;
        extern StatementMatcher function_calls_in_loops;
        extern StatementMatcher nested_loops;

        extern StatementMatcher members_used_in_for_loops;
        extern StatementMatcher members_used_in_while_loops;

        extern StatementMatcher delete_calls;
    }


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
                const clang::RecordDecl* class_struct,
                std::vector<const clang::FieldDecl*> *field_vector,
                std::map<const clang::FunctionDecl*, std::vector<const clang::MemberExpr*>> *rlvnt_funcs,
                std::map<const Stmt*, loop_credentials> *rlvnt_loops);

            ~record_info(){
                free(fun_mem.mat);
                free(loop_mem.mat);
            }


            //reference to the record node (class/struct) that is referred to by this struct
            const clang::RecordDecl
                *record;
            //reference to all the member nodes that the referred record has
            std::vector<const clang::FieldDecl*>
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
                    for(size_t o = 0; o < fields.size(); ++o){
                        float value_at = mat->at(o, getIdx(t.first));
                        value_at == 0 ? logger::log_stream << " " : logger::log_stream << value_at;
                        logger::log_stream << "\t";
                        if(o == fields.size()-1){
                            logger::log_stream << "] " << getName(t.first);
                            logger::out();
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

#endif