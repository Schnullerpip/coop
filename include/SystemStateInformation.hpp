#ifndef COOP_SYSTEMSTATEINFORMATION_HPP
#define COOP_SYSTEMSTATEINFORMATION_HPP

#include <stdio.h>
#include <string.h>
#include <sstream>


int execution_state = 0;


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
    }
}

namespace {
    FILE * open_file(std::string path, std::string file){
        std::stringstream ss;
        FILE * sys_file_handle = 0;

        ss << path << file;
        return sys_file_handle = fopen(ss.str().c_str(), "r");
    }
}

coop::system::cache_credentials get_d_cache_info(int idx, unsigned cpu_idx = 0){
    coop::system::cache_credentials cc;
    FILE *sys_file_handle = 0;

    std::stringstream ss;
    ss << "/sys/devices/system/cpu/cpu" << cpu_idx << "/cache/index" << idx;
    std::string periph_name = ss.str();

    sys_file_handle = open_file(periph_name, "/type");
    if(!sys_file_handle){
        printf("ERROR_1\n");
        return {};
    }
    const char type[5] = {};
    fscanf(sys_file_handle, "%s", &type);
    fclose(sys_file_handle);

    if(strcmp("Data", type) == 0){
        sys_file_handle = open_file(periph_name, "/level");
        if(!sys_file_handle){
            printf("ERROR_2\n");
            return {};
        }
        fscanf(sys_file_handle, "%d", &cc.lvl);
        fclose(sys_file_handle);

        sys_file_handle = open_file(periph_name, "/coherency_line_size");
        if(!sys_file_handle){
            printf("ERROR_3\n");
            return {};
        }
        fscanf(sys_file_handle, "%d", &cc.line_size);
        fclose(sys_file_handle);

        sys_file_handle = open_file(periph_name, "/size");
        if(!sys_file_handle){
            printf("ERROR_4\n");
            return {};
        }
        fscanf(sys_file_handle, "%d", &cc.size);
        fclose(sys_file_handle);

        return cc;
    }
    printf("ERROR_5\n");
    return {};
}

#endif
