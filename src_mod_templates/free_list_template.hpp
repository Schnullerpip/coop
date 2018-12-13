#ifndef COOP_FREE_LIST_HPP
#define COOP_FREE_LIST_HPP

#include<new>
#include<stddef.h>
#include<assert.h>

template<typename T, typename P>
constexpr size_t max(){
	return sizeof(T) > sizeof(P) ? sizeof(T) : sizeof(P);
}

template<typename T, size_t size>
class coop_free_list
{
	union {
		T *_ptr;
		T **_next;
	};

	union char_data {
		char * as_char_ptr;
		T * as_data_ptr;
	} c_d;

	char byte_data[size * max<T, T*>()];

    T * free_ptr = nullptr;
public:
	coop_free_list()
    {
		c_d.as_char_ptr = byte_data;
		free_ptr = c_d.as_data_ptr;
		for(unsigned long i = 0; i < (size-1); ++i){
			_ptr = c_d.as_data_ptr+i;
			*_next = c_d.as_data_ptr+(i+1);
		}
		_ptr = c_d.as_data_ptr+size-1;
		*_next = nullptr;
	}

	T * get()
    {
		assert(free_ptr && "coop free list instance was initialized with too little memory space - change default settings in coop_config.txt");
		_ptr = free_ptr;
		T *ret = _ptr;
		free_ptr = *_next;
		return ret;
	}

	void free(T *p)
    {
		assert((p >= c_d.as_data_ptr && p < c_d.as_data_ptr+size*sizeof(T)) && "coop free list will not act outside of its bounds!");

		T * tmp_ptr = free_ptr;
		free_ptr = p;
		_ptr = free_ptr;
		*_next = tmp_ptr;
	}
};

#endif
