struct coop_cold_data_struct_A {
  char m_b = 'a';
  short m_c[2];
} A_cold_data[1024];

class A {
public:
  static int s_a;
  int m_a;
  // pointer to the cold_data struct that holds this reference's cold data
  coop_cold_data_struct_A coop_cold_data_ptr;
  // char m_b = 'a';
  // short m_c[2];
};

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
  a.coop_cold_data_ptr.m_c[0] = 10;
  C c;
}

void foob(A &a, C &c) {
  for (;;) {
    c.m_d += 1;
    // c.foo(c.m_d, a);
    for (;;) {
      c.m_d++;
      a.coop_cold_data_ptr.m_c[0]++;
      for (;;) {
        c.m_d++;
        c.a_obj.m_a = 1;
      }
      for (;;) {
        a.m_a++;
      }
    }
    a.coop_cold_data_ptr.m_b++;
  }
  a.m_a += (a.m_a += 1);
  a.coop_cold_data_ptr.m_b += (a.coop_cold_data_ptr.m_b += 1);
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
