#ifndef COOP_LOGGER_HPP
#define COOP_LOGGER_HPP

#include <iostream>
#include <sstream>
#include <string.h>
#include <string>

namespace coop{
    enum Should_Exit {YES, NO};
    namespace logger {
        namespace {
            extern char const *out_prefix;
        }

        //configurable streams
        extern std::ostream& out_stream;
        extern std::stringstream log_stream;
        //configurable depth (out message indentation)
        extern int depth;

        void clear(std::stringstream& msg_stream = log_stream);

        /*outs message to out_stream*/
        int& out(const char* msg, const char* append = "\n");

        /*outs message to out_stream*/
        int& out(std::stringstream& msg_stream, const char* append = "\n");

        /*outs log_stream to out_stream*/
        int& out();

        void err(Should_Exit se = Should_Exit::YES);


        /*informing the user of a progress*/
        enum Progress_Status {RUNNING, DONE, TODO};

        int& out(const char* msg, Progress_Status status);

        /*outs message to out_stream informing the user of a progress*/
        int& out(std::stringstream& msg_stream, Progress_Status status);

        void out(Progress_Status status);
    }
}
#endif