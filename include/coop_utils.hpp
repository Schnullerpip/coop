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

//defines
#define coop_class_s "class"
#define coop_member_s "member"
#define coop_function_s "function"
#define coop_function_call_s "functionCall"

#define coop_loop_s "loop"
#define coop_for_loop_s "for_loop"
#define coop_while_loop_s "while_loop"

#define coop_parent_loop_s "parent_loop"
#define coop_parent_for_loop_s "parent_for_loop_s"
#define coop_parent_while_loop_s "parent_while_loop_s"


namespace coop{

    namespace match {
        extern DeclarationMatcher classes;
		extern DeclarationMatcher members;
		extern StatementMatcher members_used_in_functions;

        extern StatementMatcher loops;
        extern StatementMatcher function_calls_in_loops;
        extern StatementMatcher nested_loops;

        extern StatementMatcher members_used_in_for_loops;
        extern StatementMatcher members_used_in_while_loops;
    }

    class CoopMatchCallback : public MatchFinder::MatchCallback {
        public:
            CoopMatchCallback(const std::vector<const char*> *user_source_files)
                :user_source_files(user_source_files){}
        protected:
            const char* is_user_source_file(const char* file_path);
            void get_for_loop_identifier(const ForStmt* loop, SourceManager *srcMgr, std::stringstream&);
            void get_while_loop_identifier(const WhileStmt* loop, SourceManager *srcMgr, std::stringstream&);
        private:
            const std::vector<const char*> *user_source_files;
            virtual void run(const MatchFinder::MatchResult &result) = 0;
    };

    //matchcallback that registeres members of classes for later usage
    class MemberRegistrationCallback : public coop::CoopMatchCallback {
    public:
        std::map<const RecordDecl*, std::vector<const FieldDecl*>> class_fields_map;

        MemberRegistrationCallback(const std::vector<const char*> *user_files):CoopMatchCallback(user_files){}

        void printData();

    private:
        virtual void run(const MatchFinder::MatchResult &result);
    };

    /*
        will cache the functions and the members they use so a member_matrix can be made for each class telling us
        how often which members are used inside which function
    */
    class FunctionRegistrationCallback : public coop::CoopMatchCallback{
    public:
        //will hold all the functions, that use members and are therefore 'relevant' to us
        std::map<const FunctionDecl*, std::vector<const MemberExpr*>> relevant_functions;
        //will associate each relevant function with an unique index
        std::map<const FunctionDecl*, int> function_idx_mapping;

        FunctionRegistrationCallback(const std::vector<const char*> *user_files):CoopMatchCallback(user_files){}

    private:
        void run(const MatchFinder::MatchResult &result) override;
    };

    //POD for loop information
    struct loop_credentials{
        std::vector<const FunctionDecl*> funcs;
        std::vector<const MemberExpr*> member_usages;
        bool isForLoop;
        std::string identifier;
    };

    /*
        will match on all function calls, that are made inside a loop, so they can later be checked
        against wether or not they use members and therefore those members' datalayout should be optimized
    */
    class LoopFunctionsCallback : public coop::CoopMatchCallback {
    public:
        std::map<const Stmt*, loop_credentials> loop_function_calls;
        LoopFunctionsCallback(std::vector<const char*> *user_files):CoopMatchCallback(user_files){}
        void printData();
    private:
        void run(const MatchFinder::MatchResult &result);
    };

    /*
        Will cache all members used in loops and the loops respectively, so they can later be used
        to deduce a heuristic considering member usage
    */
   class LoopRegistrationCallback : public coop::CoopMatchCallback {
   public:
        //will hold all the member-references that are made inside loops
        static std::map<const clang::Stmt*, loop_credentials>
            loops;

        //will associate each loop with an unique idx
        static std::map<const clang::Stmt*, int>
            loop_idx_mapping;

        static void register_loop(const clang::Stmt* loop);

        LoopRegistrationCallback(std::vector<const char*> *user_files):CoopMatchCallback(user_files){}
   private:
        void run(const MatchFinder::MatchResult &result);
   };

   /* Will be used to find nested loops, so the loop member matrices
      can be adjusted according to loop depth
      This approach implies that a possible heuristic considers higher loop depth to be a
      valid indicator for frequent usage */
    class NestedLoopCallback : public coop::CoopMatchCallback {
    public:
        //will cache the found nested loops and save a reference to the immediate parent
        //deeper nesting needs to be found by iterating over the child_parent_map
        static std::map<const clang::Stmt*, const clang::Stmt*> parent_child_map;
        NestedLoopCallback(std::vector<const char*> *user_files):CoopMatchCallback(user_files){}
    private:
        void run(const MatchFinder::MatchResult &result);
    };

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

    namespace record {
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
            const clang::RecordDecl *record;
            //reference to all the member nodes that the referred record has
            std::vector<const clang::FieldDecl*> *fields;

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
                member_idx_mapping;

            //returns a list of members associated by a function for this record - if it does; else nullptr
            std::vector<const MemberExpr*>* isRelevantFunction(const clang::FunctionDecl* func);
            
            //returns the FieldDecl*s idx if the member is relevant to this record, else a negative value
            int isRelevantField(const MemberExpr* memExpr);

            void print_func_mem_mat();
            void print_loop_mem_mat(LoopRegistrationCallback* loop_registry);

        private:
            template<typename T, typename P>
            void print_mat (data_matrix<T, P>* mat, std::function<const char* (const T*)>& getName){
                int count = 0;
                for(auto f : *fields){
                    logger::log_stream << " " << f->getNameAsString().c_str() << "\t";
                }
                logger::out();
                for(auto t : *mat->relevant_instances){
                    logger::log_stream << "[";
                    for(size_t o = 0; o < fields->size(); ++o){
                        logger::log_stream << mat->at(o, count) << "\t";
                        if(o == fields->size()-1){
                            logger::log_stream << "] " << getName(t.first);
                            logger::out();
                        }else{
                            logger::log_stream << ",";
                        }
                    }
                    count++;
                }
            }
        };
    }

    template <typename T> class Printer : public MatchFinder::MatchCallback {
        const char* m_binder;
        std::stringstream ss;
    public:
        explicit inline Printer(const char* binder):m_binder(binder){}

        virtual void run(const MatchFinder::MatchResult &Result){
            const T* p = Result.Nodes.getNodeAs<T>(m_binder);
                ss << "found " << p->getNameAsString().c_str();
                coop::logger::out(ss);
        }
    };

}

#endif