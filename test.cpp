class A {
public:
	static int s_a;
	int m_a;
	char m_b = 'a';
	short m_c[2];
};

class B : A {};

union bleh {
	int a;
	char n;
};

class C {
	int m_d;
	void foo(int& a){
		int bleo = 12 + a;
		a += 1 + bleo - m_d;
	}

	void bar(int& a){
		int bleo = 12 + a;
	}
};

void foobar(A& a){
	a.m_a += (a.m_a += 1);
	a.m_b += (a.m_b += 1);
}

void barfoo(){
	int bleo = 2 + 4;
}

int d;
