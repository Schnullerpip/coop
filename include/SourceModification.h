#ifndef COOP_SOURCEMODIFICATION_HPP
#define COOP_SOURCEMODIFICATION_HPP
#include "coop_utils.hpp"

#define coop_cold_struct_s "coop_cold_fields_"
#define coop_safe_struct_acces_method_name "access_cold_data()"
#define coop_union_name "coop_union_"
#define coop_union_resolve "cold_data"

namespace coop {
    namespace src_mod {

        struct cold_pod_representation{
            std::string struct_name; //name of the created struct
            std::string instance_name; //name of the address for the allocated space of the created struct
            //name of the union, that holds a strut_name[] and a char[] -> this way we can allocate memory
            //for struct_name on the stack even without struct_names fields having a standard constructor
            std::string union_name;
            coop::record::record_info *record_i = nullptr;
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
    }
}
#endif

