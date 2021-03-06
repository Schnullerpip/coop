//custom includes
#include"MatchCallbacks.hpp"
#include"SourceModification.h"
#include"data.hpp"
#include"naming.hpp"
#include"data.hpp"

//static variables
std::map<const CXXRecordDecl*, std::set<const FieldDecl*>>
    coop::MemberRegistrationCallback::class_fields_map = {};

std::map<const CXXRecordDecl*, std::string>
    coop::MemberRegistrationCallback::class_file_map = {};


std::map<const CXXRecordDecl*, std::set<const AccessSpecDecl*>>
    coop::AccessSpecCallback::record_public_access_map = {};

std::map<const CXXRecordDecl*, std::set<const AccessSpecDecl*>>
    coop::AccessSpecCallback::record_private_access_map = {};

std::set<std::pair<const FunctionDecl*, std::string>>
    coop::FunctionPrototypeRegistrationCallback::function_prototypes = {};

FunctionDecl const *
    coop::FunctionRegistrationCallback::main_function_ptr = nullptr;
std::string
    coop::FunctionRegistrationCallback::main_file = "";

std::map<const clang::Stmt*, coop::loop_credentials>
    coop::LoopMemberUsageCallback::loops = {};

std::map<const clang::Stmt*, int>
    coop::LoopMemberUsageCallback::loop_idx_mapping = {};

std::map<const FunctionDecl*, std::vector<const MemberExpr*>>
    coop::FunctionRegistrationCallback::relevant_functions = {};

std::map<const FunctionDecl*, int>
    coop::FunctionRegistrationCallback::function_idx_mapping = {};

std::map<const FieldDecl*, std::vector<coop::ColdFieldCallback::memExpr_ASTcon>>
    coop::ColdFieldCallback::cold_field_occurances = {};

std::map<const CXXRecordDecl*, std::vector<std::pair<const CXXNewExpr*, ASTContext*>>>
    coop::FindInstantiations::instantiations_map = {};

std::map<const CXXRecordDecl*, std::vector<std::pair<const CXXDeleteExpr*, ASTContext*>>>
    coop::FindDeleteCalls::delete_calls_map = {};

std::map<const CXXRecordDecl*, const CXXMethodDecl*>
    coop::FindCopyAssignmentOperators::rec_copy_assignment_operator_map;

std::map<const CXXRecordDecl*, const CXXMethodDecl*>
    coop::FindMoveAssignmentOperators::rec_move_assignment_operator_map;

std::map<const CXXRecordDecl*, std::vector<const CXXConstructorDecl*>>
    coop::FindConstructor::rec_constructor_map;
std::map<const CXXRecordDecl*, std::vector<const CXXConstructorDecl*>>
    coop::FindConstructor::rec_copy_constructor_map;
std::map<const CXXRecordDecl*, std::vector<const CXXConstructorDecl*>>
    coop::FindConstructor::rec_move_constructor_map;

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

void coop::CoopMatchCallback::get_for_loop_identifier(const ForStmt* loop, SourceManager *srcMgr, std::stringstream *dest){
    const char *fileName = coop::naming::get_relevant_token(srcMgr->getFilename(loop->getForLoc()).str().c_str());
    *dest << "[F:" << fileName << ":" << srcMgr->getPresumedLineNumber(loop->getLocStart()) << "]";
}
void coop::CoopMatchCallback::get_while_loop_identifier(const WhileStmt* loop, SourceManager *srcMgr, std::stringstream *dest){
    const char *fileName = coop::naming::get_relevant_token(srcMgr->getFilename(loop->getWhileLoc()).str().c_str());
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
    const CXXRecordDecl *rd = result.Nodes.getNodeAs<CXXRecordDecl>(coop_class_s);
    std::string rd_id = coop::naming::get_decl_id<CXXRecordDecl>(rd);
    ptr_ID<CXXRecordDecl> * global = coop::global<CXXRecordDecl>::get_global(rd_id);

    if(global){
        //we already found this record declaration - do nothing
        if(rd->hasBody() && !global->ptr->hasBody())
        {
            global->ptr = rd;
            global->ast_context = result.Context;
        }
        return;
    }else{
        coop::global<CXXRecordDecl>::set_global(rd, rd_id);
    }

    std::string fileName = result.SourceManager->getFilename(rd->getLocation()).str();

    class_file_map[rd] = fileName;

    if(!rd->field_empty()){
        for(auto f : rd->fields()){
            class_fields_map[rd].insert(coop::global<FieldDecl>::use(f)->ptr);
        }
    }
}


