//this getter ensures, that an instance of the cold struct will be created on access
inline STRUCT_NAME * access_cold_data(){
	if(!COLD_DATA_PTR_NAME){
		COLD_DATA_PTR_NAME = new (FREE_LIST_INSTANCE_COLD.get()) STRUCT_NAME();
	}
	return COLD_DATA_PTR_NAME;
}

inline STRUCT_NAME * access_cold_data_const()const{
	return COLD_DATA_PTR_NAME;
}
