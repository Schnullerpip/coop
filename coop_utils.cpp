#include "coop_utils.hpp"

//static variables
std::map<const clang::Stmt*, coop::loop_credentials>
coop::LoopRegistrationCallback::loops = {};

std::map<const clang::Stmt*, int>
coop::LoopRegistrationCallback::loop_idx_mapping = {};

std::map<const clang::Stmt*, const clang::Stmt*>
coop::NestedLoopCallback::parent_child_map = {};

//method implementations
const char * coop::CoopMatchCallback::is_user_source_file(const char* file_path){
    const char* relevant_token;
    for(relevant_token = file_path+strlen(file_path); *(relevant_token-1) != '/' && *(relevant_token-1) != '\\'; --relevant_token);
    for(auto file : *user_source_files){
        if(strcmp(file, relevant_token)){
            return nullptr;
        }
    }
    return relevant_token;
}

void coop::CoopMatchCallback::get_for_loop_identifier(const ForStmt* loop, SourceManager *srcMgr, std::stringstream &dest){
    const char *fileName = is_user_source_file(srcMgr->getFilename(loop->getForLoc()).str().c_str());
    dest << "[" << fileName << ":" << srcMgr->getPresumedLineNumber(loop->getLocStart()) << "]";
}
void coop::CoopMatchCallback::get_while_loop_identifier(const WhileStmt* loop, SourceManager *srcMgr, std::stringstream &dest){
    const char *fileName = is_user_source_file(srcMgr->getFilename(loop->getWhileLoc()).str().c_str());
    dest << "[" << fileName << ":" << srcMgr->getPresumedLineNumber(loop->getLocStart()) << "]";
}


/*MemberRegistrationCallback*/

void coop::MemberRegistrationCallback::printData(){
    for(auto pair : class_fields_map){
        coop::logger::out(pair.first->getNameAsString().c_str())++;	
        for(auto mem : pair.second){
            coop::logger::out(mem->getNameAsString().c_str());
        }
        coop::logger::depth--;
    }
}

void coop::MemberRegistrationCallback::run(const MatchFinder::MatchResult &result){
    const RecordDecl* rd = result.Nodes.getNodeAs<RecordDecl>(coop_class_s);
    const FieldDecl* member = result.Nodes.getNodeAs<FieldDecl>(coop_member_s);

    SourceManager &srcMgr = result.Context->getSourceManager();
    if(is_user_source_file(srcMgr.getFilename(rd->getLocation()).str().c_str())){
        coop::logger::log_stream << "found '" << member->getNameAsString().c_str() << "' in record '" << rd->getNameAsString().c_str() << "'";
        coop::logger::out();
        class_fields_map[rd].push_back(member);
    }
}

/*FunctionRegistrationCallback*/
void coop::FunctionRegistrationCallback::run(const MatchFinder::MatchResult &result){
    const FunctionDecl* func = result.Nodes.getNodeAs<FunctionDecl>(coop_function_s);
    const MemberExpr* memExpr = result.Nodes.getNodeAs<MemberExpr>(coop_member_s);

    SourceManager &srcMgr = result.Context->getSourceManager();
    if(is_user_source_file(srcMgr.getFilename(func->getLocation()).str().c_str())){
        coop::logger::log_stream << "found function declaration '" << func->getNameAsString() << "' using member '" << memExpr->getMemberDecl()->getNameAsString() << "'";
        coop::logger::out();

        //cache the function node for later traversal
        relevant_functions[func].push_back(memExpr);

        static int function_idx = 0;
        if(function_idx_mapping.count(func) == 0){
            //the key is not present in the map yet
            function_idx_mapping[func] = function_idx++;
        }
    }
}

