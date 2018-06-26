#ifndef COOP_MATCHCALLBACKS_HPP
#define COOP_MATCHCALLBACKS_HPP

#include "coop_utils.hpp"

namespace coop {

    namespace match {
        void add_file_as_match_condition(const char *);
        std::string get_file_regex_match_condition(const char * patch_addition = "");
    }

    class CoopMatchCallback : public MatchFinder::MatchCallback {
        public:
            CoopMatchCallback(const std::vector<const char*> *user_source_files)
                :user_source_files(user_source_files){}
        protected:
            const char * is_user_source_file(const char *file_path);
            const char * get_relevant_token(const char *file_path);

            void get_for_loop_identifier(const ForStmt* loop, SourceManager *srcMgr, std::stringstream*);
            void get_while_loop_identifier(const WhileStmt* loop, SourceManager *srcMgr, std::stringstream*);
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
        static std::map<const FunctionDecl*, std::vector<const MemberExpr*>> relevant_functions;
        //will associate each relevant function with a unique index
        static std::map<const FunctionDecl*, int> function_idx_mapping;

        FunctionRegistrationCallback(const std::vector<const char*> *user_files):CoopMatchCallback(user_files){}

    private:
        void run(const MatchFinder::MatchResult &result) override;
    };


    /*
        will match on all function calls, that are made inside a loop, so they can later be checked
        against wether or not they use members and therefore those members' datalayout should be optimized
    */
    class LoopFunctionsCallback : public coop::CoopMatchCallback {
    public:
        std::map<const Stmt*, coop::loop_credentials> loop_function_calls;
        LoopFunctionsCallback(std::vector<const char*> *user_files):CoopMatchCallback(user_files){}
        void printData();
    private:
        void run(const MatchFinder::MatchResult &result);
    };

    /*
        Will cache all members used in loops and the loops respectively, so they can later be used
        to deduce a heuristic considering member usage
    */
   class LoopMemberUsageCallback : public coop::CoopMatchCallback {
   public:
        //will hold all the member-references that are made inside loops
        static std::map<const clang::Stmt*, loop_credentials>
            loops;

        //will associate each loop with an unique idx
        static std::map<const clang::Stmt*, int>
            loop_idx_mapping;

        static void register_loop(const clang::Stmt* loop);
        static bool is_registered(const clang::Stmt* loop);

        LoopMemberUsageCallback(std::vector<const char*> *user_files):CoopMatchCallback(user_files){}
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
        //careful -> this is a matcherCallback so there wont be complete information during the run method
        //we will later have to distinct which loops are even relevant to us (associate members directly/indirectly)
        static std::map<const clang::Stmt*, std::vector<const clang::Stmt*>>
            parent_child_map;

        NestedLoopCallback(std::vector<const char*> *user_files):CoopMatchCallback(user_files){}
        void traverse_parents(std::function<void (std::map<const clang::Stmt *, coop::loop_credentials>::iterator*, std::vector<const Stmt*>*)> callback);
        void traverse_parents_children(std::function<void (std::map<const clang::Stmt *, coop::loop_credentials>::iterator*, const Stmt* child)>);
        void print_data();
    private:
        void run(const MatchFinder::MatchResult &result);
    };

    /*Will be used to find the cold members of a record*/
    class ColdFieldCallback : public coop::CoopMatchCallback {
    public:
        struct memExpr_ASTcon{
            const clang::MemberExpr* mem_expr_ptr;
            ASTContext *ast_context_ptr;
        };
        /*will map a cold field to all its occurences (without declaration) in the code*/
        static std::map<const clang::FieldDecl*, std::vector<memExpr_ASTcon>>
            cold_field_occurances;
        
        explicit ColdFieldCallback(
            std::vector<const char*> *user_files,
            std::vector<const clang::FieldDecl*> *fields_to_find,
            ASTContext *ast_c):
                CoopMatchCallback(user_files)
            {
                this->fields_to_find = fields_to_find;
                this->ast_context_ptr = ast_c;
            }

    private:
        std::vector<const clang::FieldDecl*>
            *fields_to_find;
        ASTContext *ast_context_ptr;

        void run(const MatchFinder::MatchResult &result);
    };

    /*will find the main function
        this entrence is needed to ensure stack allocation for the cold data structs is present when Objects are created
    */
    class FindMainFunction : public coop::CoopMatchCallback {
    public:
        static FunctionDecl  const * main_function_ptr;
        explicit FindMainFunction(std::vector<const char*> *user_files):CoopMatchCallback(user_files){}

        void run(const MatchFinder::MatchResult &result);
    };

    /*will find destructors to certain records
    this is very important, because while using a freelist as a cold-data container is nice
    it will most certainly start to fragment, very fast -> so whenever an object becomes destroyed we want to make sure,
    that it's corresponding cold struct is marked free, as well -> we need the destructor, or make one if there is none*/
    class FindDestructor : public MatchFinder::MatchCallback {
    public:
        FindDestructor(coop::record::record_info &record_info)
            :rec(record_info)
        {

        }
    private:
        coop::record::record_info &rec;
        void run(const MatchFinder::MatchResult &result);
    };

    /*will register new calls */
    class FindInstantiations : public MatchFinder::MatchCallback {
    public:
        static std::map<const RecordDecl*, std::vector<std::pair<const CXXNewExpr*, ASTContext*>>> instantiations_map;

        std::vector<const RecordDecl*> records_to_instantiate;
        void add_record(const RecordDecl* r);
    private:
        void run(const MatchFinder::MatchResult &result);
    };

    /*will register delete calls */
    class FindDeleteCalls : public MatchFinder::MatchCallback {
    public:
        static std::map<const RecordDecl*, std::vector<std::pair<const CXXDeleteExpr*, ASTContext*>>> delete_calls_map;
        std::vector<const RecordDecl*> record_deletions_to_find;
        void add_record(const RecordDecl* r);
    private:
        void run(const MatchFinder::MatchResult &result);
    };
}
#endif