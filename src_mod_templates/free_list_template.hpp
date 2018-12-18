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

template<typename T>
class coop_free_list
{
	union {
		T *_ptr;
		T **_next;
		char *_char_ptr;
		char **_char_ptr_ptr;
	};

    T * free_ptr = nullptr;
public:
	coop_free_list(char * byte_data, char *data_end, size_t alignment)
    {

		assert("data_end can't be smaller than byte_data" && data_end > byte_data);
		
		union {
			char * as_char_ptr;
			T * as_data_ptr;
		};

		size_t size = (data_end - byte_data);

		//so the first element will be aligned 
		as_char_ptr = align(byte_data, alignment, true);

        //how many elements fit in a line //aka after how many do we need a new alignment offset
        size_t Ts_per_chunk = alignment/sizeof(T);
		Ts_per_chunk = (Ts_per_chunk == 0 ? 1 : Ts_per_chunk);

		size_t padding_to_next = alignment - sizeof(T)*Ts_per_chunk;

		size /= sizeof(T);
		free_ptr = as_data_ptr;

		size_t Ts = 0;
		for(_ptr = free_ptr; _char_ptr < data_end; _ptr = *_next)
		{
			if(++Ts > Ts_per_chunk)
				Ts = 1;
			char * next = _char_ptr+sizeof(T)+(Ts == Ts_per_chunk ? padding_to_next : 0);
			if(next >= data_end)
			{
				printf("aha\n");
				*_next = nullptr;
				break;
			}
			*_char_ptr_ptr = next;
			printf("new free ptr: %p\n\tnext: %p (+ %lu)\n\n", _ptr, *_next, sizeof(T)+(Ts == Ts_per_chunk ? padding_to_next : 0));
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
