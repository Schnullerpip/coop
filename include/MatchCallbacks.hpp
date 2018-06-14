#ifndef COOP_MATCHCALLBACKS_HPP
#define COOP_MATCHCALLBACKS_HPP

#include "coop_utils.hpp"

namespace coop {

    class CoopMatchCallback : public MatchFinder::MatchCallback {
        public:
            CoopMatchCallback(const std::vector<const char*> *user_source_files)
                :user_source_files(user_source_files){}
        protected:
            const char* is_user_source_file(const char* file_path);

            //returns true if the loop is in a user specified file
            bool get_for_loop_identifier(const ForStmt* loop, SourceManager *srcMgr, std::stringstream*);
            //returns true if the loop is in a user specified file
            bool get_while_loop_identifier(const WhileStmt* loop, SourceManager *srcMgr, std::stringstream*);
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
}
#endif