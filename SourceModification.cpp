#include"SourceModification.h"
#include"clang/AST/DeclCXX.h"
#include"coop_utils.hpp"
#include"data.hpp"
#include"MatchCallbacks.hpp"
#include<fstream>

#define STRUCT_NAME "STRUCT_NAME"
#define UNION_NAME "UNION_NAME"
#define EXTERN "EXTERN"
#define STRUCT_FIELDS "STRUCT_FIELDS"
#define DATA_NAME "DATA_NAME"
#define SIZE_COLD "SIZE_COLD"
#define SIZE_HOT "SIZE_HOT"
#define UNION_BYTE_DATA "UNION_BYTE_DATA"
#define UNION_COLD_DATA "UNION_COLD_DATA"
#define UNION_INSTANCE_NAME "UNION_INSTANCE_NAME"
#define COLD_DATA_PTR_NAME "COLD_DATA_PTR_NAME"
#define REINTERPRET_PTR "REINTERPRET_PTR"
#define REINTERPRET_NEXT "REINTERPRET_NEXT"
#define FREE_PTR_NAME "FREE_PTR_NAME"
#define FREE_LIST_NAME "FREE_LIST_NAME"
#define free_list_name_default "coop_free_list"
#define FREE_LIST_INSTANCE_COLD "FREE_LIST_INSTANCE_COLD"
#define FREE_LIST_INSTANCE_HOT "FREE_LIST_INSTANCE_HOT"
#define RECORD_NAME "RECORD_NAME"
#define RECORD_TYPE "RECORD_TYPE"
#define FIELD_INITIALIZERS "FIELD_INITIALIZERS"
#define SEMANTIC "SEMANTIC"

namespace {
    template<typename T>
    std::string get_text(T* t, ASTContext *ast_context){
        SourceManager &src_man = ast_context->getSourceManager();

        clang::SourceLocation b(t->getLocStart()), _e(t->getLocEnd());
        clang::SourceLocation e(clang::Lexer::getLocForEndOfToken(_e, 0, src_man, ast_context->getLangOpts()));

        return std::string(src_man.getCharacterData(b),
            src_man.getCharacterData(e)-src_man.getCharacterData(b));
    }



    void replaceAll(std::string &data,const std::string &to_replace,const std::string &replace_with){
        size_t pos = 0;;
        while((pos = data.find(to_replace, pos)) != std::string::npos){
            data.replace(pos, to_replace.length(), replace_with);
            pos += replace_with.length();
        }
    }

    std::string get_src_mod_file_name(std::string file_name)
    {
        std::stringstream ss;
        ss << coop::getEnvVar(COOP_TEMPLATES_PATH_NAME_S) << "/" << file_name;
        return ss.str();
    }
}

namespace coop{
    namespace src_mod{

        namespace {
            std::map<ASTContext *, Rewriter> ast_rewriter_map = {};

            Rewriter * get_rewriter(ASTContext *ast_context){
                auto iter = ast_rewriter_map.find(ast_context);
                if(iter != ast_rewriter_map.end()){
                    return &iter->second;
                }
                Rewriter &rewriter = ast_rewriter_map[ast_context];
                rewriter.setSourceMgr(ast_context->getSourceManager(), ast_context->getLangOpts());
                return &rewriter;
            }
            Rewriter * get_rewriter(ASTContext &ast_context){
                return get_rewriter(&ast_context);
            }
        }

        void apply_changes(){
            for(auto &ast_rewriter : ast_rewriter_map){
                auto &rewriter = ast_rewriter.second;
                rewriter.overwriteChangedFiles();
            }
        }

        void include_file(const char *target_file, const char *include)
        {
            std::stringstream ss;
            std::ifstream ifs(target_file);
            std::string tmpl_file_content(
                (std::istreambuf_iterator<char>(ifs)),
                (std::istreambuf_iterator<char>()));

            //if the file already includes the 'include' dont do it
            if(tmpl_file_content.find(coop::naming::get_relevant_token(include)) != std::string::npos){
                return;
            }


            ss << "#include \"" << include << "\"\n" << tmpl_file_content;

            std::ofstream ofs("coop_tmp");
            if(ofs.is_open()){
                ofs << ss.str();
            }
            ofs.close();

            ss.str("");
            ss << "cp coop_tmp " << target_file << " && rm coop_tmp";
            if(system(ss.str().c_str()) != 0){
                coop::logger::log_stream << "SourceModification.cpp -> call to system failed!";
                coop::logger::err(coop::Should_Exit::YES);
            }
        }

