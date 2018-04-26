#include <iostream>

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
};

void foobar(A& a, C& c){
	c.m_d += 1;
	a.m_a += (a.m_a += 1);
	a.m_b += (a.m_b += 1);
}


int main(){
	A a;
	std::cout << a.m_a << std::endl;
}