#ifndef COOP_UTILS_HPP
#define COOP_UTILS_HPP

// Declares clang::SyntaxOnlyAction.
#include "clang/Frontend/FrontendActions.h"
#include <ctime>
#include <string>
#include <stdlib.h>
#include <sstream>
#include <iostream>

//defines
#define coop_class_s "class"
#define coop_member_s "member"
#define coop_function_s "function"


namespace coop{



    namespace logger {
        namespace {
            const char* out_prefix = "[coop]::[logger]::[";
        }

        //configurable streams
        std::ostream& out_stream = std::cout;
        std::stringstream log_stream;
        //configurable depth (out message indentation)
        size_t depth = 0;

        /*outs message to out_stream*/
        size_t& out(const char* msg, const char* append = "\n"){
            //getting the time
            time_t now = time(0);
            char* now_s = ctime(&now);
            size_t strln = strlen(now_s);
            //getting rid of the weird trailing linebreak
            now_s[strln-1]=']';

            out_stream << out_prefix << now_s << "-> ";
            for(size_t i = 0; i < depth; ++i){
                out_stream << (i == 0 ? "  " : "|  ");
            }
            if(depth > 0){
                out_stream << "|__ ";
            }

            out_stream << msg << append;
            return depth;
        }

        /*outs message to out_stream*/
        size_t& out(std::stringstream& msg_stream, const char* append = "\n"){
            std::string msg = msg_stream.str();
            msg_stream.str("");
            msg_stream.clear();
            return out(msg.c_str(), append);
        }

        size_t& out(){
            return out(log_stream);
        }


        /*outs message to out_stream informing the user of a progress*/
        enum Progress_Status {RUNNING, DONE, TODO};
        size_t& out(const char* msg, Progress_Status status){
            return out(msg, status == RUNNING ? " [running]\n" : status == DONE ? " [done]\n" : " [TODO!!!!!!!]\n");
        }

        /*outs message to out_stream informing the user of a progress*/
        size_t& out(std::stringstream& msg_stream, Progress_Status status){
            std::string msg = msg_stream.str();
            msg_stream.str("");
            msg_stream.clear();
            return out(msg.c_str(), status);
        }

        void out(Progress_Status status){
            out(log_stream, status);
        }
    }

    namespace record {
        struct record_info {
            void init(const RecordDecl* class_struct, std::vector<const FieldDecl*> *field_vector, std::map<const FunctionDecl*, std::vector<const MemberExpr*>> *relevant_funcs){
                record = class_struct;
                fields = field_vector;
                mat_dim = field_vector->size() * relevant_funcs->size();
                fun_mem_mat = static_cast<float*>(calloc(mat_dim, sizeof(float)));
                relevant_functions = relevant_funcs;
            }
            ~record_info(){
                free(fun_mem_mat);
            }
            float *fun_mem_mat; //the matrix mapping which function uses which member of the class 
            /*the setup will be e.g.
               m_a, m_b, m_c, m_d
            f1  x    x        
            f2  x         x
            f3
            f4            x    x
            */
            size_t mat_dim; //cached matrix dimension
            const RecordDecl *record; //reference to the record node (class/struct) that is referred to by this struct
            std::vector<const FieldDecl*> *fields; //reference to all the member nodes that the referred record has
            std::map<const FunctionDecl*, std::vector<const MemberExpr*>> *relevant_functions;

            float& at(int x, int y){
                return fun_mem_mat[y * fields->size() + x];
            }

            void print_mat(){
                logger::log_stream << "'" << record->getNameAsString().c_str() << "':\n";
                int count = 0;
                logger::log_stream << "\t";
                for(auto f : *fields){
                    logger::log_stream << " " << f->getNameAsString().c_str() << "\t";
                }
                logger::log_stream << "\n";
                for(auto f : *relevant_functions){
                    logger::log_stream << f.first->getNameAsString().c_str() << "\t[";
                    for(size_t o = 0; o < fields->size(); ++o){
                        logger::log_stream << at(o, count) << "\t";
                        if(o == fields->size()-1){
                            logger::log_stream << "]\n";
                        }else{
                            logger::log_stream << ",";
                        }
                    }
                    count++;
                }
                logger::out();
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

    namespace match {
		DeclarationMatcher members = fieldDecl(hasAncestor(cxxRecordDecl(anyOf(isClass(), isStruct())))).bind(coop_member_s);
        DeclarationMatcher classes = cxxRecordDecl(hasDefinition(), unless(isUnion())).bind(coop_class_s);
		StatementMatcher funcs_using_members =
			memberExpr(hasAncestor(functionDecl().bind(coop_function_s))).bind(coop_member_s);
    }

    bool are_same_variable(const clang::ValueDecl *First, const clang::ValueDecl *Second) {
        return First && Second &&
                First->getCanonicalDecl() == Second->getCanonicalDecl();
    } 
}

#endif