#ifndef COOP_SOURCEMODIFICATION_HPP
#define COOP_SOURCEMODIFICATION_HPP
#include "coop_utils.hpp"

#define coop_cold_struct_s "coop_cold_fields_"
#define coop_safe_struct_access_method_name "access_cold_data()"
#define coop_struct_access_method_name_const "access_cold_data_const()"
#define coop_cold_data_pointer_name "coop_cold_data_ptr"
#define coop_union_name "coop_union_"
#define coop_union_instance_name "coop_u"
#define coop_union_byte_data "byte_data"
#define coop_union_cold_data "data"
#define coop_free_list_name "free_list_"
#define coop_free_list_instance_name "free_list_instance_"

namespace coop {
    namespace src_mod {

        void apply_changes();

        struct cold_pod_representation{
            //the name of the file, the record is defined in
            std::string file_name;
            bool is_header_file = true;

            //user include path
            std::string user_include_path = "";

            //name of the created struct
            std::string struct_name;
            std::string record_name;
            std::string qualified_record_name;
            std::string qualifier;

            //name of the address for the allocated space of the created struct
            std::string cold_data_container_name;
            std::string cold_data_ptr_name;

            //name of the union, that holds a strut_name[] and a char[] -> this way we can allocate memory
            //for struct_name on the stack even without struct_names fields having a standard constructor
            std::string union_name;

            //the nameof the freelist that comes with each generated struct
            std::string free_list_name,
                        free_list_instance_name_cold,
                        free_list_instance_name_hot;

            coop::record::record_info *rec_info = nullptr;

            //this stringstream will collect all mandatory commands that will be added to a record, if the record doesn't have the respective methods (ctor/dtor...)
            std::stringstream missing_mandatory_public;
            std::stringstream missing_mandatory_private;
        };

        void include_file(
            const char *target_file,
            const char *include
        );

        void include_free_list_hpp(
            const char * file_path,
            const char *include_path_to_add
        );

        void remove_decl(
            const clang::FieldDecl *fd
        );

        void create_cold_struct_for(
            coop::record::record_info*,
            cold_pod_representation*,
            std::string user_include_path,
            size_t allocation_size_hot_data,
            size_t allocation_size_cold_data
        );
        
        void create_free_list_for(
            cold_pod_representation*
        );

        void add_cpr_ref_to(
            coop::src_mod::cold_pod_representation*
        );
        
        void add_memory_allocation_to(
            coop::src_mod::cold_pod_representation *cpr,
            size_t allocation_size_hot_data,
            size_t allocation_size_cold_data
        );

        void redirect_memExpr_to_cold_struct(
            const MemberExpr *mem_expr,
            const FieldDecl *field,
            cold_pod_representation *cpr,
            ASTContext *ast_context
        );

        void handle_constructors(
            cold_pod_representation *cpr
        );

        void handle_free_list_fragmentation(
            cold_pod_representation *cpr
        );

        void handle_new_instantiation(
            cold_pod_representation *cpr,
            std::pair<const CXXNewExpr *, ASTContext*> &expr_ctxt
        );

        void handle_delete_calls(
            cold_pod_representation *cpr,
            std::pair<const CXXDeleteExpr*, ASTContext*> &del_ctxt
        );
        void define_free_list_instances(
            cold_pod_representation *cpr,
            const FunctionDecl *main_function_node,
            size_t allocation_size_hot_data,
            size_t allocation_size_cold_data
        );

        //will detect whether the operators exist
        //if not the operator declarations will be placed in the missing_mandatory of the cpr
        void handle_operators(
            cold_pod_representation *cpr
        );

        //to prevent duplicate location overwrites,  changes that have no valid entry point are stored in cpr->missing_mandatory
        //they can all be injected into the record at once
        void handle_missing_mandatory(
            cold_pod_representation *cpr
        );
    }
}
#endif

