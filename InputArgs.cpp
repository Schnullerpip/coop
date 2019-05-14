#include "InputArgs.h"
#include "Logger.hpp"
#include "coop_utils.hpp"

#include <stdlib.h>
#include<fstream>
#include<map>

constexpr const char *config_file_name = "coop_config.txt";


struct option {
    const char * option_name;
    const char * description;
};

namespace coop{
    namespace input {

        std::map<const char *, std::function<void(std::vector<std::string>)>>
            input_config_parametered;
        std::map<const char *, std::function<void()>>
            input_config_parameterless;
        
        void resolve_config()
        {
            //parse the config file - if non existent create one
            std::ifstream config_input;
            config_input.open(config_file_name);
            if(!config_input.good())
            {
                std::stringstream ss;
                ss << "cp " << COOP_TEMPLATES_PATH_NAME_S << "/" << config_file_name << " .";
                if(system(ss.str().c_str()) != 0)
                {
                    coop::logger::log_stream << "unknown problem when executing template copying";
                    coop::logger::err(coop::YES);
                }

                coop::logger::log_stream << "[Config]::Didn't find a " << config_file_name << " file - created one";
                coop::logger::out();
                coop::logger::log_stream << "[Config]::Configure the " << config_file_name << " and run coop again";
                coop::logger::out();
                exit(1);
            }

            //read config line by line
            for(std::string line; getline(config_input, line);)
            {
                //rule out comments
                const char *c_str = line.c_str();
                if(line.empty() || c_str[0] == '#')
                {
                    continue;
                }

                //wether or not it is a parametered config or not the first token is the attribute - get it
                std::stringstream ss(line);
                std::string attrib;
                getline(ss, attrib, ' ');

                std::vector<std::string> arguments;
                for(std::string arg; getline(ss, arg, ' ');)
                {
                    arguments.push_back(arg);
                }

                //check wether or not the line resembles an actual configuration
                bool found_it = false;
                for(auto &attrib_action_pair : input_config_parametered)
                {
                    if(strcmp(attrib_action_pair.first, attrib.c_str())==0)
                    {
                        //we have a match - invoke the appropriate action
                        if(arguments.empty())
                        {
                            coop::logger::log_stream << "[Config]::There is no argument/s given in the " << config_file_name << " for attribute " << attrib;
                            coop::logger::err(coop::Should_Exit::YES);
                        }
                        attrib_action_pair.second(arguments);
                        found_it = true;
                        break;
                    }
                }
                if(!found_it)
                for(auto &attrib_action_pair : input_config_parameterless)
                {
                    if(strcmp(attrib_action_pair.first, attrib.c_str())==0)
                    {
                        //we have a match - invoke the appropriate action
                        attrib_action_pair.second();
                        break;
                    }
                }
            }
        }

        void register_parametered_config(const char *attrib, std::function<void(std::vector<std::string>)> act)
        {
            input_config_parametered[attrib] = act;
        }

        void register_parameterless_config(const char * attrib, std::function<void(void)> act)
        {
            input_config_parameterless[attrib] = act;
        }

    }
}