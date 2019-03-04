#include"coop_utils.hpp"
#include"llvm-c/TargetMachine.h"
#include"data.hpp"


int coop::get_sizeof_in_bits(const FieldDecl* field){
    return field->getASTContext().getTypeSize(field->getType());
}
int coop::get_sizeof_in_byte(const FieldDecl* field){
    return get_sizeof_in_bits(field)/8;
}

std::string coop::getEnvVar( std::string const & key )
{
    char * val = getenv( key.c_str() );
    return val == NULL ? std::string("") : std::string(val);
}

void coop::record::record_info::init(
    const clang::CXXRecordDecl* class_struct,
    std::set<const clang::FieldDecl*> *field_vector,
    std::map<const clang::FunctionDecl*, std::vector<const clang::MemberExpr*>> *rlvnt_funcs,
    std::map<const Stmt*, loop_credentials> *rlvnt_loops)
    {

    record = class_struct;
    fields = *field_vector;

    fun_mem.init(fields.size(), rlvnt_funcs);
    loop_mem.init(fields.size(), rlvnt_loops);

    relevant_functions = rlvnt_funcs;
    relevant_loops = rlvnt_loops;

    //the fun_mem_mat will be written according to the indices the members are mapped to here
    //since a function can mention the same member several times, we need to make sure each
    //iteration over the same member associates with the same adress in the matrix (has the same index)
    int index_count = 0;
    field_weights.resize(fields.size());
    for(auto f : fields){
        field_idx_mapping[f] = index_count;
        field_weights[index_count++] = {f, 0};
    }
}

std::vector<const MemberExpr*>* coop::record::record_info::isRelevantFunction(const clang::FunctionDecl* func){
    auto global = coop::global<FunctionDecl>::get_global(func);
    if(!global)return nullptr;

    auto funcs_iter = relevant_functions->find(global->ptr);
    if(funcs_iter != relevant_functions->end()){
        return &funcs_iter->second;
    }
    return nullptr;
}

int coop::record::record_info::isRelevantField(const MemberExpr* memExpr){
    const FieldDecl* field = static_cast<const FieldDecl*>(memExpr->getMemberDecl());
    auto global = coop::global<FieldDecl>::get_global(field);
    if(!global)return -1;
    field = global->ptr;
    if(std::find(fields.begin(), fields.end(), field) != fields.end()){
        return field_idx_mapping[field];
    }
    return -1;
}

void coop::record::record_info::print_func_mem_mat(std::map<const FunctionDecl*, int> & mapping_reference){
    std::function<const char* (const FunctionDecl*)> getNam =
        [](const FunctionDecl* fd){ return fd->getNameAsString().c_str();};

    std::function<int (const FunctionDecl*)> getIdx =
        [&mapping_reference](const FunctionDecl* fd){
            return mapping_reference[fd];
    };
    print_mat(&fun_mem, getNam, getIdx);
}
void coop::record::record_info::print_loop_mem_mat(
                std::map<const Stmt*, coop::loop_credentials> &loop_reference,
                std::map<const Stmt*, int> &loop_idx_mapping_reference){

    std::function<const char* (const Stmt*)> getNam = [&loop_reference](const Stmt* ls){ 

        auto loop_iter = loop_reference.find(ls);
        if(loop_iter != loop_reference.end()){
            return loop_iter->second.identifier.c_str();
        }
        return "unidentified loop -> this is most likely a bug!";
    };

    std::function<int (const Stmt*)> getIdx = [&loop_idx_mapping_reference](const Stmt* ls){
        return loop_idx_mapping_reference[ls];
    };
    print_mat(&loop_mem, getNam, getIdx);
}

namespace {
    static float get_median(float *begin, size_t elements){
        if(elements==0)
            return 0;

        if(elements == 1)
            return *begin;
        
        if(elements % 2 == 0) //even so median is average of mid most 2 elements
        {
            return (begin[elements/2] + begin[elements/2-1])/2;
        }else{ //odd so median is middle element
            return begin[elements/2];
        }
    }
}

void SGroup::print()
{
    coop::logger::log_stream << "[" << start_idx << " - " << end_idx << "]";
    if(next){
        coop::logger::log_stream << " -> ";
        next->print();
    }
    else
    {
        coop::logger::out();
    }
    
}


