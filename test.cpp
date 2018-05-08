class A {
public:
	static int s_a;
	int m_a;
	char m_b = 'a';
	short m_c[2];
};

class C {
public:
	int m_d;
	void foo(int& a, A& aa){
		int bleo = 12 + a;
		a += 1 + bleo;
		aa.m_a = 0;
		a += m_d;
	}
	void bar(A& a);
};

void C::bar(A& a){
	a.m_a = 0;
	a.m_c[0] = 10;
	C c;
}

void foob(A& a, C& c){
	for(;;){
		c.m_d += 1;
		c.foo(c.m_d, a);
	}
	a.m_a += (a.m_a += 1);
	a.m_b += (a.m_b += 1);
	while(true){
		foob(a, c);
	}
}


int main(){
	A a;
	C c;
	foob(a, c);
	int bleh = 0;
	c.foo(bleh, a);
}