        void include_free_list_hpp(const char *file_path, const char *include_path_to_add){
            std::stringstream ss;
            std::ifstream ifs(file_path);
            std::string tmpl_file_content(
                (std::istreambuf_iterator<char>(ifs)),
                (std::istreambuf_iterator<char>()));
            ss << "#include \"" << include_path_to_add << "\"\n" << tmpl_file_content;

            std::ofstream ofs("coop_tmp");
            if(ofs.is_open()){
                ofs << ss.str();
            }
            ofs.close();

            ss.str("");
            ss << "cp coop_tmp " << file_path << " && rm coop_tmp";
            if(system(ss.str().c_str()) != 0){
                coop::logger::out("[ERROR]::SourceModification.cpp -> call to system failed!");
            }
        }

        void remove_decl(const FieldDecl *fd){
            Rewriter *rewriter = get_rewriter(fd->getASTContext());

            rewriter->setSourceMgr(fd->getASTContext().getSourceManager(), fd->getASTContext().getLangOpts());
            auto src_range = fd->getSourceRange();
            rewriter->RemoveText(src_range.getBegin(), rewriter->getRangeSize(src_range)+1); //the plus 1 gets rid of the semicolon
        }

        void create_cold_struct_for(
            coop::record::record_info *ri,
            cold_pod_representation *cpr,
            std::string user_include_path,
            size_t allocation_size_hot_data,
            size_t allocation_size_cold_data)
        {
            const RecordDecl* rd = ri->record;
            std::stringstream ss;
            ss << getEnvVar(COOP_TEMPLATES_PATH_NAME_S) << "/" << "cold_struct_template.cpp";
            std::ifstream ifs(ss.str());
            if(!ifs.good())
            {
                coop::logger::log_stream << "could not open file: '" << ss.str() << "'";
                coop::logger::err();
            }
            ss.str("");
            std::string tmpl_file_content(
                (std::istreambuf_iterator<char>(ifs)),
                (std::istreambuf_iterator<char>()));

            //our modification will differ depending on wether the record is defined in a h/hpp or c/cpp file
            //so first determine what it is
            const char *ending = coop::naming::get_from_end_until(cpr->file_name.c_str(), '.');

            cpr->is_header_file = (strcmp(ending, "hpp") == 0) || (strcmp(ending, "h") == 0);

            //determine the generated struct's name
            ss << coop_cold_struct_s << rd->getNameAsString();
            cpr->struct_name = ss.str();
            ss.str("");

            //determine the name of the free list coming with the generated struct
            ss << coop_free_list_name << cpr->record_name;
            cpr->free_list_name = ss.str();
            ss.str("");

            //determine the free list instance's name for the cold data
            ss << "cold_" << coop_free_list_instance_name << cpr->record_name;
            cpr->free_list_instance_name_cold = ss.str();
            ss.str("");

            //determine the free list instance's name for the hot data
            ss << "hot_" << coop_free_list_instance_name << cpr->record_name;
            cpr->free_list_instance_name_hot = ss.str();
            ss.str("");

            //determine the name of the data array that will hold space for the instances
            ss << cpr->record_name << "_cold_data";
            cpr->cold_data_container_name = ss.str();
            ss.str("");

            //determine the name of the data pointer, that will lead to the cold data
            ss << coop_cold_data_pointer_name << "_" << cpr->record_name;
            cpr->cold_data_ptr_name = ss.str();
            ss.str("");

            //assemble all the struct's cold fields
            {
                std::vector<const FieldDecl*> in_class_initialized_fields;
                std::stringstream initializer;
                    initializer << ":";
                for(auto field : ri->cold_fields){

                    ss << get_text(field, &field->getASTContext());
                    if(*ri->cold_fields.end() != field){
                        ss << ";\n";
                    }

                    if(field->hasInClassInitializer()){
                        in_class_initialized_fields.push_back(field);
                    }
                }

                for(size_t i = 0; i < in_class_initialized_fields.size(); ++i){
                    const FieldDecl *field = in_class_initialized_fields[i];
                    initializer << field->getNameAsString() << "("
                        << get_text(field->getInClassInitializer(), &field->getASTContext()) << ")";
                    if(i < in_class_initialized_fields.size()-1){
                        initializer << ", ";
                    }
                }
                replaceAll(tmpl_file_content, FIELD_INITIALIZERS, initializer.str());

            }

            replaceAll(tmpl_file_content, EXTERN, (cpr->is_header_file ? "extern " : ""));

            replaceAll(tmpl_file_content, STRUCT_NAME, cpr->struct_name);
            replaceAll(tmpl_file_content, STRUCT_FIELDS, ss.str());
            ss.str("");

            replaceAll(tmpl_file_content, DATA_NAME, cpr->cold_data_container_name);

            ss << allocation_size_cold_data;
            replaceAll(tmpl_file_content, SIZE_COLD, ss.str());
            ss.str("");

            ss << allocation_size_hot_data;
            replaceAll(tmpl_file_content, SIZE_HOT, ss.str());
            ss.str("");

            replaceAll(tmpl_file_content, RECORD_NAME, cpr->record_name);
            replaceAll(tmpl_file_content, RECORD_TYPE, cpr->rec_info->record->isStruct() ? "struct" : "class");

            if(user_include_path.empty()){
                replaceAll(tmpl_file_content, FREE_LIST_NAME, cpr->free_list_name);
            }else{
                replaceAll(tmpl_file_content, FREE_LIST_NAME, free_list_name_default);
            }
            replaceAll(tmpl_file_content, FREE_LIST_INSTANCE_COLD, cpr->free_list_instance_name_cold);
            replaceAll(tmpl_file_content, FREE_LIST_INSTANCE_HOT, cpr->free_list_instance_name_hot);

            get_rewriter(rd->getASTContext())->
                InsertTextBefore(rd->getSourceRange().getBegin(), tmpl_file_content);
        }


