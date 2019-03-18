#include"coop_utils.hpp"
#include"llvm-c/TargetMachine.h"
#include"data.hpp"


size_t coop::get_sizeof_in_bits(const FieldDecl* field){
    return field->getASTContext().getTypeSize(field->getType());
}
size_t coop::get_sizeof_in_byte(const FieldDecl* field){
    return get_sizeof_in_bits(field)/8;
}

size_t coop::get_alignment_of(const FieldDecl *field)
{
    auto type = field->getType().getTypePtr();
    if(type->isArrayType()){
        return field->getASTContext().
                getTypeSizeInChars(type->getArrayElementTypeNoTypeQual()).
                getQuantity();
    }
    return get_sizeof_in_byte(field);
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
    static float get_median(coop::weight_size *begin, size_t elements){
        if(elements==0)
            return 0;

        if(elements == 1)
            return begin->weight;
        
        if(elements % 2 == 0) //even so median is average of mid most 2 elements
        {
            return (begin[elements/2].weight + begin[elements/2-1].weight)/2;
        }else{ //odd so median is middle element
            return begin[elements/2].weight;
        }
    }
}

namespace coop {

void SGroup::finalize(std::vector<coop::weight_size> &weights)
{
    weights_and_sizes.insert(weights_and_sizes.begin(), weights.begin()+start_idx, weights.begin()+end_idx+1);
}
std::string SGroup::get_string()
{
    std::stringstream ss;
    ss << "[" << start_idx;
    if(start_idx != end_idx){ ss << " - " << end_idx; }
    ss << "]";
    return ss.str();
}
void SGroup::print(bool recursive)
{
    coop::logger::log_stream << get_string();
    if(next && recursive){
        coop::logger::log_stream << " -> ";
        next->print(recursive);
    }
    else
    {
        coop::logger::out();
    }
    
}


SGroup * find_significance_groups(coop::weight_size *elements, unsigned int offset, unsigned int number_elements){
    //coop::logger::log_stream << "x = {";
    //for(unsigned int i = offset; i < offset+number_elements; ++i)
    //{
    //    coop::logger::log_stream << elements[i].weight << ((i < (offset+number_elements-1)) ? ", " : "");
    //}
    //coop::logger::log_stream << "}";
    //coop::logger::out();


    if(number_elements < 3){
        //there are not enough field weights to determine a relative significance
        //just make the leftovers a group and return it
        return new SGroup(offset,offset+number_elements-1);
    }

    coop::weight_size *x = &elements[0];
    size_t n = number_elements;

    //get IQR Q1 and Q2
    float median = get_median(x, n);
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
        if((!found_high_spikes) && (i > 0) && (x[i].weight < spike_bound_top) && (x[i-1].weight > spike_bound_top)){
            //we have found the border that seperates the high spikes from the 'normal' data
            found_high_spikes = true;
            over_top = find_significance_groups(x, 0, i);
            mid_values_start_idx = i;
        }

        if(x[i].weight < spike_bound_bottom){
            //we found the border that separates the 'normal' data from the low spikes
            under_bottom = find_significance_groups(x, i, n-i);
            mid_values_end_idx = i-1;
            break;
        }
    }

    //now determine the significance groups for our middle segment
    //get all the deltas
    unsigned int num_deltas = mid_values_end_idx-mid_values_start_idx;
    std::vector<float> deltas(num_deltas+1);

    for(unsigned int i = mid_values_start_idx, o=1; i < mid_values_end_idx; ++i, ++o)
    {
        int idx = i+1;
        //int o = i-offset+1;
        deltas[o] = std::abs(elements[idx].weight - elements[idx-1].weight);
    }

    //get avg delta
    float average_diff = 0;
    for(unsigned int i = 1; i < deltas.size(); ++i)
    {
        average_diff += deltas[i];
    }
    average_diff/=deltas.size()-1;
    deltas[0] = average_diff+1;//so the first group will always be made

    //each new found delta that is above the average is considered the start of a new group
    SGroup *last = nullptr;
    for(unsigned int i = mid_values_start_idx, o=0; i <= mid_values_end_idx; ++i, ++o)
    {
        if(deltas[o] > average_diff)//new group found
        {
            if(!inside_bounds){
                inside_bounds = new SGroup(i, mid_values_end_idx);
                last = inside_bounds;
            }else{
                last->next = new SGroup(i, mid_values_end_idx);
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

    if(over_top){
        return over_top;
    }else{
        return inside_bounds;
    }
}

//determines the size of a set of Significance Groups regarding structure padding
//we will order fields according to their sizes in Byte to reduce structure padding
//param until -> inclusive
//param additional_field_size -> if we consider a possible hot split we need to be aware of the additional pointer's size/alignment
size_t determine_size_with_optimal_padding(SGroup *begin, SGroup *until, std::vector<size_t> additional_alignments, std::vector<const clang::FieldDecl*> additional_fields)
{
    //collect all sizes -> bring em in descending order (optimal padding) -> determine padding
    std::vector<size_t> alignments;
    size_t sum = 0;

    alignments.insert(alignments.end(), additional_alignments.begin(), additional_alignments.end());
    for(auto s : additional_alignments)
        sum += s;

    for(auto f : additional_fields)
    {
        size_t field_size = coop::get_sizeof_in_byte(f);
        size_t alignment = coop::get_alignment_of(f);
        sum += field_size;
        alignments.push_back(alignment);
    }

    SGroup *end_cond = (until ? until->next : nullptr);
    for(SGroup *p = begin; p != end_cond; p = p->next)
    {
        //each group has several weights and sizes
        for(auto &w_s : p->weights_and_sizes)
        {
            //consider that alignment, might diverge from size (array types)
            size_t alignment = w_s.alignment_requirement;
            alignments.push_back(alignment);
            sum += w_s.size_in_byte;
        }
    }

    //we have all the sizes (in  byte) of the groups -> order them
    std::sort(alignments.begin(), alignments.end(), [](size_t a, size_t b)->bool{return a > b;});

    size_t overhead = (sum % alignments[0]);
    size_t padding = (overhead > 0) ? (alignments[0] - overhead) : 0;

    return sum + padding;
}


size_t determine_size_with_padding(const clang::CXXRecordDecl *rec_decl)
{
    size_t greatest_size = 0;
    //iterate the fields to find the greatest alignment requirement (record will be self aligned on that)
    std::vector<std::pair<size_t, size_t>> size_alignment;
    for(auto f : rec_decl->fields())
    {
        size_t s = coop::get_sizeof_in_byte(f);
        size_t ali = coop::get_alignment_of(f);
        if(ali > greatest_size) greatest_size = ali;
        size_alignment.push_back({s, ali});
    }

    size_t padding_sum = 0;
    //now that we know the type sizes and the greatest type size we can determine the paddings
    size_t position = 0;
    for(size_t i = 0; i < size_alignment.size(); i++)
    {
        //check whether this self aligned field needs padding
        size_t i_s = size_alignment[i].first;
        size_t i_a = size_alignment[i].second;

        size_t overhead = position % i_a;

        if(overhead > 0)
        {
            size_t padding = i_a - overhead;
            padding_sum += padding;
            position += padding;
        }
        position += i_s;
    }

    size_t overhead = position % greatest_size;
    size_t ret_val = position + ((overhead > 0) ? (greatest_size - overhead):0);
    return ret_val;
}
}//namespace coop