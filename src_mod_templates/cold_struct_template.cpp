
EXTERNFREE_LIST_NAME FREE_LIST_INSTANCE_COLD;
EXTERNFREE_LIST_NAME FREE_LIST_INSTANCE_HOT;

struct STRUCT_NAME {
	STRUCT_FIELDS
	STRUCT_NAME():FIELD_INITIALIZERS{}

	struct deep_cpy_ptr {
		deep_cpy_ptr(){}
		deep_cpy_ptr(const deep_cpy_ptr &other){
			*this = other;
		}
		~deep_cpy_ptr(){
			ptr->~STRUCT_NAME();
			FREE_LIST_INSTANCE_COLD.free(ptr);
		}
		deep_cpy_ptr &operator=(const deep_cpy_ptr &other){
			if(this==&other)return *this;
			*ptr = *other.ptr;
			return *this;
		}

		STRUCT_NAME *ptr = new (FREE_LIST_INSTANCE_COLD.get<STRUCT_NAME>()) STRUCT_NAME();
	};
};

