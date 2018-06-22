#include "SourceModification.h"
#include "clang/AST/DeclCXX.h"
#include "coop_utils.hpp"
#include <fstream>

#define STRUCT_NAME "STRUCT_NAME"
#define UNION_NAME "UNION_NAME"
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
#define FREE_LIST_INSTANCE_COLD "FREE_LIST_INSTANCE_COLD"
#define FREE_LIST_INSTANCE_HOT "FREE_LIST_INSTANCE_HOT"
#define RECORD_NAME "RECORD_NAME"

namespace {
    std::string get_field_declaration_text(const Decl *fd){
        ASTContext &ast_context = fd->getASTContext();
        SourceManager &src_man = ast_context.getSourceManager();

        clang::SourceLocation b(fd->getLocStart()), _e(fd->getLocEnd());
        clang::SourceLocation e(clang::Lexer::getLocForEndOfToken(_e, 0, src_man, ast_context.getLangOpts()));

        return std::string(src_man.getCharacterData(b),
            src_man.getCharacterData(e)-src_man.getCharacterData(b));
    }

    std::string get_memberExpr_text(const MemberExpr* mem_expr, ASTContext *ast_context){
        SourceManager &src_man = ast_context->getSourceManager();

        clang::SourceLocation b(mem_expr->getLocStart()), _e(mem_expr->getLocEnd());
        clang::SourceLocation e(clang::Lexer::getLocForEndOfToken(_e, 0, src_man, ast_context->getLangOpts()));

        return std::string(src_man.getCharacterData(b),
            src_man.getCharacterData(e)-src_man.getCharacterData(b));
    }

