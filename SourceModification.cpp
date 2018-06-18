#include "SourceModification.h"

#define coop_cold_data_pointer_name "coop_cold_data_ptr"

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

    //std::string get_stmt_text(const Stmt* stmt, ASTContext *ast_context){
    //    SourceManager &src_man = ast_context->getSourceManager();

    //    clang::SourceLocation b(stmt->getLocStart()), _e(stmt->getLocEnd());
    //    clang::SourceLocation e(clang::Lexer::getLocForEndOfToken(_e, 0, src_man, ast_context->getLangOpts()));

    //    return std::string(src_man.getCharacterData(b),
    //        src_man.getCharacterData(e)-src_man.getCharacterData(b));
    //}

    //std::string get_sourcerange_text(SourceRange *sr, ASTContext *ast_context){
    //    SourceManager &src_man = ast_context->getSourceManager();

    //    clang::SourceLocation b(sr->getBegin()), _e(sr->getEnd());
    //    clang::SourceLocation e(clang::Lexer::getLocForEndOfToken(_e, 0, src_man, ast_context->getLangOpts()));

    //    return std::string(src_man.getCharacterData(b),
    //        src_man.getCharacterData(e)-src_man.getCharacterData(b));

    //}

    std::string get_allocate_instance_space_for_struct_string(coop::src_mod::cold_pod_representation *cpr, size_t size){
        std::stringstream ss;
        ss << cpr->instance_name << " = (" << cpr->struct_name << "*) malloc(" << size << " * sizeof(" << cpr->struct_name << "));";
        return ss.str();
    }
}

namespace coop{
    namespace src_mod{

        void remove_decl(const FieldDecl *fd, Rewriter *rewriter){
            rewriter->setSourceMgr(fd->getASTContext().getSourceManager(), fd->getASTContext().getLangOpts());
            //auto src_range = fd->getSourceRange();
            //rewriter->RemoveText(src_range.getBegin(), rewriter->getRangeSize(src_range)+1); //the plus 1 gets rid of the semicolon
            rewriter->ReplaceText(fd->getLocStart(), 0, "//");
        }

        void create_cold_struct_for(coop::record::record_info *ri, cold_pod_representation *cpr, size_t size, const FunctionDecl *main_function_ptr, Rewriter *rewriter){
            const RecordDecl* rd = ri->record;
            std::stringstream ss;

            ss << coop_cold_struct_s << rd->getNameAsString();
            cpr->struct_name = ss.str();
            ss.str("");
            ss.clear();

            cpr->rec_info = ri;

            ss << cpr->rec_info->record->getNameAsString().c_str() << "_cold_data";
            cpr->instance_name = ss.str();
            ss.str("");
            ss.clear();

            ss << "coop_u" << cpr->rec_info->record->getNameAsString();
            cpr->union_name = ss.str();
            ss.str("");
            ss.clear();

            //name the new struct
            ss << "struct " << cpr->struct_name << " {";
            //assemble all the struct's fields
            for(auto field : ri->cold_field_idx){
                ss << "\n" << get_field_declaration_text(field) << ";";
            }
            ss << "\n};\n\n";

            //reserve memory on the stack for this new struct
            ss << "char " << cpr->instance_name << "[" << size << " * sizeof(" << cpr->struct_name << ")];\n\n";

            //create a union that consists of char[] and this new struct as []
            ss << "union " << coop_union_name << cpr->rec_info->record->getNameAsString() << "{\n"
                << "char * byte_data = " << cpr->instance_name << ";\n"
                << cpr->struct_name << " * " << coop_union_resolve << ";\n}"
                << cpr->union_name << ";\n\n";

            ASTContext &ast_context = rd->getASTContext();
            rewriter->setSourceMgr(ast_context.getSourceManager(), ast_context.getLangOpts());
            rewriter->InsertTextBefore(rd->getSourceRange().getBegin(), ss.str());

            ss.str("");
            ss.clear();

            //make the target program allocate memory on the stack for the new struct TODO
            //therefor we want to take the first AST node of the main function and prepend it with the allocation code

            //auto entry_point = (*main_function_ptr->getBody()->child_begin())->getLocStart();
            //ss << cpr->instance_name << " = alloca(" << size << " * sizeof(" << cpr->struct_name << "));\n";
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
                << "//TODO safely initialize an instance of this struct, considering its fields not having a standardconstructor..." <<";\n}\nreturn " << coop_cold_data_pointer_name << ";\n}\n";

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