/*AccessSpecCallback*/

void coop::AccessSpecCallback::run(const MatchFinder::MatchResult &result)
{
    const CXXRecordDecl *record_decl = result.Nodes.getNodeAs<CXXRecordDecl>(coop_class_s);
    const AccessSpecDecl *acc_decl = result.Nodes.getNodeAs<AccessSpecDecl>(coop_access_s);

    //make sure only to work with the global instances
    auto global_rec = coop::global<CXXRecordDecl>::use(record_decl);
    record_decl = global_rec->ptr;

    if(record_decl)
    {
        bool isPublic, isPrivate;
        auto access_spec = acc_decl->getAccess();
        if((isPublic =  (access_spec == clang::AccessSpecifier::AS_public)))
        {
            auto publics = record_public_access_map[record_decl];
            publics.insert(acc_decl);
        }
        else if((isPrivate = (access_spec == clang::AccessSpecifier::AS_private)))
        {
            auto privates = record_private_access_map[record_decl];
            privates.insert(acc_decl);
        }
        //right now we dont care for protected
    }

}

/*FunctionPrototypeRegistrationCallback*/
void coop::FunctionPrototypeRegistrationCallback::run(const MatchFinder::MatchResult &result){
    const FunctionDecl *proto = result.Nodes.getNodeAs<FunctionDecl>(coop_function_s);
    const FunctionDecl *func = proto->getDefinition();

    //make sure to match only those prototypes, that we can find a definition for
    if(!func){return;}

    static coop::unique ids;
    std::string id = coop::naming::get_decl_id<FunctionDecl>(proto);
    if(ids.check(id)){
        //already registered this one - do nothing
        return;
    }

    //we store the prototypes id with the definitions pointer
    //this way in other TUs that will only be able to find the prototype, they will be able
    //to associate the definition

    //make sure to overwrite an existing global ptr_id for this prototype (could have been found and registered by other callbacks before)
    auto global_f = coop::global<FunctionDecl>::get_global(id);
    if(global_f){
        //overwrite the pointer it associates to the pointer that holds the functions definition
        global_f->ptr = func;
        global_f->ast_context = result.Context;
    }else{
        //there is no global registration for this ptr_id - create one
        coop::global<FunctionDecl>::set_global(func, id, result.Context);
    }
}

/*FunctionRegistrationCallback*/
void coop::FunctionRegistrationCallback::run(const MatchFinder::MatchResult &result){
    const FunctionDecl* func = result.Nodes.getNodeAs<FunctionDecl>(coop_function_s);
    if(!func->isThisDeclarationADefinition()){
        //this is probably just a function header - dont mind it, since this is nothing we want to change
        return;
    }
    auto global_func = coop::global<FunctionDecl>::use(func);
    func = global_func->ptr;

    const MemberExpr* memExpr = result.Nodes.getNodeAs<MemberExpr>(coop_member_s);

    //prevent redundant registration and guarantee unique access points (nodes)
    std::string id = coop::naming::get_stmt_id<MemberExpr>(memExpr, result.SourceManager);
    auto global_memExpr = coop::global<MemberExpr>::get_global(id);
    if(global_memExpr){
        //already registered this member Expression - do nothing
        return;
    }
    else {
        coop::global<MemberExpr>::set_global(memExpr, id, result.Context);
    }

    if(!coop::FunctionRegistrationCallback::main_function_ptr && func->isMain())
    {
        FunctionRegistrationCallback::main_function_ptr = func;
        FunctionRegistrationCallback::main_file = result.SourceManager->getFilename(main_function_ptr->getLocStart());
    }


    //cache the function node for later traversal
    registerFunction(func);
    relevant_functions[func].push_back(memExpr);
}

