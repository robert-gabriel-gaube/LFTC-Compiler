// implementare recursiva pentru factorial
int fact(int n){
	if(n<3)return n;
	return n*fact(n-1);
	}

void main(){
	put_i(4.9);		// se afiseaza 4
	
	put_i(fact(3));	// se afiseaza 6

	// implementare nerecursiva pentru factorial
	int r;
	r=1;
	int i;
	i=2;
	while(i<5){
		r=r*i;
		i=i+1;
		}
	put_i(r);		// se afiseaza 24
	}
