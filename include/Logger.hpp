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

        int& out(Progress_Status status);
    }
}

namespace Format {
    //FG = foregroudn
    //bg = backround
    enum Code {
        BOLD_ON     =1,
        BOLD_OFF    =21,
        UNDERLINE_ON=4,
        UNDERLINE_OFF=24,

        FG_RED      = 31,
        FG_GREEN    = 32,
        FG_BLUE     = 34,
        FG_DEFAULT  = 39,
        BG_RED      = 41,
        BG_GREEN    = 42,
        BG_BLUE     = 44,
        BG_DEFAULT  = 49
    };

    class Modifier {
        Code code;
    public:
        Modifier(Code pCode) : code(pCode) {}
        friend std::ostream&
        operator<<(std::ostream& os, const Modifier& mod) {
            return os << "\033[" << mod.code << "m";
        }
    };

    extern Format::Modifier def;
    extern Format::Modifier red;
    extern Format::Modifier green;
    extern Format::Modifier blue;
    extern Format::Modifier bold_on;
    extern Format::Modifier bold_off;
    extern Format::Modifier underline_on;
    extern Format::Modifier underline_off;
}
#endif