bool coop::FunctionRegistrationCallback::isIndexed(const FunctionDecl *func)
{
    return function_idx_mapping.count(func) != 0;
}

void coop::FunctionRegistrationCallback::indexFunction(const FunctionDecl *func)
{
    static int function_idx = 0;
    if(!isIndexed(func)){
        //the key is not present in the map yet
        function_idx_mapping[func] = function_idx++;
    }
}
void coop::FunctionRegistrationCallback::registerFunction(const FunctionDecl *f)
{
    if(!isIndexed(f)){
        relevant_functions.insert({f, {}});
        indexFunction(f);
    }
}

/*ParentedFunctionCallback*/
void coop::ParentedFunctionCallback::run(const MatchFinder::MatchResult &result){
    const FunctionDecl *fn_child = result.Nodes.getNodeAs<FunctionDecl>(coop_child_s);

    //the functioncall can be paranted by either a functionDecl or a for/while loop
    const FunctionDecl *fn_parent = result.Nodes.getNodeAs<FunctionDecl>(coop_parent_s);
    const ForStmt *for_loop_parent = result.Nodes.getNodeAs<ForStmt>(coop_for_loop_s);
    const WhileStmt *while_loop_parent = result.Nodes.getNodeAs<WhileStmt>(coop_while_loop_s);

    coop::fl_node *child_node = nullptr;
    coop::fl_node *parent_node = nullptr;


    //make sure to work with the global instances
    auto global_func = coop::global<FunctionDecl>::use(fn_child);
    fn_child = global_func->ptr;

    if(AST_abbreviation::function_nodes.count(fn_child) == 0)
    {
        //the node does not exist yet
        child_node = new coop::fl_node(global_func); 
        AST_abbreviation::function_nodes[fn_child] = child_node;
    } else{ child_node = AST_abbreviation::function_nodes[fn_child]; }

    //if the parent is a loop or a function decides the further steps
    if(fn_parent){
        global_func = coop::global<FunctionDecl>::use(fn_parent);
        fn_parent = global_func->ptr;

        if(AST_abbreviation::function_nodes.count(fn_parent) == 0)
        {
            //the node does not exist yet
            parent_node = new coop::fl_node(global_func); 
            AST_abbreviation::function_nodes[fn_parent] = parent_node;
        } else{ parent_node = AST_abbreviation::function_nodes[fn_parent]; }
    }else{
        const Stmt *loop = nullptr;
        std::string loop_name;
        bool is_for_loop = false;
        if(for_loop_parent){
            loop = for_loop_parent;
            loop_name = coop::naming::get_for_loop_identifier(for_loop_parent, result.SourceManager);
            is_for_loop = true;
        }else if(while_loop_parent){
            loop = while_loop_parent;
            loop_name = coop::naming::get_while_loop_identifier(while_loop_parent, result.SourceManager);
        }else{
            coop::logger::clear();
            return;
        }

        auto global_loop = coop::global<Stmt>::use(loop, loop_name, result.Context);
        if(AST_abbreviation::loop_nodes.count(loop) == 0)
        {
            //the node does not exist yet
            parent_node = new coop::fl_node(global_loop, is_for_loop);
            AST_abbreviation::loop_nodes[loop] = parent_node;
        }else{ parent_node = AST_abbreviation::loop_nodes[loop]; }
    }

    parent_node->insert_child(child_node);
}

