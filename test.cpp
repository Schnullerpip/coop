
struct coop_cold_fields_A {
	char m_b = 'a';
short m_c[2];

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



class A {
public:
  static int s_a;
  int m_a;
  //pointer to the cold_data struct that holds this reference's cold data
coop_cold_fields_A * coop_cold_data_ptr = nullptr;

//this getter ensures, that an instance of the cold struct will be created on access
inline coop_cold_fields_A * access_cold_data(){
	if(!coop_cold_data_ptr){
		coop_cold_data_ptr = free_list_instance_A.get();
	}
	return coop_cold_data_ptr;
}

  

  ~A();
};

A::~A(){

//marks the freelist's cold_struct instance as reusable
free_list_instance_A.free(coop_cold_data_ptr);

}

class C {
public:
  int m_d;
  A a_obj;
  void foo(int &a, A &aa) {
    int bleo = 12 + a;
    a += 1 + bleo;
    aa.m_a = 0;
    a += m_d;
  }
  void bar(A &a);
};

void C::bar(A &a) {
  a.m_a = 0;
  a.access_cold_data()->m_c[0] = 10;
  C c;
}

void foob(A &a, C &c) {
  for (;;) {
    c.m_d += 1;
    // c.foo(c.m_d, a);
    for (;;) {
      c.m_d++;
      a.access_cold_data()->m_c[0]++;
      for (;;) {
        c.m_d++;
        c.a_obj.m_a = 1;
      }
      for (;;) {
        a.m_a++;
      }
    }
    a.access_cold_data()->m_b++;
  }
  a.m_a += (a.m_a += 1);
  a.access_cold_data()->m_b += (a.access_cold_data()->m_b += 1);
  while (true) {
    foob(a, c);
  }
}

int main() {
  A a;
  C c;
  foob(a, c);
  int bleh = 0;
  c.foo(bleh, a);
}
