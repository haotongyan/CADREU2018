#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>

/*
** This code divides verifying the 12 case into two parts.
** It is divided into 28 threads. This code will run 14 of them.
*/


#define NUM_THREADS 14
#define MAX_STEPS 5498999604

void get_next_family(char* family, int size, int max_val);
unsigned long long comb(int n, int k);
unsigned long long power(unsigned int x, unsigned int y);
int bit_prod(int pre, int post, int size);
unsigned make_vector(char* arr, int size, int offset);

typedef struct {
	int* k;
	int* fam_size;
	int* col_choices;
	int* one_offset;
	int* c1_size;
	int* c2_size;
	
	unsigned short* power_set;
	int* power_set_size;
	int* starting_config;
	int thread_num;
} thread_args;



void* thread_main(void* args) {
	
	// Unpacking struct. Defining new variables to store these values.
	thread_args* act = (thread_args*) args;
	int k = *(act->k);
	int fam_size = *(act->fam_size);
	int col_choices = *(act->col_choices);
	int one_offset = *(act->one_offset);
	int c1_size = *(act->c1_size);
	int c2_size = *(act->c2_size);
	int thread_num = act->thread_num;
	
	
	unsigned short* original_power_set = act->power_set;
	int set_size = *(act->power_set_size);
	int* starting_config = act->starting_config;
	
	unsigned short* power_set = (unsigned short*) malloc(set_size*sizeof(unsigned short));
	
	for(int i=0; i<set_size; i++) {
		power_set[i] = original_power_set[i];
	}
	
	// Note: family is really columns.
	char* cols = (char *) malloc((c1_size + c2_size) * sizeof(char));
	
	// Set up family
	for (int i=0; i<c1_size+c2_size; i++) {
		cols[i] = starting_config[i];
	}
	
	// Testing setup.
	unsigned cur_splitter;
	bool flag = false;
	
	
	unsigned long long combinations = comb(power(2,fam_size - 1), c1_size) * comb(power(2,fam_size - 1) - 1, c2_size-1);
	
	unsigned long long count = 0;
	
	unsigned long max_set = 0;
	unsigned long cur_set;
	unsigned long success_count = 0;
	
	
	int cur_prod, ind;
	
	//Timing
	clock_t before = clock();
	
	while (cols[0] != 0 && count <= MAX_STEPS) {
		while (cols[c1_size] != 0) {
			
			//Testing
			cur_set = 0;
			
			for (unsigned long j=0; j<set_size; j++) {
				flag = 0;
				for (int l = 0; l < fam_size - 1; l++) {
					cur_splitter = make_vector(cols, k, l);
					cur_prod = bit_prod(cur_splitter, power_set[j], k);
					if (-1 <= cur_prod && cur_prod <= 1) {
						flag = 1;
						cur_set++;
						break;
					}
				}
				if (flag == 0) {
					break;
				}
			}
			
			if (cur_set > max_set) {
				max_set = cur_set;
			}
			
			if (flag == 1) {
				for (int i=0; i<c1_size+c2_size; i++) {
					printf("%i ", cols[i]);
				}
				printf("Success! \n");
				success_count++;
			}
			
			
			
			get_next_family(cols + c1_size, c2_size-1, col_choices);
			
			
			if (count % 1000000 == 0) {
				printf ("Thread %i: %f %%.\n", thread_num, 100 * (float) count / (MAX_STEPS));
			}
			
			
			count++;
			
			
		}
		
		ind = 1;
		for (int i=c1_size + c2_size - 2; i>=c1_size; i--) {
			cols[i] = ind;
			ind++;
		}
		
		get_next_family(cols, c1_size, col_choices);
	}
	
	clock_t after = clock();
	
	
	printf("total time taken: %lu\n", (after-before)/ CLOCKS_PER_SEC);
	printf("success count: %lu\n", success_count);
	printf("max set: %lu\n", max_set);
	printf("count: %llu \n", count);
	
	
	//Garbage Collection
	free(starting_config);
	free(power_set);
	free(cols);
	free(args);
	pthread_exit(NULL);
}


/*
** Usage: ./a.out [k] [fam_size] [c1_size] [power_set_file] [breakpoint_file]
*/