/*ParentedLoopCallback*/
void coop::ParentedLoopCallback::run(const MatchFinder::MatchResult &result){
    const Stmt *child = nullptr;
    std::string child_name;
    const ForStmt *l_f_child;
    const WhileStmt *l_w_child;
    coop::fl_node *child_node = nullptr;
    coop::fl_node *parent_node = nullptr;

    //child is either a for loop or a while loop
    bool is_for_loop = false;
    if((l_f_child = result.Nodes.getNodeAs<ForStmt>(coop_child_for_loop_s)))
    {
        child = l_f_child;
        child_name = coop::naming::get_for_loop_identifier(l_f_child, result.SourceManager);
        is_for_loop = true;
    }
    else if((l_w_child = result.Nodes.getNodeAs<WhileStmt>(coop_child_while_loop_s)))
    {
        child = l_w_child;
        child_name = coop::naming::get_while_loop_identifier(l_w_child, result.SourceManager);
    }
    else {
        return;
    }

    //make sure to work with the global instances
    auto global_child = coop::global<Stmt>::use(child, child_name, result.Context);
    child = global_child->ptr;

    if(AST_abbreviation::loop_nodes.count(global_child->ptr) == 0)
    {
        //the node does not exist yet
        child_node = new coop::fl_node(global_child, is_for_loop); 
        AST_abbreviation::loop_nodes[global_child->ptr] = child_node;
    } else{ child_node = AST_abbreviation::loop_nodes[global_child->ptr]; }

    //the loop can be paranted by either a functionDecl or a for/while loop
    const FunctionDecl *fn_parent = result.Nodes.getNodeAs<FunctionDecl>(coop_parent_s);
    const ForStmt *for_loop_parent = result.Nodes.getNodeAs<ForStmt>(coop_for_loop_s);
    const WhileStmt *while_loop_parent = result.Nodes.getNodeAs<WhileStmt>(coop_while_loop_s);

    //if the parent is a loop or a function decides the further steps
    if(fn_parent){
        auto global_func = coop::global<FunctionDecl>::use(fn_parent);
        fn_parent = global_func->ptr;

        if(AST_abbreviation::function_nodes.count(fn_parent) == 0)
        {
            //the node does not exist yet
            parent_node = new coop::fl_node(global_func); 
            AST_abbreviation::function_nodes[fn_parent] = parent_node;
        } else{ parent_node = AST_abbreviation::function_nodes[fn_parent]; }
    }else{
        const Stmt *loop = nullptr;
        std::string loop_name;
        bool is_for_loop = false;
        if(for_loop_parent){
            loop = for_loop_parent;
            loop_name = coop::naming::get_for_loop_identifier(for_loop_parent, result.SourceManager);
            is_for_loop = true;
        }else if(while_loop_parent){
            loop = while_loop_parent;
            loop_name = coop::naming::get_while_loop_identifier(while_loop_parent, result.SourceManager);
        }else{
            coop::logger::clear();
            return;
        }

        auto global_loop = coop::global<Stmt>::use(loop, loop_name, result.Context);
        if(AST_abbreviation::loop_nodes.count(loop) == 0)
        {
            //the node does not exist yet
            parent_node = new coop::fl_node(global_loop, is_for_loop);
            AST_abbreviation::loop_nodes[loop] = parent_node;
        }else{ parent_node = AST_abbreviation::loop_nodes[loop]; }
    }


    parent_node->insert_child(child_node);
}


/*LoopMemberUsageCallback*/
bool coop::LoopMemberUsageCallback::isIndexed(const clang::Stmt* loop){
    return loop_idx_mapping.count(loop) != 0;
}
void coop::LoopMemberUsageCallback::indexLoop(const clang::Stmt* loop){
    if(!isIndexed(loop)){
        //loop is not yet registered
        static int loop_count = 0;
        loop_idx_mapping[loop] = loop_count++;
    }
}
void coop::LoopMemberUsageCallback::registerLoop(const clang::Stmt* loop, std::string loop_name ,bool isForLoop)
{
    if(!isIndexed(loop))
    {
        indexLoop(loop);
        auto loop_info = &loops[loop];
        loop_info->identifier = loop_name;
        loop_info->isForLoop = isForLoop;
    }
}

