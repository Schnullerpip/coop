
//copy assignment operator
RECORD_NAME & operator=(const RECORD_NAME & other_obj)
{
    if(this != &other_obj){
        if(!COLD_DATA_PTR_NAME){
            COLD_DATA_PTR_NAME = new (FREE_LIST_INSTANCE_COLD.get<STRUCT_NAME>()) STRUCT_NAME();
        }
        SEMANTIC
    }
    return *this;
}
