#include"SourceModification.h"
#include"clang/AST/DeclCXX.h"
#include"clang/Lex/Lexer.h"
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
#define FREE_LIST_NAME_HOT "FREE_LIST_NAME_HOT"
#define FREE_LIST_NAME_COLD "FREE_LIST_NAME_COLD"
#define free_list_name_default "coop_free_list"
#define FREE_LIST_INSTANCE_COLD "FREE_LIST_INSTANCE_COLD"
#define FREE_LIST_INSTANCE_HOT "FREE_LIST_INSTANCE_HOT"
#define RECORD_NAME "RECORD_NAME"
#define RECORD_TYPE "RECORD_TYPE"
#define FIELD_INITIALIZERS "FIELD_INITIALIZERS"
#define SEMANTIC "SEMANTIC"
#define HOT_ALIGNMENT "HOT_ALIGNMENT"
#define COLD_ALIGNMENT "COLD_ALIGNMENT"
#define BYTE_DATA_HOT "BYTE_DATA_HOT"
#define BYTE_DATA_COLD "BYTE_DATA_COLD"
#define BYTE_DATA "BYTE_DATA"
#define SIZE_VARIABLE "SIZE_VARIABLE"
#define CACHE_LINE_SIZE "CACHE_LINE_SIZE"
#define CONSTRUCTORS "CONSTRUCTORS"

namespace {
    template<typename T>
    std::string get_text(T* t, ASTContext *ast_context){
        SourceManager &src_man = ast_context->getSourceManager();

        clang::SourceLocation b(t->getLocStart()), _e(t->getLocEnd());
        clang::SourceLocation e(clang::Lexer::getLocForEndOfToken(_e, 0, src_man, ast_context->getLangOpts()));

        return std::string(src_man.getCharacterData(b),
            src_man.getCharacterData(e)-src_man.getCharacterData(b));
    }
    
    std::string get_text_(SourceRange &e, ASTContext &ctxt){
        return Lexer::getSourceText(CharSourceRange::getCharRange(e), ctxt.getSourceManager(), ctxt.getLangOpts());
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

            auto src_range = fd->getSourceRange();

            rewriter->RemoveText(src_range.getBegin(), rewriter->getRangeSize(src_range)+1); //the plus 1 gets rid of the semicolon
        }

        std::string create_constructor(std::string record_name, ASTContext *ctxt, std::vector<CXXCtorInitializer*> &const_field_inis)
        {
            std::stringstream signature;
            std::stringstream body;
            signature << record_name << "(";
            body << ":";
            for(size_t i = 0; i < const_field_inis.size(); ++i)
            {
                auto ini = const_field_inis[i];
                auto member = ini->getMember();
                auto expr = get_text(ini->getInit(), ctxt);

                signature << member->getType().getAsString() << " val" << i;
                body << member->getNameAsString() << "(val" << i << ")";
                if(i < (const_field_inis.size()-1))
                {
                    signature << ", ";
                    body << ", ";
                }
            }
            signature << ")";
            body << "{}";
            signature << body.str();
            return signature.str();
        }

