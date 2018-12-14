#ifndef COOP_MATCHCALLBACKS_HPP
#define COOP_MATCHCALLBACKS_HPP

#include"coop_utils.hpp"
#include<set>

namespace coop {

    namespace match {
        void add_file_as_match_condition(const char *);
        std::string get_file_regex_match_condition(const char * patch_addition = "");
    }

    class CoopMatchCallback : public MatchFinder::MatchCallback {
        public:
        protected:
            const char * get_relevant_token(const char *file_path);

            void get_for_loop_identifier(const ForStmt* loop, SourceManager *srcMgr, std::stringstream*);
            void get_while_loop_identifier(const WhileStmt* loop, SourceManager *srcMgr, std::stringstream*);
        private:
            virtual void run(const MatchFinder::MatchResult &result) = 0;
    };


    //matchcallback that registeres members of classes for later usage
    class MemberRegistrationCallback : public coop::CoopMatchCallback {
    public:
        static std::map<const RecordDecl*, std::set<const FieldDecl*>> class_fields_map;
        static std::map<const RecordDecl*, std::string> class_file_map;

        void printData();

    private:
        virtual void run(const MatchFinder::MatchResult &result);
    };

    /*will find all the access specs for a record*/
    class AccessSpecCallback : public MatchFinder::MatchCallback
    {
    public:
        static std::map<const RecordDecl*, std::set<const AccessSpecDecl*>> record_public_access_map;
        static std::map<const RecordDecl*, std::set<const AccessSpecDecl*>> record_private_access_map;
    private:
        virtual void run(const MatchFinder::MatchResult &result);
    };


    /*  
        will cache all the pure function prototypes so later other functionDecls can be attributed to this prototype
        this is very important, so that functioncalls made in other translationunits (which can only refer to the prototype defined in some h/hpp file)
        will be able to associate the actualfunction definition (defined in some completely unrelated c/cpp file)
    */
    class FunctionPrototypeRegistrationCallback : public MatchFinder::MatchCallback {
    public:
        static std::set<std::pair<const FunctionDecl*, std::string>> function_prototypes;
    private:
        virtual void run(const MatchFinder::MatchResult &result) override;
    };

    /*
        will cache the functions and the members they use so a member_matrix can be made for each class telling us
        how often which members are used inside which function
    */
    class FunctionRegistrationCallback : public coop::CoopMatchCallback{
    public:

        static bool isIndexed(const FunctionDecl *f);
        static void indexFunction(const FunctionDecl *f);
        static void registerFunction(const FunctionDecl *f);

        //holds the main function AT node ptr - if found
        static FunctionDecl  const * main_function_ptr;
        //holds the name of the file containing a main function - if found. Before usage should always check wether main_function_ptr != nullptr
        static std::string main_file;

        //will hold all the functions, that use members and are therefore 'relevant' to us
        static std::map<const FunctionDecl*, std::vector<const MemberExpr*>>
            relevant_functions;

        //will associate each relevant function with a unique index
        static std::map<const FunctionDecl*, int>
            function_idx_mapping;

    private:
        void run(const MatchFinder::MatchResult &result) override;
    };




    /* will match on all functions, that call another function */
    class ParentedFunctionCallback : public MatchFinder::MatchCallback {
    private:
        void run(const MatchFinder::MatchResult &result);
    };

    class ParentedLoopCallback : public MatchFinder::MatchCallback {
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

        //will associate each loop with a unique idx
        static std::map<const clang::Stmt*, int>
            loop_idx_mapping;

        static bool isIndexed(const clang::Stmt* loop);
        static void indexLoop(const clang::Stmt* loop);
        static void registerLoop(const clang::Stmt* loop, std::string loop_name ,bool isForLoop);
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
            std::vector<const clang::FieldDecl*> *fields_to_find,
            ASTContext *ast_c)
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


    /*will register all constructors for the records_to_find*/
    class FindConstructor : public MatchFinder::MatchCallback {
    public:
        static std::map<const RecordDecl*, std::vector<const CXXConstructorDecl*>> rec_constructor_map;
        static std::map<const RecordDecl*, std::vector<const CXXConstructorDecl*>> rec_copy_constructor_map;
        static std::map<const RecordDecl*, std::vector<const CXXConstructorDecl*>> rec_move_constructor_map;
        void add_record(const RecordDecl* r);
    private:
        std::vector<const RecordDecl*> records_to_find;
        void run(const MatchFinder::MatchResult &result);
    };

    /*will find copy assignment operators*/
    class FindCopyAssignmentOperators : public MatchFinder::MatchCallback
    {
    public:
        static std::map<const RecordDecl*, const CXXMethodDecl*> rec_copy_assignment_operator_map;
        void add_record(const RecordDecl* r);
    private:
        std::vector<const RecordDecl*> records_to_find;
        void run(const MatchFinder::MatchResult &result);
    };

    /*will find move assignment operators*/
    class FindMoveAssignmentOperators : public MatchFinder::MatchCallback
    {
    public:
        static std::map<const RecordDecl*, const CXXMethodDecl*> rec_move_assignment_operator_map;
        void add_record(const RecordDecl* r);
    private:
        std::vector<const RecordDecl*> records_to_find;
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