#ifndef COOP_FREE_LIST_HPP
#define COOP_FREE_LIST_HPP

#include<new>//for placement new
#include<stddef.h>
#include<assert.h>
#include<cstdint>
#include<stdio.h>
#include<cstring>//for memcpy

/*	In order to keep other TU's namespaces clean we will exclude
	some functionality in an anonymous namespace*/

namespace {
/*will determine the padding needed for a type_size:cache_line_size ratio*/
constexpr size_t get_padding(size_t cache_line_size, size_t type_size)
{
	return ((cache_line_size>type_size) ?
		(cache_line_size - ((cache_line_size/type_size)*type_size)) :
		(((type_size/cache_line_size)+(type_size == cache_line_size ? 0 : 1))*cache_line_size - type_size));
}
/*will prevent division by zero for compiletime execution divisions*/
constexpr size_t div_or_1(size_t d0, size_t d1)
{
	return (d1 > d0 ? 1 : d0/d1);
}
//util for type punning
template<typename R, typename P>
static R union_cast(P p, size_t offset = 0){
	union { P _p; R _t; uintptr_t _ptr;} tmp = { p };
	tmp._ptr += offset;
	return tmp._t;
}
//is a value a power of 2?
bool isPOT(size_t i)
{
	//100 -1 => 011; 011 & 100 = 000; 100 && 111 == true
	return (i == 0) || (i && !(i & (i -1)));
}
//aligns a pointer to a predefined alignment
template<typename P>
P * align(P *p, size_t alignment, bool top)
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
} //namespace coop



namespace coop{
template<typename R>
constexpr size_t MINIMAL_FREELIST_T_SIZE(){
	return sizeof(R) > sizeof(R*) ? sizeof(R) : sizeof(R*);
}
/*will see hoy many elements fit in a cache_line, calculate how much alignment offsets will be needed
	and returns a size, that holds enough space to keep elements +their alignmentoffsets
	minimized for -std=c++11 compatibility*/
constexpr size_t size_plus_alignments(size_t number_elements, size_t cache_line_size, size_t type_size)
{
	return (div_or_1(cache_line_size,type_size) * ///Ts_per_chunk
			type_size + //chunk_size
			get_padding(cache_line_size, type_size)) *
			number_elements/div_or_1(cache_line_size,type_size) + //number chunks
			cache_line_size; //for initial alignments
}
}//namespace coop_cex




class coop_free_list
{
public:
	coop_free_list(char * data_start, char *data_end, size_t alignment, size_t block_size):
		//the first element needs to be aligned as well be aligned 
		//remember the aligned start so we can later set the free_ptr to it
		begin (free_ptr = align(data_start, alignment, true)),
		end(data_end)
    {
		assert((begin != nullptr)
			&& (end != nullptr)
			&& ((end - begin) >= (block_size))
			&& "invalid constructor arguments");

		//how many elements fit in a line aka after how many do we need a new alignment offset
		size_t Ts_per_chunk = div_or_1(alignment,block_size);

		//after enough Ts to exceed the alignment - how much padding
		//is necessary to again align to the alignment properly
		size_t padding_to_next = get_padding(alignment, block_size);

		//iterate the free list's range and initialize the pointers
		size_t Ts = 0;
		for(; free_ptr < end; free_ptr = *next)
		{
			//keep track of when we need a fresh align
			if(++Ts > Ts_per_chunk)
				Ts = 1;

			//determine where the next element will be placed
			*next = free_ptr+block_size+(Ts == Ts_per_chunk ? padding_to_next : 0);

			//it it is outside of our bounds stop
			if(*next >= end)
			{
				*next = nullptr;
				break;
			}
		}

		//set the uniform's free_ptr to the list's _beginning
		free_ptr = begin;
	}

	template<typename T>
	T * get()
	{
		assert(free_ptr && "coop free list instance was initialized with too little memory space - change default settings in coop_config.txt");
		T *ret = union_cast<T*>(free_ptr);
		free_ptr = *next;
		return ret;
	}

	void free(void *p)
	{
		assert((p != nullptr)
			&& (union_cast<uintptr_t>(p) >= union_cast<uintptr_t>(begin)
			&& union_cast<uintptr_t>(p) < union_cast<uintptr_t>(end))
			&& "Pointer to free is not inside the free list's bounds!");

		char *tmp_ptr = free_ptr;
		free_ptr = union_cast<char*>(p);
		*next = tmp_ptr;
	}
private:
	union {
		char *free_ptr;
		char **next;
	};
	char * begin = nullptr;
	char * end = nullptr;
};

#endif