        void inject_cold_struct(cold_pod_representation *cpr, const bool pool_hot_data)
        {
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

            //assemble all the struct's cold fields
            {
                std::vector<const FieldDecl*> in_class_initialized_fields;
                std::stringstream constructors;
                for(auto field : cpr->rec_info->cold_fields){
                    ss << get_text(field, &field->getASTContext()) << ";\n";
                }
            }

            replaceAll(tmpl_file_content, EXTERN, (cpr->is_header_file ? "extern " : ""));

            replaceAll(tmpl_file_content, STRUCT_NAME, cpr->struct_name);
            replaceAll(tmpl_file_content, STRUCT_FIELDS, ss.str());
            ss.str("");

            replaceAll(tmpl_file_content, DATA_NAME, cpr->cold_data_container_name);

            replaceAll(tmpl_file_content, RECORD_NAME, cpr->record_name);
            replaceAll(tmpl_file_content, RECORD_TYPE, cpr->rec_info->record->isStruct() ? "struct" : "class");

            if(cpr->user_include_path.empty()){
                replaceAll(tmpl_file_content, FREE_LIST_NAME_HOT, (pool_hot_data ? cpr->free_list_name : ""));
                replaceAll(tmpl_file_content, FREE_LIST_NAME_COLD, cpr->free_list_name);
            }else{
                replaceAll(tmpl_file_content, FREE_LIST_NAME_HOT, (pool_hot_data ? free_list_name_default : ""));
                replaceAll(tmpl_file_content, FREE_LIST_NAME_COLD, free_list_name_default);
            }
            replaceAll(tmpl_file_content, FREE_LIST_INSTANCE_COLD, cpr->free_list_instance_name_cold);
            replaceAll(tmpl_file_content, FREE_LIST_INSTANCE_HOT, (pool_hot_data ? cpr->free_list_instance_name_hot : ""));

            get_rewriter(cpr->rec_info->record->getASTContext())->
                InsertTextBefore(cpr->rec_info->record->getSourceRange().getBegin(), tmpl_file_content);
        }

        void create_cold_struct_for(
            coop::record::record_info *ri,
            cold_pod_representation *cpr,
            std::string user_include_path)
        {
            const CXXRecordDecl* rd = ri->record;
            rd = coop::global<CXXRecordDecl>::get_global(rd)->ptr;

            std::stringstream ss;

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
            {//adding the data pointer to the record
                std::stringstream ss;
                ss << coop::getEnvVar(COOP_TEMPLATES_PATH_NAME_S) << "/" << "pointer_to_cold_data.cpp";
                std::ifstream ifs(ss.str());
                std::string file_content(
                    (std::istreambuf_iterator<char>(ifs)),
                    (std::istreambuf_iterator<char>()));

                replaceAll(file_content, STRUCT_NAME, cpr->struct_name);
                replaceAll(file_content, COLD_DATA_PTR_NAME, cpr->cold_data_ptr_name);
                cpr->missing_mandatory_public << file_content;
            }
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
            auto member_decl = mem_expr->getMemberDecl();
            std::string field_decl_name = member_decl->getNameAsString();
            std::stringstream ss;

            //make sure that the right access method is called -> inheritance
            ss << field->getParent()->getQualifiedNameAsString() << "::";

            //check if the memberExpr's callee is supposed to handle a const instance
            //if(mem_expr->getBase()->getType().isConstQualified())
            //{
            //    ss << coop_struct_access_method_name_const;
            //}else{
            //    ss << coop_safe_struct_access_method_name;
            //}

            ss << cpr->cold_data_ptr_name;

            ss << ".ptr->" << field_decl_name;

            get_rewriter(ast_context)->
                ReplaceText(mem_expr->getMemberLoc(), ss.str());
        }

        void reorder_hot_data(
            cold_pod_representation *cpr)
        {
            coop::record::record_info *rec_info = cpr->rec_info;

            //assemble the pointer to the cold data struct
            std::stringstream ss;
            ss << coop::getEnvVar(COOP_TEMPLATES_PATH_NAME_S) << "/" << "pointer_to_cold_data.cpp";
            std::ifstream ifs(ss.str());
            std::string cold_data_ptr_text(
                (std::istreambuf_iterator<char>(ifs)),
                (std::istreambuf_iterator<char>()));

            replaceAll(cold_data_ptr_text, STRUCT_NAME, cpr->struct_name);
            replaceAll(cold_data_ptr_text, COLD_DATA_PTR_NAME, cpr->cold_data_ptr_name);

            //order the fields according to their alignment requirement
            struct f_a{std::string field_text; size_t alignment;};
            std::vector<f_a> field_alignments;

            for(auto field : rec_info->hot_fields){
                //get the declarations raw text
                std::string text =  get_text(field, &field->getASTContext());
                //get the field's alignment requirement
                size_t alignment = coop::get_alignment_of(field);

                field_alignments.push_back({text, alignment});

                //remove the field declaration from the original record
                coop::src_mod::remove_decl(field);
            }

            //make sure the pointer to the cold data struct is included
            field_alignments.push_back({cold_data_ptr_text, sizeof(void*)});

            //order the field declarations (according to allignment requirements)
            std::sort(field_alignments.begin(), field_alignments.end(), [](f_a a, f_a b){return a.alignment > b.alignment;});

            //add the declarations (in their now optimal order to the original record)
            cpr->missing_mandatory_public << "\n//Hot data Fields (descending order of alignment requirements)\n";
            for(f_a &i : field_alignments)
            {
                cpr->missing_mandatory_public << i.field_text << ";\n";
            }
        }