    std::string get_cxxNewExpr_text(const CXXNewExpr* expr, ASTContext *ast_context){
        SourceManager &src_man = ast_context->getSourceManager();

        clang::SourceLocation b(expr->getLocStart()), _e(expr->getLocEnd());
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

        void remove_decl(const FieldDecl *fd, Rewriter *rewriter){
            rewriter->setSourceMgr(fd->getASTContext().getSourceManager(), fd->getASTContext().getLangOpts());
            auto src_range = fd->getSourceRange();
            rewriter->RemoveText(src_range.getBegin(), rewriter->getRangeSize(src_range)+1); //the plus 1 gets rid of the semicolon
        }

        void create_cold_struct_for(coop::record::record_info *ri, cold_pod_representation *cpr, size_t allocation_size_hot_data, size_t allocation_size_cold_data, Rewriter *rewriter){
            const RecordDecl* rd = ri->record;
            std::stringstream ss;
            std::ifstream ifs("src_mod_templates/cold_struct_template.cpp");
            std::string tmpl_file_content(
                (std::istreambuf_iterator<char>(ifs)),
                (std::istreambuf_iterator<char>()));

            cpr->rec_info = ri;

            //determine the generated struct's name
            ss << coop_cold_struct_s << rd->getNameAsString();
            cpr->struct_name = ss.str();
            ss.str("");

            cpr->record_name = cpr->rec_info->record->getNameAsString().c_str();

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
            size_t byte_count = 0;
            for(auto field : ri->cold_fields){

                byte_count = coop::get_sizeof_in_byte(field);

                ss << get_field_declaration_text(field);
                if(*ri->cold_fields.end() != field){
                    ss << ";\n";
                }
            }
            replaceAll(tmpl_file_content, STRUCT_NAME, cpr->struct_name);
            replaceAll(tmpl_file_content, STRUCT_FIELDS, ss.str());
            ss.str("");
            //the freelist that is generated for each splitted record will only work if the relevant structs size is >= the size of
            //the respective pointer to such a struct (thats how freelists work)
            //so.. if we actually for example only extract a cold int (4B) then that ints memory space in the freelist wont fit 
            //a pointer - make sure to bloat the struct up if neccessary...

            //TODO
            //PROBLEM -> we dont know the target-code's target's pointer size... 32bit, 64bit... 
            //at this point we should probably ask the user for input... meh
            coop::logger::log_stream << cpr->record_name << "'s cold struct's size is " << byte_count<< " Byte -> TODO is this enough space to hold a pointer on target system?";
            coop::logger::out();

            replaceAll(tmpl_file_content, DATA_NAME, cpr->cold_data_container_name);

            ss << allocation_size_cold_data;
            replaceAll(tmpl_file_content, SIZE_COLD, ss.str());
            ss.str("");

            ss << allocation_size_hot_data;
            replaceAll(tmpl_file_content, SIZE_HOT, ss.str());
            ss.str("");

            replaceAll(tmpl_file_content, RECORD_NAME, cpr->record_name);
            replaceAll(tmpl_file_content, FREE_LIST_NAME, cpr->free_list_name);
            replaceAll(tmpl_file_content, FREE_LIST_INSTANCE_COLD, cpr->free_list_instance_name_cold);
            replaceAll(tmpl_file_content, FREE_LIST_INSTANCE_HOT, cpr->free_list_instance_name_hot);

            ASTContext &ast_context = rd->getASTContext();
            rewriter->setSourceMgr(ast_context.getSourceManager(), ast_context.getLangOpts());
            rewriter->InsertTextBefore(rd->getSourceRange().getBegin(), tmpl_file_content);
        }


        void create_free_list_for(
            cold_pod_representation *cpr,
            Rewriter *rewriter)
        {
            std::stringstream ss;
            std::ifstream ifs("src_mod_templates/free_list_template.cpp");
            std::string tmpl_file_content(
                (std::istreambuf_iterator<char>(ifs)),
                (std::istreambuf_iterator<char>()));

            replaceAll(tmpl_file_content, FREE_LIST_NAME, cpr->free_list_name);
            replaceAll(tmpl_file_content, FREE_LIST_INSTANCE_COLD, cpr->free_list_instance_name_cold);

            ASTContext &ast_context = cpr->rec_info->record->getASTContext();
            rewriter->setSourceMgr(ast_context.getSourceManager(), ast_context.getLangOpts());
            rewriter->InsertTextBefore(cpr->rec_info->record->getSourceRange().getBegin(), tmpl_file_content);
        }

        void add_cpr_ref_to(
            coop::src_mod::cold_pod_representation *cpr,
            Rewriter *rewriter)
        {
            ASTContext &ast_context = cpr->rec_info->record->getASTContext();
            rewriter->setSourceMgr(ast_context.getSourceManager(), ast_context.getLangOpts());

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
            rewriter->InsertTextBefore(cpr->rec_info->cold_fields[0]->getLocStart(), file_content);
        }

        void add_memory_allocation_to(
            coop::src_mod::cold_pod_representation *cpr,
            size_t allocation_size_cold_data,
            size_t allocation_size_hot_data,
            Rewriter *rewriter)
        {
            std::ifstream ifs("src_mod_templates/memory_allocation_template.cpp");
            std::string file_content(
                (std::istreambuf_iterator<char>(ifs)),
                (std::istreambuf_iterator<char>()));

            replaceAll(file_content, DATA_NAME, cpr->cold_data_container_name);
            replaceAll(file_content, RECORD_NAME, cpr->record_name);
            replaceAll(file_content, FREE_LIST_NAME, cpr->free_list_name);
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

            rewriter->InsertTextAfterToken(cpr->rec_info->record->getSourceRange().getEnd(), file_content);
        }

        void redirect_memExpr_to_cold_struct(const MemberExpr *mem_expr, cold_pod_representation *cpr, ASTContext *ast_context, Rewriter &rewriter)
        {
            std::string usage_text = get_memberExpr_text(mem_expr, ast_context);

            std::string field_decl_name = mem_expr->getMemberDecl()->getNameAsString();

            std::stringstream ss;
            ss << coop_safe_struct_acces_method_name << "->" << field_decl_name;

            usage_text.replace(
                usage_text.find(field_decl_name),
                field_decl_name.length(),
                ss.str());
            
            rewriter.ReplaceText(mem_expr->getSourceRange(), usage_text);
        }

        void handle_free_list_fragmentation(cold_pod_representation *cpr, Rewriter *rewriter){
            const CXXDestructorDecl *dtor_decl = cpr->rec_info->destructor_ptr;

            //define the statement, that calls the free method of the freelist with the pointer to the cold_struct
            std::stringstream free_stmt;
            free_stmt << "//marks the cold_data_freelist's cold_struct instance as reusable\n"
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

        void handle_new_instantiation(cold_pod_representation *cpr, std::pair<const CXXNewExpr*, ASTContext*> &expr_ctxt, Rewriter *rewriter)
        {
            std::string new_replacement = get_cxxNewExpr_text(expr_ctxt.first, expr_ctxt.second);
            coop::logger::log_stream << "FOUND heap instantiation -> " << new_replacement;
            coop::logger::out();
            std::stringstream ss;
            ss << "new(" << cpr->free_list_instance_name_hot << ".get())";
            replaceAll(new_replacement, "new", ss.str());
            rewriter->ReplaceText(expr_ctxt.first->getSourceRange(), new_replacement);
        }
    }
}