int main(int argc, char** argv) {
	int k = atoi(argv[1]);
	int fam_size = atoi(argv[2]);
	int col_choices = power(2,fam_size-1) - 1; // Number of possibilities (in binary) for one column, excluding the top 1111110000000 and the last column of zeros.
	int one_offset = 1 << (fam_size - 1);	
	
	int c1_size = atoi(argv[3]); // Number of columns that start with a 1.
	int c2_size = k - c1_size; // Number of columns that start with a 0.
	
	/* 
	** FILE READING. For reduced power set.
	*/
	
	FILE* file = fopen(argv[4], "r");
	int subset, set_size;
	fscanf(file, "%d", &set_size);
	
	unsigned short* power_set = (unsigned short*) malloc(set_size * sizeof(unsigned short));
	
	fscanf(file, "%d", &subset);
	int fcounter = 0;
	while (!feof (file)) {
		power_set[fcounter] = subset;
		fscanf(file, "%d", &subset);
		fcounter++;
	}
	fclose(file);
	
	/* 
	** FILE READING. For breakpoint configurations.
	*/
	
	FILE* bp_file = fopen(argv[5], "r");
	int* config;
	
	
	/*
	** THREAD CREATION.
	*/
	
	pthread_t threads[NUM_THREADS];
	int thread_success;
	
	for(long i=0; i<NUM_THREADS; i++){
		printf("In main: creating thread %ld\n", i);
		
		//Initialize struct
		thread_args *args = (thread_args*) malloc(sizeof (thread_args));
		args->k = &k;
		args->fam_size = &fam_size;
		args->col_choices = &col_choices;
		args->one_offset = &one_offset;
		args->c1_size = &c1_size;
		args->c2_size = &c2_size;
		args->thread_num = i;
		
		//Scanning breakpoints
		config = (int*) malloc(12*sizeof(int));
		fscanf(bp_file, "%d %d %d %d %d %d %d %d %d %d %d %d", config, config+1, config+2, config+3, config+4, config+5, config+6, config+7, config+8, config+9, config+10, config+11);
		
		args->power_set = power_set;
		args->power_set_size = &set_size;
		args->starting_config = config;
		
	   	thread_success = pthread_create(&threads[i], NULL, thread_main, (void *) args);
	   	if (thread_success){
	   	   	printf("ERROR; return code from pthread_create() is %d\n", thread_success);
	   		exit(-1);
	  	}
	}
    
	
	
	
	// Garbage collection
	fclose(bp_file);
	free(power_set);
	
	
	pthread_exit(NULL);
}


void get_next_family(char* family, int size, int max_val) {
	int i=0;
	while (family[i] == (max_val - i) && i < size) {
		i++;
	}
	if (i != size) {
		family[i]++;
		for (int j=0; j<i; j++) {
			family[j] = family[i] + i - j;
		}
	} else {
		family[0] = 0; //If we have gone through all the families, have the 0th element be zero. This combination isn't a valid one, so we can recognize it.
	}
}

unsigned long long comb(int n, int k) {
	if (0 <= k && k <= n) {
		int min_val = (k < n-k) ? k : n-k;
		unsigned long long ntok = 1;
		unsigned long long ktok = 1;
		for (int t=1; t<=min_val; t++) {
			ntok *= n;
			ktok *= t;
			n--;
		}
		
		return ntok / ktok;
	} else {
		return 0;
	}
}

unsigned make_vector(char* arr, int size, int offset) {
	unsigned ans = 0;
	for (int i=0; i < size; i++) {
		ans += ((arr[i] >> offset) & 1) << (size-i-1);
	}
	return ans;
}

unsigned long long power(unsigned int x, unsigned int y) {
	long long ans = 1;
	while(y > 0) {
		ans *= x;
		y--;
	}
	
	return ans;
}

int bit_prod(int pre, int post, int size) {
	int ans = 0;
	for (int i=0; i<size; i++) {
		if ((post & 1) == 1) {
			ans += (pre & 1)? 1 : -1;
		}
		pre >>= 1;
		post >>= 1;
	}
	
	return ans;
}