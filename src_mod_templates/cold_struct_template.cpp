
RECORD_TYPE RECORD_NAME;
struct STRUCT_NAME;

EXTERNFREE_LIST_NAME FREE_LIST_INSTANCE_COLD;
EXTERNFREE_LIST_NAME FREE_LIST_INSTANCE_HOT;

struct STRUCT_NAME {
	STRUCT_FIELDS
	STRUCT_NAME():FIELD_INITIALIZERS{}

	struct deep_cpy_ptr {
		deep_cpy_ptr():ptr(new_instance()){}
		~deep_cpy_ptr(){
			ptr->~STRUCT_NAME();
			FREE_LIST_INSTANCE_COLD.free(ptr);
		}

		deep_cpy_ptr &operator=(const deep_cpy_ptr &other){
			if(this==&other)return *this;
			ptr = new_instance();
			return *this;
		}

		deep_cpy_ptr(const deep_cpy_ptr &other){
			*this = other;
		}

		STRUCT_NAME *new_instance(){
			return new (FREE_LIST_INSTANCE_COLD.get<STRUCT_NAME>()) STRUCT_NAME();
		}

		STRUCT_NAME *ptr;
	};
};

