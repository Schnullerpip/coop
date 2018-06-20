#ifndef COOP_SOURCEMODIFICATION_HPP
#define COOP_SOURCEMODIFICATION_HPP
#include "coop_utils.hpp"

#define coop_cold_struct_s "coop_cold_fields_"
#define coop_safe_struct_acces_method_name "access_cold_data()"
#define coop_cold_data_pointer_name "coop_cold_data_ptr"
#define coop_union_name "coop_union_"
#define coop_union_instance_name "coop_u"
#define coop_union_byte_data "byte_data"
#define coop_union_cold_data "data"
#define coop_free_list_name "free_list_"
#define coop_free_list_instance_name "free_list_instance_"

namespace coop {
    namespace src_mod {

        struct cold_pod_representation{
            //name of the created struct
            std::string struct_name;

            //name of the address for the allocated space of the created struct
            std::string cold_data_container_name;

            //name of the union, that holds a strut_name[] and a char[] -> this way we can allocate memory
            //for struct_name on the stack even without struct_names fields having a standard constructor
            std::string union_name;

            //the nameof the freelist that comes with each generated struct
            std::string free_list_name, free_list_instance_name;

            coop::record::record_info *rec_info = nullptr;
        };

        void remove_decl(
            const clang::FieldDecl *fd,
            Rewriter *rewriter);

        void create_cold_struct_for(
            coop::record::record_info*,
            cold_pod_representation*,
            size_t allocation_size,
            const FunctionDecl* main_function_ptr,
            Rewriter*);

        void add_cpr_ref_to(
            coop::record::record_info*,
            coop::src_mod::cold_pod_representation*,
            size_t,
            Rewriter*);

        void redirect_memExpr_to_cold_struct(
            const MemberExpr *mem_expr,
            cold_pod_representation *cpr,
            ASTContext *ast_context,
            Rewriter &rewriter);

        void handle_free_list_fragmentation(
            cold_pod_representation *cpr,
            Rewriter *rewriter
        );
    }
}
#endif

