#include<stdio.h>
#include<stdlib.h>
#include<time.h>

struct A {
	float velocity = 0;
	float mass = 65;
	const char * name = "julian";
	unsigned height = 170;
	unsigned age = 26;
};


int main(){
	srand(time(NULL));
	A *a = new A();
	A *aa = new A[10];
	printf("%s is %d cm tall and %d years old\n", a->name, a->height, a->age);

	for(int i = 0; i < 10; ++i){
		a->velocity += rand()%4+1;
		for(int o = 0; o < 10; ++o){
			float energy = a->velocity * a->mass;
			printf("energy = %f\n", energy);
		}
	}

	return 0;
}
