
//copy constructor
RECORD_NAME(const RECORD_NAME & other_obj)
{
    COLD_DATA_PTR_NAME = new (FREE_LIST_INSTANCE_COLD.get()) STRUCT_NAME();
    SEMANTIC
}