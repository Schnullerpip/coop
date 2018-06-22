template<typename T, typename P>
constexpr size_t max(){
	return sizeof(T) > sizeof(P) ? sizeof(T) : sizeof(P);
}

template<typename T, size_t size>
class FREE_LIST_NAME
{
	union {
		T *_ptr;
		T **_next;
	};

	union char_data {
		char * as_char_ptr;
		T * as_data_ptr;
	} c_d;

	char byte_data[size * max<T, void*>()];

    T * free_ptr = nullptr;
public:
	FREE_LIST_NAME()
    {
		c_d.as_char_ptr = byte_data;
		free_ptr = c_d.as_data_ptr;
		for(unsigned long i = 0; i < size; ++i){
			_ptr = c_d.as_data_ptr+i;
			if(i == size - 1){
				*_next = nullptr;
			}else{
				*_next = c_d.as_data_ptr+(i+1);
			}
		}
	}

	T * get()
    {
		if(!free_ptr){
			return nullptr;
		}
		_ptr = free_ptr;
		T *ret = _ptr;
		free_ptr = *_next;
		return ret;
	}

	void free(T *p)
    {
		if(p <= c_d.as_data_ptr && p < c_d.as_data_ptr+size*sizeof(T)){
			T * tmp_ptr = free_ptr;
			free_ptr = p;
			_ptr = free_ptr;
			*_next = tmp_ptr;
		}
	}
};

