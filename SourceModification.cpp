#include "SourceModification.h"
#include <fstream>


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
            //auto src_range = fd->getSourceRange();
            //rewriter->RemoveText(src_range.getBegin(), rewriter->getRangeSize(src_range)+1); //the plus 1 gets rid of the semicolon
            rewriter->ReplaceText(fd->getLocStart(), 0, "//externalized COLD -> ");
        }

        void create_cold_struct_for(coop::record::record_info *ri, cold_pod_representation *cpr, size_t size, const FunctionDecl *main_function_ptr, Rewriter *rewriter){
            const RecordDecl* rd = ri->record;
            std::stringstream ss;
            std::ifstream ifs("src_mod_templates/cold_struct_template.coop_tmpl");
            std::string tmpl_file_content(
                (std::istreambuf_iterator<char>(ifs)),
                (std::istreambuf_iterator<char>()));

            //determine the generated struct's name
            ss << coop_cold_struct_s << rd->getNameAsString();
            cpr->struct_name = ss.str();
            ss.str("");

            cpr->rec_info = ri;

            const char * record_name = cpr->rec_info->record->getNameAsString().c_str();

            //determine the name of the data array that will hold space for the instances
            ss << record_name << "_cold_data";
            cpr->cold_data_container_name = ss.str();
            ss.str("");

            //determine the union's name that will provide access to the byte array as an array of generated_struct's type
            ss << "coop_u" << cpr->rec_info->record->getNameAsString();
            cpr->union_name = ss.str();
            ss.str("");

            replaceAll(tmpl_file_content, "$STRUCT_NAME", cpr->struct_name);

            //assemble all the struct's fields
            for(auto field : ri->cold_field_idx){
                ss << get_field_declaration_text(field) << ";\n";
            }

            replaceAll(tmpl_file_content, "$STRUCT_FIELDS", ss.str());
            ss.str("");

            replaceAll(tmpl_file_content, "$DATA_NAME", cpr->cold_data_container_name);
            ss << size;
            replaceAll(tmpl_file_content, "$SIZE", ss.str());
            ss.str("");

            replaceAll(tmpl_file_content, "$UNION_NAME", coop_union_name);
            replaceAll(tmpl_file_content, "$UNION_BYTE_DATA", coop_union_byte_data);
            replaceAll(tmpl_file_content, "$UNION_COLD_DATA", coop_union_cold_data);
            replaceAll(tmpl_file_content, "$UNION_INSTANCE_NAME", cpr->union_name);

            llvm::outs() << tmpl_file_content;

            ASTContext &ast_context = rd->getASTContext();
            rewriter->setSourceMgr(ast_context.getSourceManager(), ast_context.getLangOpts());
            rewriter->InsertTextBefore(rd->getSourceRange().getBegin(), tmpl_file_content);

            //make the target program allocate memory on the stack for the new struct TODO
            //therefor we want to take the first AST node of the main function and prepend it with the allocation code

            //auto entry_point = (*main_function_ptr->getBody()->child_begin())->getLocStart();
            //ss << cpr->cold_data_container_name << " = alloca(" << size << " * sizeof(" << cpr->struct_name << "));\n";
            //rewriter->InsertTextBefore(entry_point, ss.str());
        }

        void add_cpr_ref_to(
            coop::record::record_info *ri,
            coop::src_mod::cold_pod_representation *cpr,
            size_t size,
            Rewriter *rewriter)
        {
            ASTContext &ast_context = ri->record->getASTContext();
            rewriter->setSourceMgr(ast_context.getSourceManager(), ast_context.getLangOpts());

            std::stringstream ss;
            ss << "//pointer to the cold_data struct that holds this reference's cold data\n"
                << cpr->struct_name << " *" << coop_cold_data_pointer_name << " = nullptr;\n"
                << "//this getter ensures, that an instance of the cold struct will be created on access\n"
                << "inline " << cpr->struct_name << " * "
                << coop_safe_struct_acces_method_name << "{\nif(!" << coop_cold_data_pointer_name << ")\n{\n"
                << "//TODO safely initialize an instance of this struct, considering its fields not having a standardconstructor..." <<"\n"
                << "//TODO safely dereference an index that is unique to each"
                << "}\n"
                << "return " << coop_cold_data_pointer_name << ";\n}\n";

            //this function will only be called if ri->cold_field_idx is not empty, so we can safely take its first element
            //we simply need some place to insert the reference to the cold data to... 
            rewriter->InsertTextBefore(ri->cold_field_idx[0]->getLocStart(), ss.str());
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
    }
}