#include "InputArgs.h"
#include "Logger.hpp"
#include <stdlib.h>
#include<iostream>

struct option {
    const char * option_name;
    const char * description;
};

namespace coop{
    namespace input {
        std::vector<std::pair<option, std::function<void(const char*)>>>
            input_actions_p;

        std::vector<std::pair<option, std::function<void(void)>>>
            input_actions_p_less =

            {{{"-h", "will show this help"}, [](){
                std::cout << "\n";
                for(auto opt : input_actions_p_less){
                    std::cout << opt.first.option_name << " -> " << opt.first.description << "\n";
                }
                for(auto opt : input_actions_p){
                    std::cout << opt.first.option_name << " -> " << opt.first.description << "\n";
                }
                exit(0);
            }}};
        

        void register_parametered_action(const char * opt, const char *description, std::function<void(const char*)> act)
        {
            input_actions_p.push_back({{opt, description}, act});
        }

        void register_parameterless_action(const char * opt, const char *description, std::function<void(void)> act)
        {
            input_actions_p_less.push_back({{opt, description}, act});
        }

        int resolve_actions(int argc, const char **argv)
        {
            int clang_args_entry_point = 1;
            for(int i = 0; i < argc; ++i){
                const char * arg = argv[i];

                if(strcmp(arg, "--") == 0){
                    clang_args_entry_point = i + 1;
                    continue;
                }

                for(auto input_action : input_actions_p){
                    if((strcmp(arg, input_action.first.option_name) == 0) && ((i+1) < argc)){
                        input_action.second(argv[++i]);
                    }
                }
                for(auto input_action : input_actions_p_less){
                    if(strcmp(arg, input_action.first.option_name) == 0){
                        input_action.second();
                    }
                }
            }
            return clang_args_entry_point;
        }
    }
}