        void create_free_list_for(
            cold_pod_representation *cpr)
        {
            std::stringstream ss;
            ss << coop::getEnvVar(COOP_TEMPLATES_PATH_NAME_S) << "/" << "free_list_template.tmpl";
            std::ifstream ifs(ss.str());
            if(!ifs.good())
            {
                coop::logger::log_stream << "could not open file: " << ss.str();
                coop::logger::err();
            }
            std::string tmpl_file_content(
                (std::istreambuf_iterator<char>(ifs)),
                (std::istreambuf_iterator<char>()));

            replaceAll(tmpl_file_content, FREE_LIST_NAME, cpr->free_list_name);

            get_rewriter(cpr->rec_info->record->getASTContext())->
                InsertTextBefore(cpr->rec_info->record->getSourceRange().getBegin(), tmpl_file_content);
        }

        void add_cpr_ref_to( coop::src_mod::cold_pod_representation *cpr)
        {
            std::stringstream ss;
            ss << coop::getEnvVar(COOP_TEMPLATES_PATH_NAME_S) << "/" << "intrusive_record_addition.cpp";
            std::ifstream ifs(ss.str());
            std::string file_content(
                (std::istreambuf_iterator<char>(ifs)),
                (std::istreambuf_iterator<char>()));

            replaceAll(file_content, STRUCT_NAME, cpr->struct_name);
            replaceAll(file_content, COLD_DATA_PTR_NAME, cpr->cold_data_ptr_name);
            replaceAll(file_content, FREE_LIST_INSTANCE_COLD, cpr->free_list_instance_name_cold);
            replaceAll(file_content, FREE_LIST_INSTANCE_HOT, cpr->free_list_instance_name_hot);

            //this function will only be called if ri->cold_fields is not empty, so we can safely take its first element
            //we simply need some place to insert the reference to the cold data to... 
            get_rewriter(cpr->rec_info->record->getASTContext())->
                InsertTextBefore(cpr->rec_info->cold_fields[0]->getLocStart(), file_content);
        }

