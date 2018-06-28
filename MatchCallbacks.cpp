#include "MatchCallbacks.hpp"
#include "SourceModification.h"

//static variables
std::map<const clang::Stmt*, coop::loop_credentials>
    coop::LoopMemberUsageCallback::loops = {};

std::map<const clang::Stmt*, int>
    coop::LoopMemberUsageCallback::loop_idx_mapping = {};

std::map<const clang::Stmt*, std::vector<const clang::Stmt*>>
    coop::NestedLoopCallback::parent_child_map = {};

std::map<const FunctionDecl*, std::vector<const MemberExpr*>>
    coop::FunctionRegistrationCallback::relevant_functions = {};

std::map<const FunctionDecl*, int>
    coop::FunctionRegistrationCallback::function_idx_mapping = {};

std::map<const FieldDecl*, std::vector<coop::ColdFieldCallback::memExpr_ASTcon>>
    coop::ColdFieldCallback::cold_field_occurances = {};

FunctionDecl const *
    coop::FindMainFunction::main_function_ptr = nullptr;

std::map<const RecordDecl*, std::vector<std::pair<const CXXNewExpr*, ASTContext*>>>
    coop::FindInstantiations::instantiations_map = {};

std::map<const RecordDecl*, std::vector<std::pair<const CXXDeleteExpr*, ASTContext*>>>
    coop::FindDeleteCalls::delete_calls_map = {};

//function implementations
std::stringstream file_regex;
size_t file_inputs = 0;
void coop::match::add_file_as_match_condition(const char * file_name){
    if(file_inputs > 0){
        file_regex << "|";
    }
    file_regex << file_name;
    ++file_inputs;
}
std::string coop::match::get_file_regex_match_condition(const char * path_addition){
    std::stringstream return_string;
    return_string << "(" << file_regex.str();
    if(path_addition){
         return_string << "|" << path_addition << (path_addition[strlen(path_addition)] == '/' ? "" : "/") << "*";
    }
    return_string  << ")";
    return return_string.str();
}


//method implementations
const char * coop::CoopMatchCallback::get_relevant_token(const char *file){
    const char *relevant_token;
    size_t iterations = 0;
    for(relevant_token = file+strlen(file); *(relevant_token-1) != '/' && *(relevant_token-1) != '\\' && iterations < strlen(file); --relevant_token){
        ++iterations;
    }
    return relevant_token;
}

void coop::CoopMatchCallback::get_for_loop_identifier(const ForStmt* loop, SourceManager *srcMgr, std::stringstream *dest){
    const char *fileName = get_relevant_token(srcMgr->getFilename(loop->getForLoc()).str().c_str());
    *dest << "[F:" << fileName << ":" << srcMgr->getPresumedLineNumber(loop->getLocStart()) << "]";
}
void coop::CoopMatchCallback::get_while_loop_identifier(const WhileStmt* loop, SourceManager *srcMgr, std::stringstream *dest){
    const char *fileName = get_relevant_token(srcMgr->getFilename(loop->getWhileLoc()).str().c_str());
    *dest << "[W:" << fileName << ":" << srcMgr->getPresumedLineNumber(loop->getLocStart()) << "]";
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

    std::string fileName = result.SourceManager->getFilename(rd->getLocation()).str().c_str();

    coop::logger::log_stream << "found " << rd->getNameAsString().c_str() << " in file: " << get_relevant_token(fileName.c_str());
    coop::logger::out();

    class_file_map[rd] = fileName;

    if(!rd->field_empty()){
        for(auto f : rd->fields()){
            coop::logger::log_stream << "found '" << f->getNameAsString().c_str()
                << "'(" << coop::get_sizeof_in_bits(f) << " bit) in record '"
                << rd->getNameAsString().c_str();
            coop::logger::out();


            class_fields_map[rd].push_back(f);
        }
    }
}

/*FunctionRegistrationCallback*/
void coop::FunctionRegistrationCallback::run(const MatchFinder::MatchResult &result){
    const FunctionDecl* func = result.Nodes.getNodeAs<FunctionDecl>(coop_function_s);
    const MemberExpr* memExpr = result.Nodes.getNodeAs<MemberExpr>(coop_member_s);

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
    std::stringstream ss;

    const FunctionDecl *function_call = result.Nodes.getNodeAs<CallExpr>(coop_function_call_s)->getDirectCallee();
    if(!function_call){
        return;
    }

    coop::logger::log_stream << "found function '" << function_call->getNameAsString() << "' being called in a " ;

    const Stmt *loop;
    bool isForLoop = true;
    if(const ForStmt* for_loop = result.Nodes.getNodeAs<ForStmt>(coop_loop_s)){
        loop = for_loop;
        get_for_loop_identifier(for_loop, &srcMgr, &ss);
        coop::logger::log_stream << "for";
    }else if(const WhileStmt* while_loop = result.Nodes.getNodeAs<WhileStmt>(coop_loop_s)){
        loop = while_loop;
        get_while_loop_identifier(while_loop, &srcMgr, &ss);
        isForLoop = false;
        coop::logger::log_stream << "while";
    }

    coop::logger::log_stream << "Loop " << ss.str();
    coop::logger::out();
    
    loop_function_calls[loop].identifier = ss.str();
    loop_function_calls[loop].isForLoop = isForLoop;
    loop_function_calls[loop].funcs.push_back(function_call);

    /*since here we find really each functionCall made inside a loop, we dont want to directly register this loop
    after the matchers have run over the AST we will aggregate additional data and then filter these loops with information we do not have right now
    remember -> during AST traversal we can never expect the momentary information to be complete*/
}

