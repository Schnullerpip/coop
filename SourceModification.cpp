#include "SourceModification.h"

namespace {
    std::string get_field_declaration_text(const FieldDecl *fd){
        ASTContext &ast_context = fd->getASTContext();
        SourceManager &src_man = ast_context.getSourceManager();

        clang::SourceLocation b(fd->getLocStart()), _e(fd->getLocEnd());
        clang::SourceLocation e(clang::Lexer::getLocForEndOfToken(_e, 0, src_man, ast_context.getLangOpts()));

        return std::string(src_man.getCharacterData(b),
            src_man.getCharacterData(e)-src_man.getCharacterData(b));
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

        void create_cold_struct_for(coop::record::record_info *ri, cold_pod_representation *cpr, size_t size, Rewriter *rewriter){
            const RecordDecl* rd = ri->record;
            std::stringstream ss;

            ss << coop_cold_struct_s << rd->getNameAsString();
            cpr->name = ss.str();
            ss.str("");
            ss.clear();
            cpr->record_i = ri;

            //name the new struct
            ss << "struct " << cpr->name << " {";

            //assemble all the struct's fields
            for(auto field : ri->cold_field_idx){
                ss << "\n" << get_field_declaration_text(field) << ";";
            }

            ss << "\n} " << cpr->record_i->record->getNameAsString().c_str() << "_cold_data" << "[" << size << "]" << ";\n";

            ASTContext &ast_context = rd->getASTContext();
            rewriter->setSourceMgr(ast_context.getSourceManager(), ast_context.getLangOpts());
            rewriter->InsertTextBefore(rd->getSourceRange().getBegin(), ss.str());
        }

        void add_cpr_ref_to(coop::record::record_info *ri, const char *name, Rewriter *rewriter){
            ASTContext &ast_context = ri->record->getASTContext();
            rewriter->setSourceMgr(ast_context.getSourceManager(), ast_context.getLangOpts());

            std::stringstream ss;
            ss << "//pointer to the cold_data struct that holds this reference's cold data\n"<< name  << " cold_data_ptr;\n" ;
            //this function will only be called if ri->cold_field_idx is not empty, so we can safely take its first element
            //we simply need some place to insert the reference to the cold data to... 
            rewriter->InsertTextBefore(ri->cold_field_idx[0]->getLocStart(), ss.str());

        }
    }
}
