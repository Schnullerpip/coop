#ifndef COOP_INPUT_ARGS_HPP
#define COOP_INPUT_ARGS_HPP

#include<vector>
#include<functional>
#include<string.h>

namespace coop{
    namespace input {
        void resolve_config();
        void register_parametered_config(const char *, std::function<void(std::vector<std::string>)>);
        void register_parameterless_config(const char *, std::function<void(void)>);
    }
}

#endif