/*LoopFunctionsCallback*/
void coop::LoopFunctionsCallback::printData(){
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

void coop::LoopFunctionsCallback::run(const MatchFinder::MatchResult &result){
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

/*LoopRegistrationCallback*/

void coop::LoopRegistrationCallback::register_loop(const clang::Stmt* loop){
    if(loop_idx_mapping.count(loop) == 0){
        //loop is not yet registered
        static int loop_count = 0;
        loop_idx_mapping[loop] = loop_count++;
    }
}

void coop::LoopRegistrationCallback::run(const MatchFinder::MatchResult &result){

    const MemberExpr *member = result.Nodes.getNodeAs<MemberExpr>(coop_member_s);
    Stmt const *loop_stmt;
    std::stringstream ss;
    bool isForLoop = true;

    if(const ForStmt* loop = result.Nodes.getNodeAs<ForStmt>(coop_loop_s)){
        SourceManager &srcMgr = result.Context->getSourceManager();
        if(is_user_source_file(srcMgr.getFilename(loop->getForLoc()).str().c_str())){
            loop_stmt = loop;
            get_for_loop_identifier(loop, &srcMgr, ss);
            coop::logger::log_stream << "found 'for loop' " << ss.str() << " iterating '" << member->getMemberDecl()->getNameAsString().c_str() << "'";
            coop::logger::out();
        }
    }else if(const WhileStmt* loop = result.Nodes.getNodeAs<WhileStmt>(coop_loop_s)){
        SourceManager &srcMgr = result.Context->getSourceManager();
        if(is_user_source_file(srcMgr.getFilename(loop->getWhileLoc()).str().c_str())){
            loop_stmt = loop;
            isForLoop = false;
            get_while_loop_identifier(loop, &srcMgr, ss);
            coop::logger::log_stream << "found 'while loop' " << ss.str() << " iterating '" << member->getMemberDecl()->getNameAsString().c_str() << "'";
            coop::logger::out();
        }
    }
    auto loop_info = &loops[loop_stmt];
    loop_info->identifier = ss.str();
    loop_info->isForLoop = isForLoop;
    loop_info->member_usages.push_back(member);
    coop::LoopRegistrationCallback::register_loop(loop_stmt);
}

/*NestedLoopCallback*/
void coop::NestedLoopCallback::run(const MatchFinder::MatchResult &result){
    SourceManager &srcMgr = result.Context->getSourceManager();

    //find parent loop
    {
        std::stringstream ss;
        if(const ForStmt *for_loop_parent = result.Nodes.getNodeAs<ForStmt>(coop_parent_for_loop_s)){
            get_for_loop_identifier(for_loop_parent, &srcMgr, ss);
            coop::logger::log_stream << "found PARENT 'for loop' " << ss.str();
        }else if(const WhileStmt *while_loop_parent = result.Nodes.getNodeAs<WhileStmt>(coop_parent_while_loop_s)){
            get_while_loop_identifier(while_loop_parent, &srcMgr, ss);
            coop::logger::log_stream << "found PARENT 'while loop' " << ss.str();
        }
        coop::logger::log_stream << " parenting -> ";
    }

    //find child loop
    {
        std::stringstream ss;
        if(const ForStmt *for_loop_child = result.Nodes.getNodeAs<ForStmt>(coop_for_loop_s)){
            get_for_loop_identifier(for_loop_child, &srcMgr, ss);
            coop::logger::log_stream << "'for loop' " << ss.str();
        }else if(const WhileStmt *while_loop_child = result.Nodes.getNodeAs<WhileStmt>(coop_while_loop_s)){
            get_while_loop_identifier(while_loop_child, &srcMgr, ss);
            coop::logger::log_stream << "'while loop' " << ss.str();
        }
    }
    coop::logger::out();
}



void coop::record::record_info::init(
    const clang::RecordDecl* class_struct,
    std::vector<const clang::FieldDecl*> *field_vector,
    std::map<const clang::FunctionDecl*, std::vector<const clang::MemberExpr*>> *rlvnt_funcs,
    std::map<const Stmt*, loop_credentials> *rlvnt_loops)
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

std::vector<const MemberExpr*>* coop::record::record_info::isRelevantFunction(const clang::FunctionDecl* func){
    auto funcs_iter = relevant_functions->find(func);
    if(funcs_iter != relevant_functions->end()){
        return &funcs_iter->second;
    }
    return nullptr;
}

int coop::record::record_info::isRelevantField(const MemberExpr* memExpr){
    const FieldDecl* field = static_cast<const FieldDecl*>(memExpr->getMemberDecl());
    if(std::find(fields->begin(), fields->end(), field) != fields->end()){
        return member_idx_mapping[field];
    }
    return -1;
}

void coop::record::record_info::print_func_mem_mat(){
    std::function<const char* (const FunctionDecl*)> getNam = [](const FunctionDecl* fd){ return fd->getNameAsString().c_str();};
    print_mat(&fun_mem, getNam);
}
void coop::record::record_info::print_loop_mem_mat(LoopRegistrationCallback* loop_registry){
    std::function<const char* (const Stmt*)> getNam = [loop_registry](const Stmt* ls){ 

        auto loop_iter = loop_registry->loops.find(ls);
        if(loop_iter != loop_registry->loops.end()){
            return loop_iter->second.identifier.c_str();
        }
        return "unidentified loop -> this is most likely a bug!";
    };
    print_mat(&loop_mem, getNam);
}



//namespace specific
namespace coop {
    namespace match {
        DeclarationMatcher classes = cxxRecordDecl(hasDefinition(), unless(isUnion())).bind(coop_class_s);
		DeclarationMatcher members = fieldDecl(hasAncestor(cxxRecordDecl(anyOf(isClass(), isStruct())).bind(coop_class_s))).bind(coop_member_s);
		StatementMatcher members_used_in_functions = memberExpr(hasAncestor(functionDecl().bind(coop_function_s))).bind(coop_member_s);

        StatementMatcher loops = anyOf(forStmt().bind(coop_loop_s), whileStmt().bind(coop_loop_s));
        StatementMatcher loops_distinct = anyOf(forStmt().bind(coop_for_loop_s), whileStmt().bind(coop_while_loop_s));
        StatementMatcher function_calls_in_loops = callExpr(hasAncestor(loops)).bind(coop_function_call_s);
        auto has_loop_descendant = hasDescendant(loops_distinct);
        StatementMatcher nested_loops =
            anyOf(forStmt(has_loop_descendant).bind(coop_parent_for_loop_s),
                  whileStmt(has_loop_descendant).bind(coop_parent_while_loop_s));

        StatementMatcher members_used_in_for_loops =
            memberExpr(hasAncestor(forStmt().bind(coop_loop_s))).bind(coop_member_s);
        StatementMatcher members_used_in_while_loops =
            memberExpr(hasAncestor(whileStmt().bind(coop_loop_s))).bind(coop_member_s);
    }

}