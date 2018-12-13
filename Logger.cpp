#include "Logger.hpp"

Format::Modifier Format::def(Format::FG_DEFAULT);
Format::Modifier Format::red(Format::FG_RED);
Format::Modifier Format::green(Format::FG_GREEN);
Format::Modifier Format::blue(Format::FG_BLUE);

Format::Modifier Format::bold_on(Format::BOLD_ON);
Format::Modifier Format::bold_off(Format::BOLD_OFF);
Format::Modifier Format::underline_on(Format::UNDERLINE_ON);
Format::Modifier Format::underline_off(Format::UNDERLINE_OFF);

namespace coop{
    namespace logger {
        namespace {
            char const *out_prefix = "[coop]::[logger]::[";
        }

        std::ostream& out_stream = std::cout;
        std::stringstream log_stream;

        int depth = 0;

        void clear(std::stringstream& msg_stream){
            msg_stream.str("");
            msg_stream.clear();
        }

        /*outs message to out_stream*/
        int& out(const char* msg, const char* append){
            //getting the time
            time_t now = time(0);
            char* now_s = ctime(&now);
            size_t strln = strlen(now_s);
            //getting rid of the weird trailing linebreak
            now_s[strln-1]=']';

            out_stream << out_prefix << now_s << "-> ";
            for(int i = 0; i < (depth >= 0 ? depth : 0); ++i){
                out_stream << (i == 0 ? "  " : "|  ");
            }
            if(depth > 0){
                out_stream << "|__ ";
            }

            out_stream << msg << append;
            return depth;
        }

        /*outs message to out_stream*/
        int& out(std::stringstream& msg_stream, const char* append){
            std::string msg = msg_stream.str();
            clear(msg_stream);
            return out(msg.c_str(), append);
        }

        int& out(){
            return out(log_stream);
        }

        void err(Should_Exit se){
            std::stringstream local;
            local << "[ERROR]::" << log_stream.str();
            clear();
            out(local.str().c_str());
            if(se == Should_Exit::YES){
                exit(1);
            }
        }

        int& out(const char* msg, Progress_Status status){
            return out(msg, status == RUNNING ? " [RUNNING]\n" : status == DONE ? " [DONE]\n" : " [TODO!!!!!!!]\n");
        }

        /*outs message to out_stream informing the user of a progress*/
        int& out(std::stringstream& msg_stream, Progress_Status status){
            std::string msg = msg_stream.str();
            msg_stream.str("");
            msg_stream.clear();
            return out(msg.c_str(), status);
        }

        int& out(Progress_Status status){
            out(log_stream, status);
            return depth;
        }
    }
}
