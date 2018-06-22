#include<stdio.h>
#include<stdlib.h>
#include<time.h>

struct coop_cold_fields_A {
	const char * name = "julian";
unsigned height = 170;
unsigned age = 26;

};

char A_cold_data[1024 * sizeof(coop_cold_fields_A)];

union coop_union_A {
	char * byte_data = A_cold_data;
	coop_cold_fields_A * data;
} coop_uA;

class free_list_A {
	union {
		coop_cold_fields_A *_ptr;
		coop_cold_fields_A **_next;
	};

	coop_cold_fields_A * free_ptr = coop_uA.data;
public:
	free_list_A(){
		for(unsigned long i = 0; i < 1024; ++i){
			_ptr = coop_uA.data+i;
			if(i == 1024 - 1){
				*_next = nullptr;
			}else{
				*_next = coop_uA.data+(i+1);
			}
		}
	}

	coop_cold_fields_A * get(){
		if(!free_ptr){
			return nullptr;
		}
		_ptr = free_ptr;
		coop_cold_fields_A *ret = _ptr;
		free_ptr = *_next;
		return ret;
	}

	void free(coop_cold_fields_A *p){
		if(p <= coop_uA.data && p < coop_uA.data+1024*sizeof(coop_cold_fields_A)){
			coop_cold_fields_A * tmp_ptr = free_ptr;
			free_ptr = p;
			_ptr = free_ptr;
			*_next = tmp_ptr;
		}
	}
}free_list_instance_A;



struct A {
	//pointer to the cold_data struct that holds this reference's cold data
coop_cold_fields_A * coop_cold_data_ptr = nullptr;

//this getter ensures, that an instance of the cold struct will be created on access
inline coop_cold_fields_A * access_cold_data(){
	if(!coop_cold_data_ptr){
		coop_cold_data_ptr = free_list_instance_A.get();
	}
	return coop_cold_data_ptr;
}

	
	
	float velocity = 0;
	float mass = 65;

~A()
{
//marks the freelist's cold_struct instance as reusable
free_list_instance_A.free(coop_cold_data_ptr);
}};

int main(){
	srand(time(NULL));
	A *a = new A();
	A *aa = new A[10];
	printf("%s is %d cm tall and %d years old\n", a->access_cold_data()->name, a->access_cold_data()->height, a->access_cold_data()->age);

	for(int i = 0; i < 10; ++i){
		a->velocity += rand()%4+1;
		for(int o = 0; o < 10; ++o){
			float energy = a->velocity * a->mass;
			printf("energy = %f\n", energy);
		}
	}

	return 0;
}