/*LoopMemberUsageCallback*/
bool coop::LoopMemberUsageCallback::is_registered(const clang::Stmt* loop){
    return loop_idx_mapping.count(loop) != 0;
}
void coop::LoopMemberUsageCallback::register_loop(const clang::Stmt* loop){
    if(!coop::LoopMemberUsageCallback::is_registered(loop)){
        //loop is not yet registered
        static int loop_count = 0;
        loop_idx_mapping[loop] = loop_count++;
    }
}

void coop::LoopMemberUsageCallback::run(const MatchFinder::MatchResult &result){

    const MemberExpr *member = result.Nodes.getNodeAs<MemberExpr>(coop_member_s);
    Stmt const *loop_stmt;
    std::stringstream ss;
    bool isForLoop = true;

    if(const ForStmt* loop = result.Nodes.getNodeAs<ForStmt>(coop_loop_s)){
        SourceManager &srcMgr = result.Context->getSourceManager();
        loop_stmt = loop;
        get_for_loop_identifier(loop, &srcMgr, &ss);
        coop::logger::log_stream << "found 'for loop' " << ss.str() << " iterating '" << member->getMemberDecl()->getNameAsString().c_str() << "'";
        coop::logger::out();
    }else if(const WhileStmt* loop = result.Nodes.getNodeAs<WhileStmt>(coop_loop_s)){
        SourceManager &srcMgr = result.Context->getSourceManager();
        loop_stmt = loop;
        isForLoop = false;
        get_while_loop_identifier(loop, &srcMgr, &ss);
        coop::logger::log_stream << "found 'while loop' " << ss.str() << " iterating '" << member->getMemberDecl()->getNameAsString().c_str() << "'";
        coop::logger::out();
    }
    auto loop_info = &loops[loop_stmt];
    loop_info->identifier = ss.str();
    loop_info->isForLoop = isForLoop;
    loop_info->member_usages.push_back(member);
    coop::LoopMemberUsageCallback::register_loop(loop_stmt);
}

/*NestedLoopCallback*/

void coop::NestedLoopCallback::traverse_parents(std::function<void (std::map<const clang::Stmt *, coop::loop_credentials>::iterator*, std::vector<const Stmt*>*)> callback){
    for(auto parent_childs : NestedLoopCallback::parent_child_map){
        auto parent_iter = LoopMemberUsageCallback::loops.find(parent_childs.first);
        if(parent_iter == LoopMemberUsageCallback::loops.end()){
            continue;
        }
        std::vector<const Stmt*> *children = &parent_childs.second;

        callback(&parent_iter, children);
    }
}
void coop::NestedLoopCallback::traverse_parents_children(std::function<void (std::map<const clang::Stmt *, coop::loop_credentials>::iterator*, const Stmt* child)> callback){
    traverse_parents([&](std::map<const clang::Stmt *, coop::loop_credentials>::iterator *p, std::vector<const Stmt*>* children){
        for(auto c : *children){
            callback(p, c);
        }
    });
}

void coop::NestedLoopCallback::print_data(){
    coop::logger::out("Following 1-tier loop relations were found")++;
    traverse_parents([](std::map<const clang::Stmt *, coop::loop_credentials>::iterator *parent_iter, std::vector<const Stmt*>* children){
        auto parent_info = &(**parent_iter).second;

        coop::logger::log_stream << "parent: " << parent_info->identifier;
        coop::logger::out()++;
        coop::logger::log_stream << "<";
        for(auto c : *children){
            auto child_iter = LoopMemberUsageCallback::loops.find(c);
            if(child_iter == LoopMemberUsageCallback::loops.end()){
                continue;
            }
            coop::loop_credentials *child_info = &(*child_iter).second;
            coop::logger::log_stream << child_info->identifier << ", ";
        }
        coop::logger::log_stream << ">";
        coop::logger::out()--;
    });
    coop::logger::depth--;
}