        void add_memory_allocation_to(
            coop::src_mod::cold_pod_representation *cpr,
            size_t allocation_size_hot_data,
            size_t allocation_size_cold_data)
        {
            return;
            std::stringstream ss;
            ss << coop::getEnvVar(COOP_TEMPLATES_PATH_NAME_S) << "/" << "memory_allocation_template.cpp";
            std::ifstream ifs(ss.str());
            ss.str("");
            std::string file_content(
                (std::istreambuf_iterator<char>(ifs)),
                (std::istreambuf_iterator<char>()));

            replaceAll(file_content, EXTERN, (cpr->is_header_file ? "extern " : ""));
            replaceAll(file_content, DATA_NAME, cpr->cold_data_container_name);
            replaceAll(file_content, RECORD_NAME, cpr->record_name);
            if(cpr->user_include_path.empty()){
                replaceAll(file_content, FREE_LIST_NAME, cpr->free_list_name);
            }else{
                replaceAll(file_content, FREE_LIST_NAME, free_list_name_default);
            }
            replaceAll(file_content, STRUCT_NAME, cpr->struct_name);
            ss << allocation_size_cold_data;
            replaceAll(file_content, SIZE_COLD, ss.str());
            ss.str("");
            ss << allocation_size_hot_data;
            replaceAll(file_content, SIZE_HOT, ss.str());
            ss.str("");
            replaceAll(file_content, FREE_LIST_INSTANCE_COLD, cpr->free_list_instance_name_cold);
            replaceAll(file_content, FREE_LIST_INSTANCE_HOT, cpr->free_list_instance_name_hot);

            get_rewriter(cpr->rec_info->record->getASTContext())->
                InsertTextAfterToken(cpr->rec_info->record->getSourceRange().getEnd(), file_content);
        }

        void redirect_memExpr_to_cold_struct(
            const MemberExpr *mem_expr,
            const FieldDecl *field,
            cold_pod_representation *cpr,
            ASTContext *ast_context)
        {
            std::string field_decl_name = mem_expr->getMemberDecl()->getNameAsString();

            std::stringstream ss;
            ss << coop_safe_struct_acces_method_name << "->" << field_decl_name;

            //usage_text.replace(
            //    usage_text.find(field_decl_name),
            //    field_decl_name.length(),
            //    ss.str());

            get_rewriter(ast_context)->
                ReplaceText(mem_expr->getMemberLoc(), ss.str());
        }

        //TODO DELETE THIS
        void isNull(std::string n, const void *ptr){
            if(!ptr)
            {
                coop::logger::log_stream << n << "IS NULL";
                coop::logger::out();
            }
        }

        void handle_operators(cold_pod_representation *cpr)
        {
            auto record_decl = coop::global<RecordDecl>::get_global(cpr->rec_info->record)->ptr;

            //copy assignment
            auto copy_op = coop::FindCopyAssignmentOperators::rec_copy_assignment_operator_map.find(record_decl);
            if(copy_op == coop::FindCopyAssignmentOperators::rec_copy_assignment_operator_map.end())
            {
                //generate code to copy EACH field
                std::stringstream semantic;

                //make sure the copy will have its own data (emulating deep copy so tmp objects will not destroy shared cold data)
                std::ifstream ifs(get_src_mod_file_name("copy_assignment_operator_deep_copy_emulation.cpp"));
                std::string code(
                    (std::istreambuf_iterator<char>(ifs)),
                    (std::istreambuf_iterator<char>()));

                replaceAll(code, RECORD_NAME, cpr->record_name);
                replaceAll(code, COLD_DATA_PTR_NAME, cpr->cold_data_ptr_name);
                replaceAll(code, FREE_LIST_INSTANCE_COLD, cpr->free_list_instance_name_cold);
                replaceAll(code, STRUCT_NAME, cpr->struct_name);

                //the field copies
                auto fields = cpr->rec_info->fields;
                auto cold_fields = cpr->rec_info->cold_fields;
                for(auto field : fields)
                {
                    auto type = field->getType();

                    //except for stuff we literally can't copy...
                    if(type.isConstQualified() || type.isLocalConstQualified() || dyn_cast_or_null<ConstantArrayType>(type.getTypePtr()))
                    {
                        continue;
                    }

                    bool is_cold = std::find(cold_fields.begin(), cold_fields.end(), field) != cold_fields.end();
                    if(is_cold) semantic << cpr->cold_data_ptr_name << "->";
                    semantic << field->getNameAsString() << " = other_obj.";
                    if(is_cold) semantic << cpr->cold_data_ptr_name << "->";
                    semantic << field->getNameAsString() << ";\n";
                }

                replaceAll(code, SEMANTIC, semantic.str());
                cpr->missing_mandatory << code;
            }
        }

