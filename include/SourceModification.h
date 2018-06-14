#ifndef COOP_SOURCEMODIFICATION_HPP
#define COOP_SOURCEMODIFICATION_HPP
#include "coop_utils.hpp"

#define coop_cold_struct_s "coop_cold_data_struct_"

namespace coop {
    namespace src_mod {

        struct cold_pod_representation{
            std::string name;
            coop::record::record_info *record_i = nullptr;
        };

        void remove_decl(const clang::FieldDecl *fd, Rewriter *rewriter);
        void create_cold_struct_for(coop::record::record_info*, cold_pod_representation*, size_t allocation_size, Rewriter*);
        void add_cpr_ref_to(coop::record::record_info*, const char*, Rewriter*);
    }
}
#endif