void coop::NestedLoopCallback::run(const MatchFinder::MatchResult &result){
    SourceManager &srcMgr = result.Context->getSourceManager();
    std::stringstream ss;
    Stmt const * parent_loop, *child_loop;

    //find child loop
    if(const ForStmt *for_loop_child = result.Nodes.getNodeAs<ForStmt>(coop_child_for_loop_s)){
        child_loop = for_loop_child;
        get_for_loop_identifier(for_loop_child, &srcMgr, &ss);
        coop::logger::log_stream << "found CHILD 'for loop' " << ss.str();
    }else if(const WhileStmt *while_loop_child = result.Nodes.getNodeAs<WhileStmt>(coop_child_while_loop_s)){
        child_loop = while_loop_child;
        get_while_loop_identifier(while_loop_child, &srcMgr, &ss);
        coop::logger::log_stream << "found CHILD 'while loop' " << ss.str();
    }
    coop::logger::log_stream << " parented by -> ";

    //match parent loop
    ss.str("");
    if(const ForStmt *for_loop_parent = result.Nodes.getNodeAs<ForStmt>(coop_for_loop_s)){
        parent_loop = for_loop_parent;
        get_for_loop_identifier(for_loop_parent, &srcMgr, &ss);
        coop::logger::log_stream << "'for loop' " << ss.str();
    }else if(const WhileStmt *while_loop_parent = result.Nodes.getNodeAs<WhileStmt>(coop_while_loop_s)){
        parent_loop = while_loop_parent;
        get_while_loop_identifier(while_loop_parent, &srcMgr, &ss);
        coop::logger::log_stream << "'while loop' " << ss.str();
    }
    coop::logger::out();

    NestedLoopCallback::parent_child_map[parent_loop].push_back(child_loop);
}

void coop::ColdFieldCallback::run(const MatchFinder::MatchResult &result)
{
    const MemberExpr *mem_expr = result.Nodes.getNodeAs<MemberExpr>(coop_member_s);
    if(mem_expr){
        if(FieldDecl const *field_decl = (const FieldDecl*) mem_expr->getMemberDecl()){
            //check if the found member_expr matches a cold field
            auto iter = std::find(fields_to_find->begin(), fields_to_find->end(), field_decl);
            if(iter != fields_to_find->end()){
                //we have found an occurence of a cold field! map it
                memExpr_ASTcon e_a{mem_expr, ast_context_ptr};
                coop::ColdFieldCallback::cold_field_occurances[field_decl].push_back(e_a);
            }
        }
    }
}

void coop::FindMainFunction::run(const MatchFinder::MatchResult &result){
    main_function_ptr = result.Nodes.getNodeAs<FunctionDecl>(coop_function_s);
}

void coop::FindDestructor::run(const MatchFinder::MatchResult &result){
    const CXXDestructorDecl *destructor_decl = result.Nodes.getNodeAs<CXXDestructorDecl>(coop_destructor_s);
    if(destructor_decl){
        rec.destructor_ptr = destructor_decl;
    }
}

void coop::FindInstantiations::add_record(const RecordDecl *p)
{
    records_to_instantiate.push_back(p);
}

void coop::FindInstantiations::run(const MatchFinder::MatchResult &result){
    const CXXNewExpr* new_expr = result.Nodes.getNodeAs<CXXNewExpr>(coop_new_instantiation_s);
    if(new_expr && !new_expr->isArray() && (new_expr->placement_arg_begin() == new_expr->placement_arg_end())){
        const RecordDecl *record = new_expr->getAllocatedType().getTypePtr()->getAsCXXRecordDecl();
        //get the new record Type's name
        std::string records_name(record->getNameAsString());
        //check if the instantiation of a new object is of relevant type
        for(auto r : records_to_instantiate){
            if(r->getNameAsString() == records_name){
                coop::FindInstantiations::instantiations_map[record].push_back({new_expr, result.Context});
                break;
            }
        }
    }
}

void coop::FindDeleteCalls::add_record(const RecordDecl *rd)
{
    record_deletions_to_find.push_back(rd);
}

void coop::FindDeleteCalls::run(const MatchFinder::MatchResult &result){

    const CXXDeleteExpr *delete_call = result.Nodes.getNodeAs<CXXDeleteExpr>(coop_deletion_s);
    const DeclRefExpr *deleted_instance_ref = result.Nodes.getNodeAs<DeclRefExpr>(coop_class_s);

    if(!delete_call->isArrayForm()){
        const RecordDecl *record_decl = deleted_instance_ref->getBestDynamicClassType();
        if(record_decl){
            if(std::find(record_deletions_to_find.begin(), record_deletions_to_find.end(), record_decl) != record_deletions_to_find.end()){
                //we found a relevant deletion
                coop::FindDeleteCalls::delete_calls_map[record_decl].push_back({delete_call, result.Context});
            }
        }
    }
}