void coop::LoopMemberUsageCallback::run(const MatchFinder::MatchResult &result){

    const MemberExpr *member = result.Nodes.getNodeAs<MemberExpr>(coop_member_s);
    Stmt const *loop_stmt;
    bool isForLoop = true;

    //prevent redundant memberUsage registration (due to multiple includes of the same h/hpp file)
    static coop::unique member_ids;
    if(member_ids.check(coop::naming::get_stmt_id<MemberExpr>(member, result.SourceManager))){
        //already registered this member usage - do nothing
        return;
    }

    std::string loop_name, member_id = coop::naming::get_stmt_id<MemberExpr>(member, result.SourceManager);
    if(const ForStmt* loop = result.Nodes.getNodeAs<ForStmt>(coop_loop_s)){
        SourceManager &srcMgr = result.Context->getSourceManager();
        loop_stmt = loop;
        loop_name = coop::naming::get_for_loop_identifier(loop, &srcMgr);
    }else if(const WhileStmt* loop = result.Nodes.getNodeAs<WhileStmt>(coop_loop_s)){
        SourceManager &srcMgr = result.Context->getSourceManager();
        loop_stmt = loop;
        isForLoop = false;
        loop_name = coop::naming::get_while_loop_identifier(loop, &srcMgr);
    }

    //make sure to only use the global versions / the unique access points to the nodes
    loop_stmt = coop::global<Stmt>::use(loop_stmt, loop_name, result.Context)->ptr;
    member = coop::global<MemberExpr>::use(member, member_id, result.Context)->ptr;

    registerLoop(loop_stmt, loop_name, isForLoop);
    loops[loop_stmt].member_usages.push_back(member);
}

//TODO DEBUG DELETE THIS
template<typename T>
std::string get_text(T* t, ASTContext *ast_context){
    SourceManager &src_man = ast_context->getSourceManager();

    clang::SourceLocation b(t->getLocStart()), _e(t->getLocEnd());
    clang::SourceLocation e(clang::Lexer::getLocForEndOfToken(_e, 0, src_man, ast_context->getLangOpts()));

    return std::string(src_man.getCharacterData(b),
        src_man.getCharacterData(e)-src_man.getCharacterData(b));
}

void coop::ColdFieldCallback::run(const MatchFinder::MatchResult &result)
{
    const MemberExpr *mem_expr = result.Nodes.getNodeAs<MemberExpr>(coop_member_s);

    if(mem_expr){
        //make sure to search for the global version of this field
        if(auto global_field = coop::global<FieldDecl>::get_global(static_cast<const FieldDecl*>(mem_expr->getMemberDecl()))){

            //This is a 'bug' related hack/workaround, since this matcher will include record-declarations implicitly!!!
            //https://bugs.llvm.org/show_bug.cgi?id=39522
            std::string mem_expr_text = get_text(mem_expr, result.Context);
            const char *relevant_token_dot = coop::naming::get_from_end_until(mem_expr_text.c_str(), '.'); //so foo.a will yield a
            const char *relevant_token_paren = coop::naming::get_from_end_until(mem_expr_text.c_str(), '>'); //so foo->a will yield a

            const FieldDecl *field_decl = global_field->ptr;
            //check if the found member_expr matches a cold field
            auto iter = std::find(fields_to_find->begin(), fields_to_find->end(), field_decl);
            if(iter != fields_to_find->end()){

                //the workaround/hack is to get the actual text of the mem_expr which in case of those implicit record nodes will be the records name
                //we will compate the memExpr's actual text with what it claims to be
                int c1 = strcmp(relevant_token_dot, mem_expr->getMemberDecl()->getNameAsString().c_str()) , c2 = strcmp(relevant_token_paren, mem_expr->getMemberDecl()->getNameAsString().c_str());
                bool match = (c1 == 0) || (c2 == 0);

                if(!match)
                    return;

                //we have found an occurence of a cold field! map it
                //prevent redundant registration due to multiple includes of the same h/hpp file and make sure to only use the global version of this
                static coop::unique mem_ids;
                std::string mem_expr_id = coop::naming::get_stmt_id<MemberExpr>(mem_expr, result.SourceManager);
                if(mem_ids.check(mem_expr_id)){
                    return;
                }

                coop::ColdFieldCallback::cold_field_occurances[field_decl].push_back({ mem_expr, result.Context});
            }
        }
    }
}