        void handle_constructors(cold_pod_representation *cpr)
        {
            //create the strings that handle stuff
            auto record_decl = coop::global<RecordDecl>::get_global(cpr->rec_info->record)->ptr;

            //create the code for general constructors
            std::ifstream ifs(get_src_mod_file_name("constructor_deep_copy_emulation.cpp"));
            std::string constructor_code(
                (std::istreambuf_iterator<char>(ifs)),
                (std::istreambuf_iterator<char>()));
            replaceAll(constructor_code, COLD_DATA_PTR_NAME, cpr->cold_data_ptr_name);
            replaceAll(constructor_code, FREE_LIST_INSTANCE_COLD, cpr->free_list_instance_name_cold);
            replaceAll(constructor_code, STRUCT_NAME, cpr->struct_name);

            //check whether or not the record has the respective methods 
            //if so inject the code where it belongs
            //else wrap the code to be injected as methods and mark it as missing_mandatory so it is injected later on

            //constructor
            auto ctors = coop::FindConstructor::rec_constructor_map[record_decl];
            if(ctors.empty())
            {
                //there is no ctor -> create one
                coop::logger::out("no constructors found");
                cpr->missing_mandatory << "\n" << cpr->record_name << "(){" << constructor_code << "}\n";
            }else{
                for(auto ctor : ctors)
                {
                    //inject the code to the existing ctor
                    //get the ctors location
                    auto r = get_rewriter(ctor->getASTContext());
                    r->InsertTextAfterToken(ctor->getBody()->getLocStart(), constructor_code);
                }
            }

            //create the code for copy constructors
            /*if the copy ctor is implicit, then we must now make sure, that not the cold_data_ptr is copied (implicitly)
            but rather ALL fields cold and hot - so we emulate a deep copy and temporary copies of objects don't trigger
            the cold data instances to be destructed*/
            auto copy_ctor = coop::FindConstructor::rec_copy_constructor_map.find(record_decl);
            if(copy_ctor == coop::FindConstructor::rec_copy_constructor_map.end())
            {
                //there is no definition of a copy constructor - so we make one
                ifs = std::ifstream (get_src_mod_file_name("existing_copy_constructor_deep_copy_emulation.cpp"));
                std::string copy_constructor_code(
                    (std::istreambuf_iterator<char>(ifs)),
                    (std::istreambuf_iterator<char>()));
                replaceAll(copy_constructor_code, RECORD_NAME, cpr->record_name);
                cpr->missing_mandatory << copy_constructor_code;
            }

        }

        void handle_free_list_fragmentation(cold_pod_representation *cpr)
        {
            const CXXDestructorDecl *dtor_decl = cpr->rec_info->destructor_ptr;

            Rewriter *rewriter = get_rewriter(dtor_decl->getASTContext());

            //define the statement, that calls the free method of the freelist with the pointer to the cold_struct
            std::stringstream free_stmt;
            free_stmt << "//marks the cold_data_freelist's cold_struct instance as reusable\n"
               << cpr->cold_data_ptr_name << "->~"
               << cpr->struct_name << "();\n"
               << cpr->free_list_instance_name_cold
               << ".free("<< cpr->cold_data_ptr_name <<");";

            if(dtor_decl){
                coop::logger::out("found dtor_decl");
                const Stmt *dtor_body = cpr->rec_info->destructor_ptr->getBody();
                if(dtor_body && dtor_decl->isUserProvided()){
                    //insert the relevant code to the existing constructor
                    rewriter->InsertTextBefore(dtor_body->getLocEnd(), "\n");
                    rewriter->InsertTextBefore(dtor_body->getLocEnd(), free_stmt.str());
                }else{
                    //has no destructor body
                    const RecordDecl* record_decl = cpr->rec_info->record;
                    //now we can't even be sure that the rewriter fits the ast - multiple definitions can exist, since there are multiple ASTs all containing the same headers
                    rewriter = get_rewriter(record_decl->getASTContext());

                    //make sure to only work with the global instances
                    auto global_rec = coop::global<RecordDecl>::get_global(record_decl);
                    if(!global_rec)
                    {
                        coop::logger::log_stream << "found no global node for: " << record_decl->getNameAsString();
                        coop::logger::out();
                        return;
                    }
                    record_decl = global_rec->ptr;

                    //since there is no existing definition of a destructor - we make one
                    //missing_mandatory will be added in conclusion
                    cpr->missing_mandatory << "\n~" << record_decl->getNameAsString() << "()\n{\n" << free_stmt.str() << "\n}\n";
                }
            }
        }

