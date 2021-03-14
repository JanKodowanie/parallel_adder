#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h> /*deklaracje standardowych funkcji uniksowych (fork(), write() itd.)*/
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <sys/time.h>

// Zmodyfikowany program z zad1, sluzacy do pomiaru czasu dla N procesow

int done = 0;

void on_usr1(int signal) {
	pid_t pid = getpid();
	done = 1;
}

int main(int argc, char *argv[])
{
	pid_t pid;
	struct timeval tval_before, tval_after, tval_diff;
	int n_proc = 3;
	char buffer[17];
	int n = 0;
	
	if(argc > 2 && strcmp(argv[1], "--nproc") == 0) {
		int N = atoi(argv[2]);
		if(N > 0) 
			n_proc = N;
    	}

	pid_t *children = malloc(n_proc * sizeof(pid_t)); 

	FILE *file = fopen("vector2.dat", "r");

	if(file == NULL) {
		fprintf(stderr, "Nie znaleziono pliku vector.dat\n");
		return EXIT_FAILURE;

	}

        while(fgets(buffer, 17, file) != NULL) {
		n++;
	}

	if(n < 2) {
		fprintf(stderr, "Za malo liczb w pliku\n");
		return EXIT_FAILURE;

	}

	rewind(file);
	
	int *data = malloc(n*sizeof(int));
	n = 0;

	while(fgets(buffer, 17, file) != NULL) {
		data[n++] = atoi(buffer);
	}

	key_t key_a, key_b, key_c;
	int id_a, id_b, id_c;
	int *range, *result, *init_data;

	if((key_a = ftok("zad1.c", 'A')) == -1 || 
			(key_b = ftok("zad1.c", 'B')) == -1 ||
    	                (key_c = ftok("zad1.c", 'C')) == -1)  {	
		perror("ftok");
		return EXIT_FAILURE;
	}

	if((id_a = shmget(key_a, 2*n_proc*sizeof(int), 0666 | IPC_CREAT)) == -1 || 
			(id_b = shmget(key_b, n_proc*sizeof(double), 0666 | IPC_CREAT)) == -1 || 
			(id_c = shmget(key_c, n*sizeof(double), 0666 | IPC_CREAT)) == -1) {
		perror("shmget");
		return EXIT_FAILURE;
	}

	range = shmat(id_a, (void *) 0, 0);
	result = shmat(id_b, (void *) 0, 0);
	init_data = shmat(id_c, (void *) 0, 0);
	
	if(range == (int *) -1 || 
			result == (int *) -1 || 
			init_data == (int *) -1) {
		perror("shmat");
		return EXIT_FAILURE;
	}	
	
	int index;
	gettimeofday(&tval_before, NULL);
	for(int i=0; i<n_proc; i++) {
		switch(pid = fork()){
			case -1:
				fprintf(stderr, "Blad w fork\n");
				return EXIT_FAILURE;
			case 0: /*proces potomny*/
				index = i;
				printf("PID: %d, index: %d.\n", getpid(), index);
				sigset_t mask; /* Maska sygnałów */
				struct sigaction usr1;
				sigemptyset(&mask); /* Wyczyść maskę */
				usr1.sa_handler = (&on_usr1);
				usr1.sa_mask = mask;
				usr1.sa_flags = SA_SIGINFO;
				sigaction(SIGUSR1, &usr1, NULL);
				sigprocmask(SIG_BLOCK, &mask, NULL);
	
				while(done == 0) {
					pause();						
				}
				if(range[2*index] != -1) {
				        result[index] += init_data[range[2*index]];		
					for(int i=range[2*index]+1; i<=range[2*index+1]; i++) {
						result[index] += init_data[i];
					}
				}
				shmdt(init_data);
				shmdt(range);
				shmdt(result);
	
				return EXIT_SUCCESS;   
				
			default:
				children[i] = pid;
				break;
			}
	}

	sleep(6);

	memcpy(init_data, data, n*sizeof(int));
	int nums_per_proc = n / n_proc;
	int diff = n % n_proc;
	int first = 0;
	int last = nums_per_proc + diff - 1;

	for(int i=0; i<2*n_proc - 1; i=i+2) {
		if(last < n) {
			range[i] = first;
			range[i+1] = last;
			first = last + 1;
			last = first + nums_per_proc - 1;
		} else {
			range[i] = -1;
			range[i+1] = -1;
		}
	}

	for(int i=0; i<n_proc; i++) {
		result[i] = 0;
	}

	for(int i=0; i<n_proc; i++) {
		kill(children[i], SIGUSR1);
	}
	
	for(int i=0; i<n_proc; i++) {
		if(wait(0) == -1) {
			fprintf(stderr, "Blad w wait\n");
			return EXIT_FAILURE;
		}
	}

	int sum = 0;
	for(int i=0; i<n_proc; i++) {
		sum += result[i];
		printf("%d ", result[i]);
	}
	gettimeofday(&tval_after, NULL);
	timersub(&tval_after, &tval_before, &tval_diff);

	printf("\n");
	printf("Suma: %d\n", sum);
	printf("Czas obliczeń: %ld.%06ld s\n", (long int) tval_diff.tv_sec - 6, (long int) tval_diff.tv_usec);

	shmdt(init_data);
	shmdt(range);
	shmdt(result);
	shmctl(id_a, IPC_RMID, 0);
	shmctl(id_b, IPC_RMID, 0);
	shmctl(id_c, IPC_RMID, 0);
	return EXIT_SUCCESS;
}
