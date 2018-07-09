#include"SourceModification.h"
#include"clang/AST/DeclCXX.h"
#include"coop_utils.hpp"
#include"data.hpp"
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
#define FIELD_INITIALIZERS "FIELD_INITIALIZERS"

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
        size_t pos = data.find(to_replace);
        while(pos != std::string::npos){
            data.replace(pos, to_replace.size(), replace_with);
            pos = data.find(to_replace, pos + to_replace.size());
        }
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
            const char * user_include_path,
            size_t allocation_size_hot_data,
            size_t allocation_size_cold_data)
        {
            const RecordDecl* rd = ri->record;
            std::stringstream ss;
            std::ifstream ifs("src_mod_templates/cold_struct_template.cpp");
            std::string tmpl_file_content(
                (std::istreambuf_iterator<char>(ifs)),
                (std::istreambuf_iterator<char>()));

            cpr->rec_info = ri;
            cpr->record_name = ri->record->getNameAsString().c_str();

            //our modification will differ depending on wether the record is defined in a h/hpp or c/cpp file
            //so first determine what it is
            const char *ending = coop::naming::get_from_end_until(cpr->file_name.c_str(), '.');
            if((strcmp(ending, "hpp") != 0) && (strcmp(ending, "h") != 0)){
                cpr->is_header_file = false;
            }

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

            if(user_include_path){
                replaceAll(tmpl_file_content, FREE_LIST_NAME, free_list_name_default);
            }else{
                replaceAll(tmpl_file_content, FREE_LIST_NAME, cpr->free_list_name);
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
            std::ifstream ifs("src_mod_templates/free_list_template.tmpl");
            std::string tmpl_file_content(
                (std::istreambuf_iterator<char>(ifs)),
                (std::istreambuf_iterator<char>()));

            replaceAll(tmpl_file_content, FREE_LIST_NAME, cpr->free_list_name);

            get_rewriter(cpr->rec_info->record->getASTContext())->
                InsertTextBefore(cpr->rec_info->record->getSourceRange().getBegin(), tmpl_file_content);
        }

        void add_cpr_ref_to( coop::src_mod::cold_pod_representation *cpr)
        {
            std::ifstream ifs("src_mod_templates/intrusive_record_addition.cpp");
            std::string file_content(
                (std::istreambuf_iterator<char>(ifs)),
                (std::istreambuf_iterator<char>()));

            replaceAll(file_content, STRUCT_NAME, cpr->struct_name);
            replaceAll(file_content, COLD_DATA_PTR_NAME, coop_cold_data_pointer_name);
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
            std::ifstream ifs("src_mod_templates/memory_allocation_template.cpp");
            std::string file_content(
                (std::istreambuf_iterator<char>(ifs)),
                (std::istreambuf_iterator<char>()));

            replaceAll(file_content, EXTERN, (cpr->is_header_file ? "extern " : ""));
            replaceAll(file_content, DATA_NAME, cpr->cold_data_container_name);
            replaceAll(file_content, RECORD_NAME, cpr->record_name);
            if(cpr->user_include_path){
                replaceAll(file_content, FREE_LIST_NAME, free_list_name_default);
            }else{
                replaceAll(file_content, FREE_LIST_NAME, cpr->free_list_name);
            }
            replaceAll(file_content, STRUCT_NAME, cpr->struct_name);
            std::stringstream ss;
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
            cold_pod_representation *cpr,
            ASTContext *ast_context)
        {
            std::string usage_text = get_text(mem_expr, ast_context);

            std::string field_decl_name = mem_expr->getMemberDecl()->getNameAsString();

            std::stringstream ss;
            ss << coop_safe_struct_acces_method_name << "->" << field_decl_name;

            usage_text.replace(
                usage_text.find(field_decl_name),
                field_decl_name.length(),
                ss.str());

            get_rewriter(ast_context)->
                ReplaceText(mem_expr->getSourceRange(), usage_text);
        }

        void handle_free_list_fragmentation(
            cold_pod_representation *cpr)
        {
            const CXXDestructorDecl *dtor_decl = cpr->rec_info->destructor_ptr;

            Rewriter *rewriter = get_rewriter(dtor_decl->getASTContext());

            //define the statement, that calls the free method of the freelist with the pointer to the cold_struct
            std::stringstream free_stmt;
            free_stmt << "//marks the cold_data_freelist's cold_struct instance as reusable\n"
               << "coop_cold_data_ptr->~"
               << cpr->struct_name << "();\n"
               << cpr->free_list_instance_name_cold
               << ".free(coop_cold_data_ptr);\n";

            if(dtor_decl){
                const Stmt *dtor_body = cpr->rec_info->destructor_ptr->getBody();
                if(dtor_body && dtor_decl->isUserProvided()){
                    //insert the relevant code to the existing constructor
                    rewriter->InsertTextBefore(dtor_body->getLocEnd(), "\n");
                    rewriter->InsertTextBefore(dtor_body->getLocEnd(), free_stmt.str());
                }else{
                    //has no destructor body
                    const RecordDecl* record_decl = cpr->rec_info->record;
                    std::stringstream dtor_string;
                    dtor_string << "\n~" << record_decl->getNameAsString() << "()\n{\n" << free_stmt.str() << "}";

                    if(!record_decl->field_empty()){
                        rewriter->InsertTextBefore(record_decl->getLocEnd(), dtor_string.str());
                    }else{
                        //severe BUG this should/could never happen -> if a record has no fields we should have never found it to be hot/cold splittable
                        //if this occures shit is hitting fans, lots of em...
                        coop::logger::out("severe BUG considered fieldless record for a hot/cold split... this should NEVER happen...");
                    }
                }
            }
        }

        void handle_new_instantiation(
            cold_pod_representation *cpr,
            std::pair<const CXXNewExpr*, ASTContext*> &expr_ctxt)
        {
            std::string new_replacement = get_text(expr_ctxt.first, expr_ctxt.second);
            std::stringstream ss;
            ss << "new(" << cpr->free_list_instance_name_hot << ".get())";
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
            std::ifstream ifs("src_mod_templates/free_list_definition.cpp");
            std::string file_content(
                (std::istreambuf_iterator<char>(ifs)),
                (std::istreambuf_iterator<char>()));

            if(cpr->user_include_path){
                replaceAll(file_content, FREE_LIST_NAME, free_list_name_default);
            }else{
                replaceAll(file_content, FREE_LIST_NAME, cpr->free_list_name);
            }
            replaceAll(file_content, RECORD_NAME, cpr->record_name);
            replaceAll(file_content, STRUCT_NAME, cpr->struct_name);
            replaceAll(file_content, FREE_LIST_INSTANCE_HOT, cpr->free_list_instance_name_hot);
            replaceAll(file_content, FREE_LIST_INSTANCE_COLD, cpr->free_list_instance_name_cold);

            std::stringstream ss;
            ss << allocation_size_hot_data;
            replaceAll(file_content, SIZE_HOT, ss.str().c_str());
            ss.str("");
            ss << allocation_size_cold_data;
            replaceAll(file_content, SIZE_COLD, ss.str().c_str());

            get_rewriter(main_function_node->getASTContext())->
                InsertTextBefore(main_function_node->getLocStart(), file_content);
        }
    }
}