
//copy assignment operator
RECORD_NAME & operator=(const RECORD_NAME & other_obj)
{
    if(this == &other_obj) return *this;
    COLD_DATA_PTR_NAME = new (FREE_LIST_INSTANCE_COLD.get()) STRUCT_NAME();
    SEMANTIC
}