namespace coop{
SGroup * find_significance_groups(float *elements, unsigned int offset, unsigned int number_elements){
    coop::logger::log_stream << "call:find_significance_groups(" << elements << "," <<offset<< "," << number_elements << ")";
    coop::logger::out();
    coop::logger::depth+=1;

    coop::logger::log_stream << "x = {";
    for(unsigned int i = offset; i < offset+number_elements; ++i)
    {
        coop::logger::log_stream << elements[i] << ((i < (offset+number_elements-1)) ? ", " : "");
    }
    coop::logger::log_stream << "}";
    coop::logger::out();


    if(number_elements < 3){
        //there are not enough field weights to determine a relative significance
        //just make the leftovers a group and return it
        coop::logger::depth-=1;
        return new SGroup(offset,offset+number_elements-1);
    }

    float *x = &elements[0];
    size_t n = number_elements;

    //get IQR Q1 and Q2
    float median = get_median(x, n);
    coop::logger::log_stream << "median: " << median;
    coop::logger::out();
    float q1, q2;
        q1 = get_median(x, n/2);
    if(n%2==0){
        q2 = get_median(x+(n/2), n/2);
    }else{
        q2 = get_median(x+(n/2)+1, n/2);
    }

    //interquartile range (IQR) -> tolerance range 3 IQR
    float IQR = q1 - q2;
    float spike_bound_top = median+IQR/2+IQR;
    float spike_bound_bottom = median-IQR/2-IQR;

    coop::logger::log_stream << "q1: " << q1 << "; q2: " << q2;
    coop::logger::out();
    coop::logger::log_stream << "q1+IQR: " << spike_bound_top << "; q2-IQR: " << spike_bound_bottom;
    coop::logger::out();

    //determine whether we need another recursion (if not all elements can be found inside our tolerance range)
    //for each significance range (over top bound, inside top/bottom bounds, under bottom bounds) invoke tha routine another time.
    //the elements are sorted in a descending order, so we iterate from start to beginning checking for annomalies.
    SGroup *over_top = nullptr;
    SGroup *inside_bounds = nullptr;
    SGroup *under_bottom = nullptr;

    unsigned int mid_values_start_idx = 0;
    unsigned int mid_values_end_idx = n-1;

    bool found_high_spikes = false;
    for(unsigned int i = 0; i < n; ++i)
    {
        if((!found_high_spikes) && (i > 0) && (x[i] < spike_bound_top) && (x[i-1] > spike_bound_top)){
            //we have found the border that seperates the high spikes from the 'normal' data
            found_high_spikes = true;
            coop::logger::out("found high spikes");
            over_top = find_significance_groups(x, 0, i);
            mid_values_start_idx = i;
        }

        if(x[i] < spike_bound_bottom){
            //we found the border that separates the 'normal' data from the low spikes
            coop::logger::out("found low spikes");
            under_bottom = find_significance_groups(x, i, n-i);
            mid_values_end_idx = i-1;
            break;
        }
    }

    coop::logger::log_stream << "MID VALUES: " << mid_values_start_idx << " " << mid_values_end_idx;
    coop::logger::out();
    
    //now determine the significance groups for our middle segment
    //get all the deltas
    unsigned int num_deltas = mid_values_end_idx-mid_values_start_idx;
    std::vector<float> deltas(num_deltas+1);

    for(unsigned int i = mid_values_start_idx, o=1; i < mid_values_end_idx; ++i, ++o)
    {
        int idx = i+1;
        //int o = i-offset+1;
        deltas[o] = std::abs(elements[idx] - elements[idx-1]);
    }

    coop::logger::log_stream << "d = {";
    for(unsigned int i = 0; i < deltas.size(); ++i){
        coop::logger::log_stream << deltas[i] << ((i < (deltas.size()-1)) ? ", " : "");
    }
    coop::logger::log_stream << "}";
    coop::logger::out();

    //get avg delta
    float average_diff = 0;
    for(unsigned int i = 1; i < deltas.size(); ++i)
    {
        average_diff += deltas[i];
    }
    average_diff/=deltas.size()-1;
    deltas[0] = average_diff+1;//so the first group will always be made
    coop::logger::log_stream << "delta avg: " << average_diff;
    coop::logger::out();

    //each new found delta that is above the average is considered the start of a new group
    SGroup *last = nullptr;
    for(unsigned int i = mid_values_start_idx, o=0; i <= mid_values_end_idx; ++i, ++o)
    {
        if(deltas[o] > average_diff)//new group found
        {
            if(!inside_bounds){
                coop::logger::log_stream << "new SGroup " << i << " - " << mid_values_end_idx;
                coop::logger::out();
                inside_bounds = new SGroup(i, mid_values_end_idx);
                last = inside_bounds;
            }else{
                last->next = new SGroup(i, mid_values_end_idx);
                coop::logger::log_stream << "new SGroup " << i << " - " << mid_values_end_idx;
                coop::logger::out();
                last->end_idx=i-1;
                last = last->next;
            }
        }
    }

    if(over_top){
        //there were values exceeding the upper bounds -> they have been made a group on their own. connect them to this mid segment
        //find the lowest ordered group among them
        SGroup *lowest = over_top;
        while(lowest->next)
            lowest = lowest->next;
        lowest->next = inside_bounds;
    }

    //if there were values exceeding the lower bounds -> they have been made a group on their own. connect them to this mid segment
    last->next = under_bottom;

    if(!over_top && !inside_bounds && !under_bottom){
        coop::logger::out("NO SIG GROUPS FOUND IN RECURSION");
    }

    coop::logger::depth-=1;
    if(over_top){
        return over_top;
    }else{
        return inside_bounds;
    }
}
}