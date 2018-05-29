#ifndef COOP_SYSTEMSTATEINFORMATION_HPP
#define COOP_SYSTEMSTATEINFORMATION_HPP


namespace coop {
    namespace system {
        //POD that holds cache data, that is relevant to us
        struct cache_credentials {
            //cache level
            int lvl;
            //size in K-bytes
            unsigned size;
            //cacheline size in bytes
            unsigned line_size;
        };

        enum CACHE_IDX {IDX_0, IDX_1, IDX_2, IDX_3};

        cache_credentials get_d_cache_info(CACHE_IDX idx, unsigned cpu_idx = 0);
    }
}
#endif
