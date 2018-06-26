struct A {
	float velocity = 0;
	float mass = 65;
	const char * name = "julian";
	unsigned height = 170;
	unsigned age = 26;
};

float multi(A &a){
	return a.velocity * a.mass;
}
