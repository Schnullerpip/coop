#ifndef COOP_INPUT_ARGS_HPP
#define COOP_INPUT_ARGS_HPP

#include<vector>
#include<functional>
#include<string.h>

namespace coop{
    namespace input {
        void register_parametered_action(const char *, std::function<void(const char*)>);
        void register_parameterless_action(const char *, std::function<void(void)>);
        int resolve_actions(int argc, const char **argv);
    }
}

#endif