#include "Logger.hpp"


namespace coop{
    namespace logger {
        namespace {
            char const *out_prefix = "[coop]::[logger]::[";
        }

        std::ostream& out_stream = std::cout;
        std::stringstream log_stream;

        size_t depth = 0;

        void clear(std::stringstream& msg_stream){
            msg_stream.str("");
            msg_stream.clear();
        }

        /*outs message to out_stream*/
        size_t& out(const char* msg, const char* append){
            //getting the time
            time_t now = time(0);
            char* now_s = ctime(&now);
            size_t strln = strlen(now_s);
            //getting rid of the weird trailing linebreak
            now_s[strln-1]=']';

            out_stream << out_prefix << now_s << "-> ";
            for(size_t i = 0; i < depth; ++i){
                out_stream << (i == 0 ? "  " : "|  ");
            }
            if(depth > 0){
                out_stream << "|__ ";
            }

            out_stream << msg << append;
            return depth;
        }

        /*outs message to out_stream*/
        size_t& out(std::stringstream& msg_stream, const char* append){
            std::string msg = msg_stream.str();
            clear(msg_stream);
            return out(msg.c_str(), append);
        }

        size_t& out(){
            return out(log_stream);
        }

        size_t& out(const char* msg, Progress_Status status){
            return out(msg, status == RUNNING ? " [running]\n" : status == DONE ? " [done]\n" : " [TODO!!!!!!!]\n");
        }

        /*outs message to out_stream informing the user of a progress*/
        size_t& out(std::stringstream& msg_stream, Progress_Status status){
            std::string msg = msg_stream.str();
            msg_stream.str("");
            msg_stream.clear();
            return out(msg.c_str(), status);
        }

        void out(Progress_Status status){
            out(log_stream, status);
        }
    }
}