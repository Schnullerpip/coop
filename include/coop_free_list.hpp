#ifndef COOP_FREE_LIST_IMPL
#define COOP_FREE_LIST_IMPL
template<typename A, int size>
class coop_free_list
{
	union char_dat {
		char * as_char_ptr;
		A *as_data_ptr;
	}c_d;

	A *free_ptr;

public:
	coop_free_list(char *byte_data){
		union {
			A *_ptr;
			A **_next;
		};

		c_d.as_char_ptr = byte_data;
		free_ptr = c_d.as_data_ptr;

		for(int i = 0; i < size; ++i){
			_ptr = c_d.as_data_ptr+i;
			if(i == size -1){
				*_next = nullptr;
			}
			else{
				*_next = c_d.as_data_ptr+(i+1);
			}
		}
	}
	
	A * get(){
		union {
			A *_ptr;
			A **_next;
		};

		if(!free_ptr){
			return nullptr;
		}
		_ptr = free_ptr;
		A *ret = _ptr;
		free_ptr = *_next;
		return ret;
	}

	void free(A *p){
		union {
			A *_ptr;
			A **_next;
		};

		if(p >= c_d.as_data_ptr && p < c_d.as_data_ptr + size * sizeof(A)){
			A *tmp_p = free_ptr;
			free_ptr = p;
			_ptr = p;
			*_next = tmp_p;
		}
	}

};
#endif
