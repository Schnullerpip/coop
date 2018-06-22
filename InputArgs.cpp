#include "InputArgs.h"

namespace coop{
    namespace input {
        std::vector<std::pair<const char *, std::function<void(const char*)>&>>
            input_actions_p;

        std::vector<std::pair<const char *, std::function<void(void)>&>>
            input_actions_p_less;
        
        int clang_args_entry_point = 1;

        void register_parametered_action(const char * opt, std::function<void(const char*)> act)
        {
            input_actions_p.push_back({opt, act});
        }

        void register_parameterless_action(const char * opt, std::function<void(void)> act)
        {
            input_actions_p_less.push_back({opt, act});
        }

        int resolve_actions(int argc, const char **argv)
        {
            for(int i = 0; i < argc; ++i){
                const char * arg = argv[i];
                if(strcmp(arg, "--") == 0){
                    clang_args_entry_point = i +1;
                    continue;
                }
                for(auto input_action : input_actions_p){
                    if(strcmp(arg, input_action.first) == 0 && (i+1) < argc){
                        input_action.second(argv[++i]);
                    }
                }
                for(auto input_action : input_actions_p_less){
                    if(strcmp(arg, input_action.first) == 0){
                        input_action.second();
                    }
                }
            }
            return clang_args_entry_point;
        }
    }
}