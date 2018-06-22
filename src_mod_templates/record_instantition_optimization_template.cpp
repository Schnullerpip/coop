
RECORD_NAME * coop_RECORD_NAME_factory_get(){
	static count = 0;
	return new(RECORD_NAME_hot_data+(count++ * sizeof(RECORD_NAME))) RECORD_INSTANTIATION;
}
