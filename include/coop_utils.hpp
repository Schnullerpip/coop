#ifndef COOP_UTILS_HPP
#define COOP_UTILS_HPP

#include "clang/AST/Decl.h"
#include <ctime>
#include <string>
#include <stdlib.h>
#include <sstream>
#include <iostream>
#include <functional>

//defines
#define coop_class_s "class"
#define coop_member_s "member"
#define coop_function_s "function"
#define coop_function_call_s "functionCall"
#define coop_loop_s "loop"


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

        void clear(std::stringstream& msg_stream = log_stream){
            msg_stream.str("");
            msg_stream.clear();
        }

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
            clear(msg_stream);
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

    namespace match {
        DeclarationMatcher classes = cxxRecordDecl(hasDefinition(), unless(isUnion())).bind(coop_class_s);
		DeclarationMatcher members = fieldDecl(hasAncestor(cxxRecordDecl(anyOf(isClass(), isStruct())).bind(coop_class_s))).bind(coop_member_s);
		StatementMatcher members_used_in_functions = memberExpr(hasAncestor(functionDecl().bind(coop_function_s))).bind(coop_member_s);

        StatementMatcher loops = anyOf(forStmt().bind(coop_loop_s), whileStmt().bind(coop_loop_s));
        StatementMatcher function_calls_in_loops = callExpr(hasAncestor(loops)).bind(coop_function_call_s);

        StatementMatcher members_used_in_for_loops =
            memberExpr(hasAncestor(forStmt().bind(coop_loop_s))).bind(coop_member_s);
        StatementMatcher members_used_in_while_loops =
            memberExpr(hasAncestor(whileStmt().bind(coop_loop_s))).bind(coop_member_s);
    }

    class CoopMatchCallback : public MatchFinder::MatchCallback {
        public:
            CoopMatchCallback(const std::vector<const char*> *user_source_files)
                :user_source_files(user_source_files){}
        protected:
            const char* is_user_source_file(const char* file_path){
                const char* relevant_token;
                for(relevant_token = file_path+strlen(file_path); *(relevant_token-1) != '/' && *(relevant_token-1) != '\\'; --relevant_token);
                for(auto file : *user_source_files){
                    if(strcmp(file, relevant_token)){
                        return nullptr;
                    }
                }
                return relevant_token;
            }
        private:
            const std::vector<const char*> *user_source_files;
            virtual void run(const MatchFinder::MatchResult &result) = 0;

    };

    //matchcallback that registeres members of classes for later usage
    class MemberRegistrationCallback : public coop::CoopMatchCallback {
    public:
        std::map<const RecordDecl*, std::vector<const FieldDecl*>> class_fields_map;

        MemberRegistrationCallback(const std::vector<const char*> *user_files):CoopMatchCallback(user_files){}

        void printData(){
            for(auto pair : class_fields_map){
                coop::logger::out(pair.first->getNameAsString().c_str())++;	
                for(auto mem : pair.second){
                    coop::logger::out(mem->getNameAsString().c_str());
                }
                coop::logger::depth--;
            }
        }

    private:
        virtual void run(const MatchFinder::MatchResult &result){
            const RecordDecl* rd = result.Nodes.getNodeAs<RecordDecl>(coop_class_s);
            const FieldDecl* member = result.Nodes.getNodeAs<FieldDecl>(coop_member_s);

            SourceManager &srcMgr = result.Context->getSourceManager();
            if(is_user_source_file(srcMgr.getFilename(rd->getLocation()).str().c_str())){
                coop::logger::log_stream << "found '" << member->getNameAsString().c_str() << "' in record '" << rd->getNameAsString().c_str() << "'";
                coop::logger::out();
                class_fields_map[rd].push_back(member);
            }
            ////retreive
            //const RecordDecl* rd = result.Nodes.getNodeAs<RecordDecl>(coop_class_s);

            //SourceManager &srcMgr = result.Context->getSourceManager();
            //if(is_user_source_file(srcMgr.getFilename(rd->getLocation()).str().c_str())){
            //    //register the field
            //    clang::RecordDecl::field_iterator fi;
            //    coop::logger::depth++;
            //    for(fi = rd->field_begin(); fi != rd->field_end(); fi++){
            //        class_fields_map[rd].push_back(*fi);
            //        coop::logger::log_stream << "found '" << fi->getNameAsString().c_str() << "' in record '" << rd->getNameAsString().c_str() << "'";
            //        coop::logger::out();
            //    }
            //    coop::logger::depth--;
            //}
        }
    };

    /*
        will cache the functions and the members they use so a member_matrix can be made for each class telling us
        how often which members are used inside which function
    */
    class MemberUsageInFunctionsCallback : public coop::CoopMatchCallback{
    public:
        //will hold all the functions, that use members and are therefore 'relevant' to us
        std::map<const FunctionDecl*, std::vector<const MemberExpr*>> relevant_functions;
        MemberUsageInFunctionsCallback(const std::vector<const char*> *user_files):CoopMatchCallback(user_files){}

    private:
        void run(const MatchFinder::MatchResult &result) override {
            const FunctionDecl* func = result.Nodes.getNodeAs<FunctionDecl>(coop_function_s);
            const MemberExpr* memExpr = result.Nodes.getNodeAs<MemberExpr>(coop_member_s);

            SourceManager &srcMgr = result.Context->getSourceManager();
            if(is_user_source_file(srcMgr.getFilename(func->getLocation()).str().c_str())){
                coop::logger::log_stream << "found function declaration '" << func->getNameAsString() << "' using member '" << memExpr->getMemberDecl()->getNameAsString() << "'";
                coop::logger::out();

                //cache the function node for later traversal
                relevant_functions[func].push_back(memExpr);
            }
        }
    };

    /*
        will match on all function calls, that are made inside a loop, so they can later be checked
        against wether or not they use members and therefore those members' datalayout should be optimized
    */
    struct loop_credentials{
        std::vector<const FunctionDecl*> funcs;
        bool isForLoop;
        std::string identifier;
    };
    class LoopFunctionsCallback : public coop::CoopMatchCallback {
    public:
        std::map<const Stmt*, loop_credentials> loop_function_calls;
        LoopFunctionsCallback(std::vector<const char*> *user_files):CoopMatchCallback(user_files){}
        void printData(){
			for(auto lc : loop_function_calls){
				coop::logger::log_stream << "loop " << lc.second.identifier.c_str();
				coop::logger::out()++;
				coop::logger::log_stream << "[";
				for(auto f : lc.second.funcs){
					coop::logger::log_stream << f->getNameAsString() << ", ";
				}
				coop::logger::log_stream << "]";
				coop::logger::out()--;
			}
        }
    private:
        void run(const MatchFinder::MatchResult &result){
            SourceManager &srcMgr = result.Context->getSourceManager();

            const FunctionDecl *function_call = result.Nodes.getNodeAs<CallExpr>(coop_function_call_s)->getDirectCallee();
            if(function_call){
                if(!is_user_source_file(srcMgr.getFilename(function_call->getLocation()).str().c_str())){
                    return;
                }
            }else{
                return;
            }

            coop::logger::log_stream << "found function '" << function_call->getNameAsString() << "' being called in a " ;

            Stmt const *loop;
            char const *fileName;
            bool isForLoop = true;
            if(const ForStmt* forLoop = result.Nodes.getNodeAs<ForStmt>(coop_loop_s)){
                loop = forLoop;
                fileName = is_user_source_file(srcMgr.getFilename(forLoop->getForLoc()).str().c_str());
                coop::logger::log_stream << "for";
            }else if(const WhileStmt* whileLoop = result.Nodes.getNodeAs<WhileStmt>(coop_loop_s)){
                loop = whileLoop;
                fileName = is_user_source_file(srcMgr.getFilename(whileLoop->getWhileLoc()).str().c_str());
                isForLoop = false;
                coop::logger::log_stream << "while";
            }

            //check if the loop occurs in a user file
            if(!fileName){
                coop::logger::clear();
                return;
            }

            std::stringstream ss;
            ss << "[" << fileName << ":" << srcMgr.getPresumedLineNumber(loop->getLocStart()) << "]";

            coop::logger::log_stream << "Loop " << ss.str();
            coop::logger::out();
            
            loop_function_calls[loop].identifier = ss.str();
            loop_function_calls[loop].isForLoop = isForLoop;
            loop_function_calls[loop].funcs.push_back(function_call);
        }
    };

    /*
        Will cache all members used in loops and the loops respectively, so they can later be used
        to deduce a heuristic considering member usage
    */
   class LoopRegistrationCallback : public coop::CoopMatchCallback {
   public:
        static std::map<const clang::Stmt*, std::vector<const clang::MemberExpr*>> loop_members_map;
        LoopRegistrationCallback(std::vector<const char*> *user_files):CoopMatchCallback(user_files){}
   private:
        void run(const MatchFinder::MatchResult &result){

            const MemberExpr *member = result.Nodes.getNodeAs<MemberExpr>(coop_member_s);

            if(const ForStmt* loop = result.Nodes.getNodeAs<ForStmt>(coop_loop_s)){
                SourceManager &srcMgr = result.Context->getSourceManager();
                if(is_user_source_file(srcMgr.getFilename(loop->getForLoc()).str().c_str())){
                    coop::logger::log_stream << "found for loop iterating '" << member->getMemberDecl()->getNameAsString().c_str() << "'";
                    coop::logger::out();
                    loop_members_map[loop].push_back(member);
                }
            }else if(const WhileStmt* loop = result.Nodes.getNodeAs<WhileStmt>(coop_loop_s)){
                SourceManager &srcMgr = result.Context->getSourceManager();
                if(is_user_source_file(srcMgr.getFilename(loop->getWhileLoc()).str().c_str())){
                    coop::logger::log_stream << "found while loop iterating '" << member->getMemberDecl()->getNameAsString().c_str() << "'";
                    coop::logger::out();
                    loop_members_map[loop].push_back(member);
                }
            }
        }
   };

    template <class T>
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
        std::map<const T*, std::vector<const MemberExpr*>> *relevant_instances;
    private:
        size_t fields;


    public:
        void init(size_t num_fields, std::map<const T*, std::vector<const MemberExpr*>> *ref){
            fields = num_fields;
            dim = fields * ref->size();
            mat = static_cast<float*>(calloc(dim, sizeof(float)));
            relevant_instances = ref;
        }

        float& at(int x, int y){
            return mat[y * fields + x];
        }
    };

    namespace record {
        struct record_info {
            void init(
                const clang::RecordDecl* class_struct,
                std::vector<const clang::FieldDecl*> *field_vector,
                std::map<const clang::FunctionDecl*, std::vector<const clang::MemberExpr*>> *rlvnt_funcs,
                std::map<const Stmt*, std::vector<const MemberExpr*>> *rlvnt_loops)
                {

                record = class_struct;
                fields = field_vector;

                fun_mem.init(fields->size(), rlvnt_funcs);
                loop_mem.init(fields->size(), rlvnt_loops);

                relevant_functions = rlvnt_funcs;
                relevant_loops = rlvnt_loops;

                //the fun_mem_mat will be written according to the indices the members are mapped to here
                //since a function can mention the same member several times, we need to make sure each
                //iteration over the same member associates with the same adress in the matrix (has the same index)
                int index_count = 0;
                for(auto f : *fields){
                        member_idx_mapping[f] = index_count++;
                }
            }

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
            std::map<const clang::Stmt*, std::vector<const clang::MemberExpr*>>
                *relevant_loops;

            //the matrix mapping which function uses which member of the class
            data_matrix<clang::FunctionDecl> fun_mem;
            //the matrix mapping which loop uses which member of the class
            data_matrix<clang::Stmt> loop_mem;

            //will associate each member with a consistent index
            std::map<const clang::FieldDecl*, int> member_idx_mapping;

            //returns a list of members associated by a function for this record - if it does; else nullptr
            std::vector<const MemberExpr*>* isRelevantFunction(const clang::FunctionDecl* func){
                auto funcs_iter = relevant_functions->find(func);
                if(funcs_iter != relevant_functions->end()){
                    return &funcs_iter->second;
                }
                return nullptr;
            }
            
            //returns the FieldDecl*s idx if the member is relevant to this record, else a negative value
            int isRelevantField(const MemberExpr* memExpr){
                const FieldDecl* field = static_cast<const FieldDecl*>(memExpr->getMemberDecl());
                if(std::find(fields->begin(), fields->end(), field) != fields->end()){
                    return member_idx_mapping[field];
                }
                return -1;
            }

            void print_func_mem_mat(){
                std::function<const char* (const FunctionDecl*)> getNam = [](const FunctionDecl* fd){ return fd->getNameAsString().c_str();};
                print_mat(&fun_mem, getNam);
            }
            void print_loop_mem_mat(LoopFunctionsCallback* lfc){
                std::function<const char* (const Stmt*)> getNam = [lfc](const Stmt* ls){ 
                    auto iter = lfc->loop_function_calls.find(ls);
                    if(iter != lfc->loop_function_calls.end()){
                        return iter->second.identifier.c_str();
                    }
                    return "unidentified loop -> this is most likely a bug!";
                };
                print_mat(&loop_mem, getNam);
            }

        private:
            template<typename T>
            void print_mat (data_matrix<T>* mat, std::function<const char* (const T*)>& getName){
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

    bool are_same_variable(const clang::ValueDecl *First, const clang::ValueDecl *Second) {
        return First && Second &&
                First->getCanonicalDecl() == Second->getCanonicalDecl();
    } 
}

#endif