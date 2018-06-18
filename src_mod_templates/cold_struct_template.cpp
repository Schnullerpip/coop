struct STRUCT_NAME {
	STRUCT_FIELDS
};

char DATA_NAME[SIZE * sizeof(STRUCT_NAME)];

union UNION_NAME {
	char * UNION_BYTE_DATA = DATA_NAME;
	STRUCT_NAME * UNION_COLD_DATA;
} UNION_INSTANCE_NAME;

class FREE_LIST_NAME {
	union {
		STRUCT_NAME *_ptr;
		STRUCT_NAME **_next;
	};

	STRUCT_NAME * free_ptr = UNION_INSTANCE_NAME.UNION_COLD_DATA;
public:
	FREE_LIST_NAME(){
		for(unsigned long i = 0; i < SIZE; ++i){
			_ptr = UNION_INSTANCE_NAME.UNION_COLD_DATA+i;
			if(i == SIZE - 1){
				*_next = UNION_INSTANCE_NAME.UNION_COLD_DATA;
			}else{
				*_next = UNION_INSTANCE_NAME.UNION_COLD_DATA+(i+1);
			}
		}
	}

	STRUCT_NAME * get(){
		_ptr = free_ptr;
		STRUCT_NAME *ret = _ptr;
		free_ptr = *_next;
		return ret;
	}
}FREE_LIST_INSTANCE;