        void handle_operators(cold_pod_representation *cpr)
        {
            auto record_decl = coop::global<CXXRecordDecl>::get_global(cpr->rec_info->record)->ptr;

            //copy assignment
            //if we can't have a copy constructor, then don't create one
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
                    if(type.isConstQualified() || type.isLocalConstQualified())
                    {
                        continue;
                    }

                    //if this is a non const static array type - memcpy it
                    auto t = dyn_cast_or_null<ConstantArrayType>(type.getTypePtr());
                    if(t)
                    {
                        auto s = field->getASTContext().getTypeSizeInChars(t).getQuantity();

                        bool is_cold = std::find(cold_fields.begin(), cold_fields.end(), field) != cold_fields.end();
                        semantic << "memcpy(";
                        if(is_cold) semantic << cpr->cold_data_ptr_name << "->";
                        semantic << field->getNameAsString() << ", other_obj.";
                        if(is_cold) semantic << cpr->cold_data_ptr_name << "->";
                        semantic << field->getNameAsString() << ", " << s << ");\n";

                    }else{
                        bool is_cold = std::find(cold_fields.begin(), cold_fields.end(), field) != cold_fields.end();
                        if(is_cold) semantic << cpr->cold_data_ptr_name << "->";
                        semantic << field->getNameAsString() << " = other_obj.";
                        if(is_cold) semantic << cpr->cold_data_ptr_name << "->";
                        semantic << field->getNameAsString() << ";\n";
                    }
                }
                replaceAll(code, SEMANTIC, semantic.str());
                cpr->missing_mandatory_public << code;
            }
        }

        void handle_constructors(cold_pod_representation *cpr)
        {
            //create the strings that handle stuff
            auto global_rec = coop::global<CXXRecordDecl>::get_global(cpr->rec_info->record);
            if(!global_rec)
            {
                coop::logger::log_stream << "Could NOT find a global instance for " << cpr->rec_info->record->getNameAsString();
                coop::logger::err();
            }
            auto record_decl = global_rec->ptr;

            //bool found_normal_ctor = false;
            auto &ctors = coop::FindConstructor::rec_constructor_map[record_decl];
            for(auto ctor : ctors)
            {
                if(!ctor->isThisDeclarationADefinition() || ctor->isMoveConstructor() || ctor->isImplicit() || !ctor->hasBody() || !ctor->isDefined())
                {
                    continue;
                }

                //we want to make sure that a cold_field struct is generated for each hot data
                //thats why we inject code here - but we don't want to do this for each constructor!
                //Because there might be delegating consructors - resulting in redundant cold data generation
                //make sure we only insert code at the 'root' constructors
                while(ctor->isDelegatingConstructor())
                {
                    ctor = ctor->getTargetConstructor();
                }

                //multiple construtors might delegate to the same 'root' constructor
                //make sure to only ever inject code to a ctor ONCE
                static coop::unique uniques;
                if(uniques.check(coop::naming::get_decl_id<CXXConstructorDecl>(ctor)))
                {
                    continue;
                }

                //if this constructor (that we know is not a delegating constructor) has initializers, and if those initializers are 
                //meant for cold fields -> move those initializations into the ctor body
                //THIS implies having trouble with const members ->
                //TODO how to handle const members? how to guarantee, that const cold members are initialized, correctly?
                std::stringstream const_field_initializers;

                std::stringstream cold_field_initializations;

                std::vector<CXXCtorInitializer*> inis;
                std::vector<CXXCtorInitializer*> const_inis;
                std::vector<CXXCtorInitializer*> non_cold_inis;
                int num_ctor_inis = 0;
                int num_cold_field_inis = 0;
                const CXXCtorInitializer *first = nullptr;
                const CXXCtorInitializer *last = nullptr;
                for(auto ini : ctor->inits())
                {
                    if(ini->isBaseInitializer() || !ini->isWritten() || !ini->isAnyMemberInitializer())
                    {
                        continue;
                    }

                    if(ini->getSourceOrder() == 0)
                    {
                        first = ini;
                        if(!last)
                        {
                            last = first;
                        }
                    }else if(ini->getSourceOrder() >= num_ctor_inis)
                    {
                        last = ini;
                    }
                    
                    num_ctor_inis++;


                    auto member = ini->getMember();
                    //make sure to work with the global version
                    auto g_member = coop::global<FieldDecl>::get_global(member);
                    if(!g_member)
                    {
                        //there is no global version of the field - nothing to do here
                        continue;
                    }
                    
                    //check if the initializer refers to a cold filed
                    auto &cold_fields = cpr->rec_info->cold_fields;
                    auto iter = std::find(cold_fields.begin(), cold_fields.end(), g_member->ptr);
                    if(iter == cold_fields.end())
                    {
                        non_cold_inis.push_back(ini);
                        continue;
                    }

                    num_cold_field_inis++;

                    inis.push_back(ini);

                    ////depending whether or not the initialized field is const we need to treat them differently
                    //if(member->getType().isConstQualified())
                    //{
                    //    const_inis.push_back(ini);
                    //}else{
                    //    inis.push_back(ini);
                    //}
                }

                auto rewriter = get_rewriter(&ctor->getASTContext());

                if(num_ctor_inis != static_cast<int>(non_cold_inis.size()))
                {
                    //there are cold field initializations in this ctor
                    //assemble a new ctor code, that excludes those cold field initializations
                    std::stringstream new_initializers;
                    new_initializers << "";
                    for(size_t i = 0; i < non_cold_inis.size(); ++i)
                    {
                        CXXCtorInitializer *ini = non_cold_inis[i];
                        auto member = ini->getMember();
                        auto src_rng = ini->getSourceRange();
                        std::string txt = get_text_(src_rng, ctor->getASTContext());
                        txt = coop::naming::get_from_start_until(txt.c_str(), '(');
                        new_initializers << member->getNameAsString() << "(" << txt << ")";
                        if(i < (non_cold_inis.size()-1))
                        {
                            new_initializers << ", ";
                        }
                        coop::logger::out(new_initializers.str().c_str());
                    }

                    //if there are no ctor initializers left (they were all cold fields) make sure to also remove the :
                    SourceLocation from = first->getSourceLocation();
                    size_t until = last->getSourceRange().getEnd().getRawEncoding()-first->getSourceLocation().getRawEncoding()+1;
                    if(num_cold_field_inis == num_ctor_inis)
                    {
                        from = from.getLocWithOffset(-1);
                        ++until;
                    }

                    rewriter->ReplaceText(from, until, new_initializers.str());
                }

                //if(!const_inis.empty())
                //{
                //    //make sure that a respective constructor for those const cold fields is made
                //    cpr->const_ctors.push_back({&ctor->getASTContext(), const_inis});

                //    //make sure that this constructor calls the appropriate cold_struct ctor
                //    for(size_t i = 0; i < const_inis.size(); ++i)
                //    {
                //        auto ini = const_inis[i];

                //        const_field_initializers << get_text(ini->getInit(), &ctor->getASTContext());
                //        if(i < const_inis.size()-1)
                //        {
                //            const_field_initializers << ", ";
                //        }
                //    }
                //}


                //if there were cold field initializations removed - move them to the ctor body!
                if(!inis.empty())
                {
                    //append those initializations to the code that will be injected
                    for(auto ini : inis)
                    {
                        auto member = ini->getMember();
                        auto src_rng = ini->getSourceRange();
                        std::string txt = get_text_(src_rng, ctor->getASTContext());
                        txt = coop::naming::get_from_start_until(txt.c_str(), '(');
                        cold_field_initializations << "\n" << cpr->cold_data_ptr_name << ".ptr->" << member->getNameAsString() << " = " << txt << ";";

                    }
                }


                //inject the code to the existing ctor
                //get the ctors location
                std::stringstream code_injection;
                code_injection << cold_field_initializations.str();
                auto r = get_rewriter(ctor->getASTContext());
                r->InsertTextAfterToken(ctor->getBody()->getLocStart(), code_injection.str());
            }

            //if(!found_normal_ctor)
            //{
            //    //there is no ctor -> create one
            //    cpr->missing_mandatory_public << "\n" << cpr->record_name << "(){" << constructor_code << "}\n";
            //}

            //create the code for copy constructors
            /*if the copy ctor is implicit, then we must now make sure, that not the cold_data_ptr is copied (implicitly)
            but rather ALL fields cold and hot - so we emulate a deep copy and temporary copies of objects don't trigger
            the cold data instances to be destructed*/
            //auto cp_ctors = coop::FindConstructor::rec_copy_constructor_map[record_decl];
            //if(cp_ctors.empty())
            //{
            //    //there is no definition of a copy constructor - so we make one
            //    ifs = std::ifstream (get_src_mod_file_name("existing_copy_constructor_deep_copy_emulation.cpp"));
            //    std::string copy_constructor_code(
            //        (std::istreambuf_iterator<char>(ifs)),
            //        (std::istreambuf_iterator<char>()));
            //    replaceAll(copy_constructor_code, RECORD_NAME, cpr->record_name);
            //    replaceAll(copy_constructor_code, COLD_DATA_PTR_NAME, cpr->cold_data_ptr_name);
            //    replaceAll(copy_constructor_code, FREE_LIST_INSTANCE_COLD, cpr->free_list_instance_name_cold);
            //    replaceAll(copy_constructor_code, STRUCT_NAME, cpr->struct_name);
            //    cpr->missing_mandatory_public << copy_constructor_code;
            //}else if(cpr->rec_info->record->hasUserDeclaredCopyConstructor()){
            //    //there is a user defined copy constructor
            //    //make sure it gets an initialized pointer to a cold struct instance
            //    std::stringstream ss;   
            //    ss << cpr->cold_data_ptr_name << " = new (" << cpr->free_list_instance_name_cold << ".get<" << cpr->struct_name << ">()) " << cpr->struct_name << "();";
            //    auto ctor = cp_ctors[0];

            //    auto r = get_rewriter(ctor->getASTContext());
            //    r->InsertTextAfterToken(ctor->getBody()->getLocStart(), ss.str());
            //}
        }

        void handle_free_list_fragmentation(cold_pod_representation *cpr)
        {
            const CXXDestructorDecl *dtor_decl = cpr->rec_info->destructor_ptr;

            //define the statement, that calls the free method of the freelist with the pointer to the cold_struct
            std::stringstream free_stmt;
            free_stmt
               << cpr->cold_data_ptr_name << ".ptr->~"
               << cpr->struct_name << "();\n"
               << cpr->free_list_instance_name_cold
               << ".free("<< cpr->cold_data_ptr_name <<".ptr);";

            if(dtor_decl){
                const Stmt *dtor_body = cpr->rec_info->destructor_ptr->getBody();
                if(dtor_body && dtor_decl->isUserProvided()){
                    //insert the relevant code to the existing constructor
                    //rewriter->InsertTextBefore(dtor_body->getLocEnd(), "\n");
                    //rewriter->InsertTextBefore(dtor_body->getLocEnd(), free_stmt.str());
                    cpr->addition_for_existing_destructor << "\n" << free_stmt.str();
                }else{
                    //has no destructor body
                    const CXXRecordDecl* record_decl = cpr->rec_info->record;

                    //make sure to only work with the global instances
                    auto global_rec = coop::global<CXXRecordDecl>::get_global(record_decl);
                    if(!global_rec)
                    {
                        coop::logger::log_stream << "found no global node for: " << record_decl->getNameAsString();
                        coop::logger::out();
                        return;
                    }
                    record_decl = global_rec->ptr;

                    //since there is no existing definition of a destructor - we make one
                    //missing_mandatory will be added in conclusion
                    cpr->missing_mandatory_public << "\n~" << record_decl->getNameAsString() << "()\n{\n" << free_stmt.str() << "\n}\n";
                }
            }
        }

        void handle_new_instantiation(
            cold_pod_representation *cpr,
            std::pair<const CXXNewExpr*, ASTContext*> &expr_ctxt)
        {
            std::string new_replacement = get_text(expr_ctxt.first, expr_ctxt.second);
            std::stringstream ss;
            ss << "new(" << cpr-> qualifier << cpr->free_list_instance_name_hot << ".get<" << cpr->qualified_record_name<< ">())";
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
            std::stringstream &injection_above_main,
            size_t allocation_size_hot_data,
            size_t allocation_size_cold_data,
            size_t cache_line,
            size_t hot_alignment,
            size_t cold_alignment,
            const bool pool_hot_data)
        {
            std::stringstream ss;
            ss << coop::getEnvVar(COOP_TEMPLATES_PATH_NAME_S) << "/" << (pool_hot_data ? "free_list_definition.cpp" : "free_list_definition_only_cold.cpp");
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
            replaceAll(file_content, SIZE_HOT, ss.str());
            ss.str("");
            ss << allocation_size_cold_data;
            replaceAll(file_content, SIZE_COLD, ss.str());

            ss.str("");
            ss << "size_plus_ali_" << cpr->record_name;
            replaceAll(file_content, SIZE_VARIABLE, ss.str());

            ss.str("");
            ss << cache_line;
            replaceAll(file_content, CACHE_LINE_SIZE, ss.str());

            ss.str("");
            ss << hot_alignment;
            replaceAll(file_content, HOT_ALIGNMENT, ss.str().c_str());
            ss.str("");
            ss << cold_alignment;
            replaceAll(file_content, COLD_ALIGNMENT, ss.str().c_str());

            ss.str("");
            ss << "byte_data_" << cpr->record_name;
            replaceAll(file_content, BYTE_DATA, ss.str());
            //ss.str("");
            //ss << "byte_data_cold_" << cpr->record_name;
            //replaceAll(file_content, BYTE_DATA_COLD, ss.str());

            injection_above_main << file_content;
        }


        void handle_missing_mandatory(cold_pod_representation *cpr)
        {
            auto rec = cpr->rec_info->record;
            auto &ast_ctxt = rec->getASTContext();
            auto r = get_rewriter(ast_ctxt);

            //make sure the access modifiers fit in
            std::stringstream ss;
            ss << "private:\n" << cpr->missing_mandatory_private.str();
            ss << "\npublic:\n" << cpr->missing_mandatory_public.str();

            r->InsertTextBefore(rec->getLocEnd(), ss.str());

            //if a destructor exists - manually add the respective code to it
            if(auto dtor = rec->getDestructor())
            {
                auto dtor_body = dtor->getBody();
                if(dtor_body && dtor->isUserProvided())
                {
                    //insert the relevant code to the existing constructor
                    r->InsertTextBefore(dtor_body->getLocEnd(), cpr->addition_for_existing_destructor.str());
                }
            }
        }

        void handle_injection_above_main(
            std::stringstream &injection_above_main,
            const FunctionDecl *main_function_node)
        {
            get_rewriter(main_function_node->getASTContext())->
                InsertTextBefore(main_function_node->getLocStart(), injection_above_main.str());
        }
    }
}