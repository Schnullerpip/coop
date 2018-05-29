#include <stdio.h>
#include <string.h>
#include <sstream>
#include "SystemStateInformation.hpp"

namespace {
    //returns 0 on success, else failure
    template<typename T>
    int read_from_to(std::string path_to_file, std::string file, const char* format, T *dest){
        std::stringstream ss;
        FILE * sys_file_handle = 0;

        ss << path_to_file << file;
        sys_file_handle = fopen(ss.str().c_str(), "r");
        if(!sys_file_handle){
            return 1;
        }
        int chars_read = fscanf(sys_file_handle, format, dest);
        fclose(sys_file_handle);
        
        if(chars_read == 0){
            return 1;
        }
        return 0;
    }
}


coop::system::cache_credentials coop::system::get_d_cache_info(CACHE_IDX idx, unsigned cpu_idx){
    coop::system::cache_credentials cc;
    int routine_state = 0;

    std::stringstream ss;
    ss << "/sys/devices/system/cpu/cpu" << cpu_idx << "/cache/index" << idx;
    std::string periph_name = ss.str();

    const char type[5] = {};
    routine_state = read_from_to(periph_name, "/type", "%s", &type);
    if(routine_state != 0){
        printf("ERROR_1\n");
        return {};
    }

    if(strcmp("Data", type) == 0){
        routine_state = read_from_to(periph_name, "/level", "%d", &cc.lvl);
        if(routine_state != 0){
            printf("ERROR_2\n");
            return {};
        }

        routine_state = read_from_to(periph_name, "/coherency_line_size", "%d", &cc.line_size);
        if(routine_state != 0){
            printf("ERROR_3\n");
            return {};
        }

        routine_state = read_from_to(periph_name, "/size", "%d", &cc.size);
        if(routine_state != 0){
            printf("ERROR_4\n");
            return {};
        }

        return cc;
    }
    printf("ERROR_5\n");
    return {};
}