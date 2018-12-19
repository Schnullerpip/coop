#ifndef COOP_FREE_LIST_HPP
#define COOP_FREE_LIST_HPP

#include<new>
#include<stddef.h>
#include<assert.h>
#include<cstdint>
#include<stdio.h>

template<typename R, typename P>
constexpr size_t max(){
	return sizeof(R) > sizeof(P) ? sizeof(R) : sizeof(P);
}

namespace coop_cex{

/*will see hoy many elements fit in a cache_line, calculate how much alignment offsets will be needed
	and returns a size, that holds enough space to keep elements +their alignmentoffsets
	minimized for -std=c++11 compatibility*/
template<size_t raw_size, size_t cache_line_size, size_t type_size>
constexpr size_t size_plus_alignments()
{
    return raw_size + cache_line_size + (cache_line_size % type_size) * ((raw_size/(cache_line_size/type_size))/(cache_line_size/type_size));
}

}//namespace coop_cex


template<typename T>
class coop_free_list
{
	union {
		T *_ptr;
		T **_next;
		char *_char_ptr;
		char **_char_ptr_ptr;
	};

public:
    T * free_ptr = nullptr;
	coop_free_list(char * byte_data, char *data_end, size_t alignment)
    {
		assert("data_end can't be smaller than byte_data" && data_end > byte_data);
		
		union {
			char * as_char_ptr;
			T * as_data_ptr;
		};

		//so the first element will also be aligned 
		as_char_ptr = align(byte_data, alignment, true);

		free_ptr = as_data_ptr;

		for(_ptr = free_ptr; _char_ptr < data_end; _ptr = *_next)
		{
			char * next = align(_char_ptr+sizeof(T), alignment, true);
			if(next >= data_end)
			{
				*_next = nullptr;
				break;
			}
			*_char_ptr_ptr = next;
		}
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
		//assert((union_cast<uintptr_t>(p) >= union_cast<uintptr_t>(byte_data)
		//	&& union_cast<uintptr_t>(p) < union_cast<uintptr_t>(union_cast<char*>(byte_data)+size*sizeof(T)))
		//	&& "coop free list will not act outside of its bounds!");

		T * tmp_ptr = free_ptr;
		free_ptr = p;
		_ptr = free_ptr;
		*_next = tmp_ptr;
	}

private:
	template<typename R, typename P>
	static R union_cast(P p, size_t offset = 0){
		union { P _p; R _t; uintptr_t _ptr;} tmp = { p };
		tmp._ptr += offset;
		return tmp._t;
	}
	static bool isPOT(size_t i)
	{
		//100 -1 => 011; 011 & 100 = 000; 100 && 111 == true
		return (i == 0) || (i && !(i & (i -1)));
	}
	template<typename P>
	static P * align(P *p, size_t alignment, bool top)
	{
		if(alignment == 0) { return p; }
		assert("alignment must be a POT" && isPOT(alignment));
		union { uintptr_t _as_uintptr; P * _as_ptr;};
		_as_ptr = p;
		if(top)
			_as_uintptr += (alignment - 1);
		_as_uintptr &= ~(alignment -1);
		return _as_ptr;
	}
};

#endif