        void handle_new_instantiation(
            cold_pod_representation *cpr,
            std::pair<const CXXNewExpr*, ASTContext*> &expr_ctxt)
        {
            std::string new_replacement = get_text(expr_ctxt.first, expr_ctxt.second);
            std::stringstream ss;
            ss << "new(" << cpr-> qualifier << cpr->free_list_instance_name_hot << ".get())";
            replaceAll(new_replacement, "new", ss.str());

            get_rewriter(expr_ctxt.second)->
                ReplaceText(expr_ctxt.first->getSourceRange(), new_replacement);
        }

        void handle_delete_calls(
            cold_pod_representation *cpr,
            std::pair<const CXXDeleteExpr*, ASTContext*> &del_ctxt)
        {
            Rewriter *rewriter = get_rewriter(del_ctxt.second);

            std::string instance_name = get_text(del_ctxt.first->getArgument(), del_ctxt.second);
            
            std::stringstream deletion_replacement;
            deletion_replacement << instance_name << "->~" << cpr->record_name << "();";
            rewriter->ReplaceText(del_ctxt.first->getSourceRange(), deletion_replacement.str());

            //after the actual destruction of the instance we need to inform the freelist
            std::stringstream deletion_addition;
            deletion_addition << "\n//marks the cold_data_freelist's cold_struct instance as reusable\n"
                << cpr->qualifier
                << cpr->free_list_instance_name_hot
                << ".free(" << instance_name << ")";

            rewriter->InsertTextAfterToken(del_ctxt.first->getSourceRange().getEnd(), deletion_addition.str());
        }

        void define_free_list_instances(
            cold_pod_representation *cpr,
            const FunctionDecl *main_function_node,
            size_t allocation_size_hot_data,
            size_t allocation_size_cold_data)
        {
            std::stringstream ss;
            ss << coop::getEnvVar(COOP_TEMPLATES_PATH_NAME_S) << "/" << "free_list_definition.cpp";
            std::ifstream ifs(ss.str());
            ss.str("");
            std::string file_content(
                (std::istreambuf_iterator<char>(ifs)),
                (std::istreambuf_iterator<char>()));

            if(cpr->user_include_path.empty()){
                replaceAll(file_content, FREE_LIST_NAME, cpr->free_list_name);
            }else{
                replaceAll(file_content, FREE_LIST_NAME, free_list_name_default);
            }
            replaceAll(file_content, RECORD_NAME, cpr->qualified_record_name);
            ss << cpr->qualifier << cpr->struct_name;

            replaceAll(file_content, STRUCT_NAME, ss.str().c_str());
            ss.str("");
            ss << cpr->qualifier << cpr->free_list_instance_name_hot;
            replaceAll(file_content, FREE_LIST_INSTANCE_HOT, ss.str().c_str());
            ss.str("");
            ss << cpr->qualifier << cpr->free_list_instance_name_cold;
            replaceAll(file_content, FREE_LIST_INSTANCE_COLD, ss.str().c_str());

            ss.str("");
            ss << allocation_size_hot_data;
            replaceAll(file_content, SIZE_HOT, ss.str().c_str());
            ss.str("");
            ss << allocation_size_cold_data;
            replaceAll(file_content, SIZE_COLD, ss.str().c_str());

            get_rewriter(main_function_node->getASTContext())->
                InsertTextBefore(main_function_node->getLocStart(), file_content);
        }


        void handle_missing_mandatory(cold_pod_representation *cpr)
        {
            auto rec = cpr->rec_info->record;
            auto &ast_ctxt = rec->getASTContext();
            auto r = get_rewriter(ast_ctxt);
            r->InsertTextBefore(rec->getLocEnd(), cpr->missing_mandatory.str());
        }
    }
}