void coop::FindDestructor::run(const MatchFinder::MatchResult &result){
    const CXXDestructorDecl *destructor_decl = result.Nodes.getNodeAs<CXXDestructorDecl>(coop_destructor_s);
    if(destructor_decl){
        rec.destructor_ptr = destructor_decl;
    }
}

void coop::FindInstantiations::add_record(const CXXRecordDecl *p)
{
    records_to_instantiate.push_back(p);
}

void coop::FindInstantiations::run(const MatchFinder::MatchResult &result){
    const CXXNewExpr* new_expr = result.Nodes.getNodeAs<CXXNewExpr>(coop_new_instantiation_s);
    if(new_expr && !new_expr->isArray() && (new_expr->placement_arg_begin() == new_expr->placement_arg_end())){
        const CXXRecordDecl *record = new_expr->getAllocatedType().getTypePtr()->getAsCXXRecordDecl();

        //make sure only to work with the global instances
        auto global_rec = coop::global<CXXRecordDecl>::use(record);
        record = global_rec->ptr;

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

void coop::FindDeleteCalls::add_record(const CXXRecordDecl *rd)
{
    record_deletions_to_find.push_back(rd);
}

void coop::FindDeleteCalls::run(const MatchFinder::MatchResult &result){

    const CXXDeleteExpr *delete_call = result.Nodes.getNodeAs<CXXDeleteExpr>(coop_deletion_s);

    if(!delete_call->isArrayForm()){

        const clang::Type *destroyed_type = delete_call->getDestroyedType().getTypePtr();
        const CXXRecordDecl *record_decl = destroyed_type->getAsCXXRecordDecl();

        if(!record_decl)
        {
            //we cant deduce an associated record decl - so this is most certainly not something we want to change!
            return;
        }

        //make sure only to work with the global instances
        auto global_rec = coop::global<CXXRecordDecl>::use(record_decl);
        record_decl = global_rec->ptr;

        if(record_decl){
            //check whether the deletion has something to do with a record that was splitted by coop
            if(std::find(record_deletions_to_find.begin(), record_deletions_to_find.end(), record_decl) != record_deletions_to_find.end()){
                //we found a relevant deletion
                coop::FindDeleteCalls::delete_calls_map[record_decl].push_back({delete_call, result.Context});
            }
        }
    }
}

/*FindConstructor*/
void coop::FindConstructor::add_record(const CXXRecordDecl *rd)
{
    records_to_find.push_back(rd);
}
void coop::FindConstructor::run(const MatchFinder::MatchResult &result){

    const CXXConstructorDecl *constructor = result.Nodes.getNodeAs<CXXConstructorDecl>(coop_constructor_s);

    {
        while(constructor->isDelegatingConstructor())
        {
            constructor = constructor->getTargetConstructor();
        }
    }

    //after delegation resolving or just through other ASTs make sure not to work with redundant data
    coop::unique uniques;
    if(uniques.check(constructor->getNameAsString()))
    {
        return;
    }
    

    const CXXRecordDecl *record_decl = constructor->getParent();
    if(record_decl){

        //make sure to only use global instances 
        auto global_rec = coop::global<CXXRecordDecl>::use(record_decl);
        record_decl = global_rec->ptr;

        //register copy constructors
        if(constructor->isCopyConstructor())
        {
            auto &consts = rec_copy_constructor_map[record_decl];
            consts.push_back(constructor);
            return;
        }
        
        //register move constructors
        if(constructor->isMoveConstructor())
        {
            auto &consts = rec_move_constructor_map[record_decl];
            consts.push_back(constructor);
            return;
        }

        //register other constructors
        auto &consts = rec_constructor_map[record_decl];
        consts.push_back(constructor);
    }
}