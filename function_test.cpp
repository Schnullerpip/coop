class A {
	public:
	int m_a;
};

int foo(A& a){
	return (a.m_a += 2) * a.m_a;
}
