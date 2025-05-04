#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <time.h>
#include <pthread.h>
#include <math.h>


//#define print_flow 1
//#define print_diagnostic 1
//#define print_run_time 1

#define OK 1
#define ERROR 0

#define MAX_BRUTE_FORCE_GRID_SIZE 100
#define MAX_BRUTE_FORCE_N 3
#define MAX_ITERATIONS 150
#define INITIAL_TEMP 1000.0
#define COOLING_RATE 0.995
#define MAX_THREADS 128
#define MAX_X 10000
#define MAX_GRID_SIZE 1000

#define BUILD_ALL m_max_n == m_num_plots
#define BRUTE_FORCE m_num_plots <= MAX_BRUTE_FORCE_GRID_SIZE && m_max_n <= MAX_BRUTE_FORCE_N


int m_num_plots = 0;
int m_max_x = 0, m_max_y = 0, m_max_n = 0;
int m_use_heuristics = 1;	//we can't brute force a solution and we're not fully building out the site
int m_global_happiness = 0;

typedef struct {
    int desirability;
    int x;
    int y;
    int built;
    int happiness;
} plot_t;
plot_t *m_plots = NULL;	//list of all plots, locations, status and desirability
plot_t *m_optimal_solution = NULL;	//contains the optimal solution between iterations of the brute force algorithm
int m_next_empty_slot = 0;

typedef struct {
    int x;
    int y;
} build_t;
build_t *m_builds = NULL;	//current list of built sites used in the SA algorithm

int m_desirability_lookup[MAX_GRID_SIZE][MAX_GRID_SIZE] = {0};	//table of desirabilities for fast lookups in the SA algorithm
int m_occupied[MAX_GRID_SIZE][MAX_GRID_SIZE] = {0}; 	//tracks the occupied sites to ensure that the random SA does not build on a spot that is already occupied


typedef struct {
    int start;
	int end;
    build_t *builds;
    int *happiness;
} thread_data;
pthread_mutex_t sa_lock, builds_table_lock, plots_table_lock;


/*
Discussion:
	Process:
		The table data is loaded and validated
		If we're building on every site:
			each site is selected and the results are output
			the run time complexity is O(max_n)
		If the grid of the plots is less than MAX_BRUTE_FORCE_GRID_SIZE:
			we brute force the selection since it's small enough to be feasible
			the results are output
			the run time complexity is O(max_n * max_n)
		Next, we find a sub-optimal solution the simple way:
			the data is sorted by the desirability (l) value
			the top max_n are selected
			the run time complexity is O(log max_n)
		Next, we apply the multi-threaded simulated annealing solution:
			breaks the solution set into up to 128 chunks
			each chunk is processed on it's own thread to calculate the global happiness value
			see the function itself for the specific rules set
			this is a probabilistic solution
*/


/**
 * @brief merge:
 *        This is the comparison function to sorts the respective chunks and merges them nack into plots
 * @param [in] left
 *        Left side of the chunk to  be sorted
 * @param [in] right
 *        Right side of the chunk to the sorted
 */
void merge(int left, int mid, int right) {
	int l = mid - left + 1;
	int r = right - mid;

	//create the left and right arrays
	plot_t* left_array = (plot_t*)malloc(l * sizeof(plot_t));
	plot_t* right_array = (plot_t*)malloc(r * sizeof(plot_t));

	//copy data to the temporary left and right arrays
	memcpy(&left_array[0], &m_plots[left], l*sizeof(plot_t));
	memcpy(&right_array[0], &m_plots[mid+1], r*sizeof(plot_t));

    //merge the temporary arrays back into plots
    int i = 0, j = 0, k = left;
	while (i < l && j < r) {
		if (left_array[i].desirability >= right_array[j].desirability) {
			m_plots[k] = left_array[i];
			i++;
		} else {
			m_plots[k] = right_array[j];
			++j;
		}

		++k;
	}

    //copy the elements of the left array
	while (i < l) {
		m_plots[k] = left_array[i];
		++i;
		++k;
	}

	//copy the elements of right array
	while (j < r) {
		m_plots[k] = right_array[j];
		++j;
		++k;
	}

	//cleanup
	free(left_array);
	free(right_array);
}


/**
 * @brief merge_sort_plots:
 *        This merge sort sorts the plots table using l as the key from high-to-low 
 *        Merge sort calls itself recursively, splitting the data into halves each time until this is nothing left to split
 *        It then begins to merge from the bottom up
 *        Th run time complexity is O(n log n)
 *        The space time complexity is O(n) extra space
 * @param [in] left
 *        Left side of the chunk to  be divided
 * @param [in] right
 *        Right side of the chunk to the divided
 */
void merge_sort_plots(int left, int right) {
	if (left < right) {
		int mid = left + (right - left) / 2;

		// sort the first and second halves
		merge_sort_plots(left, mid);
		merge_sort_plots(mid + 1, right);

		// merge the sorted halves
		merge(left, mid, right);
	}
}


/**
 * @brief display_builds_table:
 *        Prints the builds table for debugging
 */
void display_builds_table() {
    printf("\nBuilds table:\n");
    for (int i = 0; i < m_max_n; ++i)
        printf("@ %d x=%d, y=%d\n", i, m_builds[i].x, m_builds[i].y);
    printf("\n");
}


/**
 * @brief display_lookup_table:
 *        Prints the desirabilities lookup table for debugging
 */
void display_lookup_table() {
    printf("\nLookup table:\n");
	for (int j = 0; j < m_max_y; ++j) {
		for (int i = 0; i < m_max_x; ++i)
			printf("%d ", m_desirability_lookup[i][j]);
		printf("\n");
	}
    printf("\n");
}


/**
 * @brief display_occupied_table:
 *        Prints the occupied table for debugging
 */
void display_occupied_table() {
    printf("\nOccupied table:\n");
	for (int j = 0; j < m_max_y; ++j) {
		for (int i = 0; i < m_max_x; ++i)
			printf("%d ", m_occupied[i][j]);
		printf("\n");
	}
    printf("\n");
}


/**
 * @brief display_table:
 *        Prints the plots table for debugging
 */
void display_plots_table() {
    printf("\nPlots table:\n");
    for (int i = 0; i < m_num_plots; ++i)
        printf("@ %d l=%d, x=%d, y=%d, built=%d, happiness=%d\n", i, m_plots[i].desirability, m_plots[i].x, m_plots[i].y, m_plots[i].built, m_plots[i].happiness);
    printf("\n");
}


/**
 * @brief print_results:
 *        Prints the output table to stdout
 *        Each line of the table has the form: x y
 */
void print_results() {
    #ifdef print_flow
        printf("Required Output:\n");
    #endif

 	if (m_use_heuristics) {
	   for (int i = 0; i < m_max_n; ++i) {
			printf("%d %d\n", m_builds[i].x, m_builds[i].y);
		}
	} else {
		for (int i = 0; i < m_num_plots; ++i)
			if (m_optimal_solution[i].built) {
				printf("%d %d\n", m_optimal_solution[i].x, m_optimal_solution[i].y);
			}
	}
}


/**
 * @brief save_optimal_solution:
 *        Makes a copy of the current solution 
 */
void save_optimal_solution() {
    memcpy(&m_optimal_solution[0], &m_plots[0], m_num_plots*sizeof(plot_t));
}


/**
 * @brief manhattan_distance:
 *        Calcuates the distance function which could be different in the future
 * @param [in] x1
 *        The x location of the first build site
 * @param [in] y1
 *        The y location of the first build site
 * @param [in] x2
 *        The x location of the second build site
 * @param [in] y2
 *        The y location of the second build site
 * @param [out] distance
 *        Returns the distance between the two built sites
 */
int manhattan_distance(int x1, int y1, int x2, int y2) {
    return abs(x1 - x2) + abs(y1 - y2);
}


/**
 * @brief find_happiness:
 *        Calculates all the current happiness values for selected plots using the plot table for the brute force method
 * @param [out] happiness
 *        Returns the global happiness value
 */
int find_happiness() {
    int happiness = 0;

    for (int i = 0; i < m_num_plots; ++i) {
        if (!m_plots[i].built) continue;

		int min_distance = m_max_n * 2;
        for (int j = 0; j < m_num_plots; ++j) {
            if (i == j || !m_plots[j].built) continue;

            int distance = manhattan_distance(m_plots[i].x, m_plots[i].y, m_plots[j].x, m_plots[j].y);
			if (distance < min_distance) min_distance = distance;
        }

		happiness += min_distance*m_plots[i].desirability;
    }

    return happiness;
}


/**
 * @brief find_range_happiness:
 *        Calculates all the current happiness values for selected plots using the builds table for the SA method
 * @param [in] builds
 *        The builds set used in the happiness calculation
 * @param [in] num_builds
 *        The number of builds included in the calculated
 * @param [in] start
 *        Beginning of the range
 * @param [in] end
 *        End of the range
 * @param [out] happiness
 *        Returns the global happiness value
 */
int find_range_happiness(build_t builds[], int num_builds, int start, int end) {
	int happiness = 0;

	for (int i = start; i < end; ++i) {
		int min_distance = m_max_n * 2;
		int x = builds[i].x;
		int y = builds[i].y;

		for (int j = 0; j < num_builds; ++j) {
			if (i == j) continue;

			int dist = manhattan_distance(x, y, builds[j].x, builds[j].y);
			if (dist < min_distance)
				min_distance = dist;
		}

		happiness += m_desirability_lookup[x-1][y-1] * min_distance;
	}

	return happiness;
}


/**
 * @brief generate_neighbor_subset:
 *        Generates the changes made to the existing built set which will be used to check for a better happiness score
 * @param [in] builds
 *        The builds set to be modified
 * @param [in] start
 *        Beginning of the range
 * @param [in] end
 *        End of the range
 */
void generate_neighbor_subset(build_t builds[], int start, int end) {
    for (int i = start; i < end; ++i) {
		//this could be implemented with one or more strategies for selecting changes
		//the three used here are:
		//		move a build in one of four directions (10%)
		//		pick a random open location on the grid (80%)
		//		pick the next highest desirability open location off the plots list (this becomes deterministic)
		int move_type = rand() % 100;
		int x = builds[i].x;
		int y = builds[i].y;

		if (move_type < 10) { 	//local move one step removed from the current location
			int direction = rand() % 4;
			switch (direction) {
				case 1: y = (builds[i].y - 1 + m_max_y) % m_max_y; break;	//north
				case 2: x = (builds[i].x + 1) % m_max_x; break;	//east
				case 3: y = (builds[i].y + 1) % m_max_y; break;	//south
				case 4: x = (builds[i].x - 1 + m_max_x) % m_max_x; break;	//west
			}
		} else if (move_type < 90) { //move to a random location inside the full grid
			x = (rand() % m_max_x) + 1;
			y = (rand() % m_max_y) + 1;
		} else { //select the next highest desirability open location
			//get the next open (unbuilt) slot from the m_plots table
			//Note: this is going to trash the plots list which is OK since we won't be using it for any other reason
			if (m_next_empty_slot < m_num_plots) {
				pthread_mutex_lock(&plots_table_lock);
				m_plots[m_next_empty_slot].built = 1;
				x = m_plots[m_next_empty_slot].x;
				y = m_plots[m_next_empty_slot].y;

				++m_next_empty_slot;
				pthread_mutex_unlock(&plots_table_lock);
			}
		}

		//range check the location
		if (x < 1) x = 1;
		if (y < 1) y = 1;
		if (x > m_max_x) x = m_max_x;
		if (y > m_max_y) y = m_max_y;
		
		//use the new position if it's not already built on
        pthread_mutex_lock(&builds_table_lock);
		if (!m_occupied[x-1][y-1]) {
			builds[i].x = x;
			builds[i].y = y;
			m_occupied[builds[i].x-1][builds[i].y-1] = 0;	//clear the current built site
			m_occupied[x-1][y-1] = 1;	//build here
		}
        pthread_mutex_unlock(&builds_table_lock);
	}
}


/**
 * @brief sa_worker:
 *        The worker thread for the simulated annealing algorithm
 * @param [in] arg
 *        Data being passed to the thread
 */
void *sa_worker(void *arg) {
    thread_data *data = (thread_data *)arg;

    generate_neighbor_subset(data->builds, data->start, data->end);

    int range_happiness = find_range_happiness(data->builds, m_max_n, data->start, data->end);

    pthread_mutex_lock(&sa_lock);
    *(data->happiness) += range_happiness;
    pthread_mutex_unlock(&sa_lock);

    return NULL;
}


/**
 * @brief simulated_annealing:
 *        Sets up the annealing threads to carry out the algorithm
 */
void simulated_annealing() {
    double temperature = INITIAL_TEMP;
	int thread_count = m_max_n / (MAX_X / MAX_THREADS);
	thread_count = (thread_count < 1 ? 1 : thread_count);
	int chunk_size = m_max_n / thread_count;

	//locks for thread synchronization
    pthread_mutex_init(&sa_lock, NULL);
	pthread_mutex_init(&builds_table_lock, NULL);
	pthread_mutex_init(&plots_table_lock, NULL);


	//thread structures
    pthread_t threads[thread_count];
    thread_data sa_data[thread_count];


	#ifdef print_flow
		printf("Spinning up %d thread(s) for %d iterations\n", thread_count, MAX_ITERATIONS);
	#endif


    for (int iter = 0; iter < MAX_ITERATIONS; ++iter) {
        build_t temp_builds[m_max_n];
        memcpy(&temp_builds[0], &m_builds[0], m_max_n*sizeof(build_t));
 
        int happiness = 0;

		//spin up all threads
        for (int t = 0; t < thread_count; ++t) {
            sa_data[t].start = t * chunk_size;
            sa_data[t].end = (t ==  thread_count - 1) ? m_max_n : (t + 1) * chunk_size;
            sa_data[t].builds = temp_builds;
            sa_data[t].happiness = &happiness;

            pthread_create(&threads[t], NULL, sa_worker, &sa_data[t]);
        }

        // block for all threads to finish
        for (int t = 0; t <  thread_count; ++t) {
            pthread_join(threads[t], NULL);
        }

        // acceptance probability and update the current state for the next iteration
        if (happiness > m_global_happiness || exp((happiness - m_global_happiness) / temperature) > ((double)rand() / RAND_MAX)) {
			memcpy(&m_builds[0], &temp_builds[0], m_max_n*sizeof(build_t));
            m_global_happiness = happiness;
        }

        // cooling schedule
        temperature *= COOLING_RATE;
    }
}


/**
 * @brief greedy_optimization:
 *        Since the table is sorted high to low, a niave optiomization would be to take the top n plots
 * @param [out] the global happiness
 *        The maximum happiness of the greedy optimization
 */
int greedy_optimization() {
    int index = 0;

	merge_sort_plots(0, m_num_plots-1);
    
    while (index < m_max_n) {
        m_plots[index].built = 1;
		m_occupied[m_plots[index].x-1][m_plots[index].y-1] = 1;


		//populate the builds table if we're using the large implementation method
		//this gives us a starting point for the SA heuristic and means we don't have to use the plots table
		//which is  larger
		if (m_use_heuristics) {
			build_t p = {m_plots[index].x, m_plots[index].y};
			memcpy(&m_builds[index], &p, sizeof(build_t));
			#ifdef print_diagnostic
				printf("%d %d %d\n", index, m_builds[index].x, m_builds[index].y);
				printf("%d %d %d\n", index, m_plots[index].x, m_plots[index].y);
			#endif
		}

        ++index;
    }
    save_optimal_solution();
	m_next_empty_slot = index;

    #ifdef print_diagnostic
		if (m_use_heuristics) {
			display_builds_table();
			display_occupied_table();
		}
	#endif

	int happy = 0;
	if (m_use_heuristics)
		happy = find_range_happiness(m_builds, m_max_n, 0, m_max_n);
	else 
		happy = find_happiness();
 
	return happy;
}


/**
 * @brief brute_force:
 *        Since the table is small enough, use the brute force way to find the optimal happiness
 * @param [in] start
 *        The starting depth of the current call
 * @param [in] depth
 *        The depth we can go down to
 * @param [in] max_happiness
 *        Integer pointer of the maximum happiness found
 */
void brute_force(int start, int depth, int *max_happiness) {
    if (depth == m_max_n) {
		//we've selected m_max_n builds
		int happiness = find_happiness(m_plots);

        //If the happiness is better than our highest, reset the highest and preserve the solution
        if (happiness > *max_happiness) {
			#ifdef print_diagnostic
				printf("New max = %d\n", happiness);
				display_plots_table();
			#endif

            *max_happiness = happiness;
            save_optimal_solution();
        }

        return;
    }
   
    for (int i = start; i < m_num_plots; ++i) {
        if (!m_plots[i].built) {
            m_plots[i].built = 1; //build here
            brute_force(i + 1, depth + 1, max_happiness);
            m_plots[i].built = 0; //backtrack
        }
    }
}


/**
 * @brief full_build:
 *        We're building everything so select all sites
 */
void full_build() {
    for (int i = 0; i < m_max_n; ++i)
        m_plots[i].built = 1;

    save_optimal_solution();
} 


/**
 * @brief find_solution:
 *        Use a stepped approach to find the optimal solution
 */
void find_solution() {
    int max_happiness = 0;

    //full build out
    if (BUILD_ALL) {
        #ifdef print_flow
            printf("Full Build\n");
        #endif

        full_build();
        return;
    }


    //brute force if the grid is small enough
    if (BRUTE_FORCE) {
        #ifdef print_flow
            printf("Brute Force\n");
        #endif

        brute_force(0, 0, &max_happiness);
        return;
    }


    //start with being greedy
    #ifdef print_flow
        printf("Greedy Optimization\n");
    #endif
    max_happiness = greedy_optimization();


    //try something more interesting
	#ifdef print_flow
		printf("Simulated Annealing Optimization\n");
	#endif
	simulated_annealing(max_happiness);
}

/**
 * @brief is_pos_integer:
 *        Generates RSA key pair in PEM format
 * @param [in] str
 *        The string to be tested
 * @return OK - Positive integer
 * @return ERROR - Not a positive integer
 */
int is_pos_integer(char* str) {
    int len = strlen(str);


    #ifdef print_diagnostic
        printf("Evaluating string: %s\n", str);
    #endif

    if (len == 0) return ERROR;
    for (int i = 0; i < len; ++i)
        if (!isdigit(str[i])) return ERROR;

    return OK;
}


/**
 * @brief insert_plot:
 *        Inserts each new plot into the plot array (higest to lowest) using the desirability value as the key
 *        Run time complexity is O(log n) * n elements making the file load O(n * log n)
 *        Space time complexity is O(n)
 * @param [in] l
 *        The l value for the plot
 * @param [in] x
 *        The x coordinate for the location
 * @param [in] y
 *        The y coordinate for the location
 */
void insert_plot(int l, int x, int y) {
	static int plot_num = 0;

    plot_t p = {l, x, y, 0, 0};
	memcpy(&m_plots[plot_num], &p, sizeof(plot_t));
	#ifdef print_diagnostic
		printf("plots: @ %d, x=%d, y=%d, l=%d\n", plot_num, m_plots[plot_num].x, m_plots[plot_num].y, m_plots[plot_num].desirability);
	#endif
	
	//populate the lookup table if we're using the large implementation method
	if (m_use_heuristics) {
		m_desirability_lookup[x-1][y-1] = l;
		#ifdef print_diagnostic
			printf("lookup: @ %d, x=%d, y=%d, l=%d\n", plot_num, x, y, m_desirability_lookup[x-1][y-1]);
		#endif
	}

	++plot_num;
}


/**
 * @brief read_params:
 *        Reads and parses out the parameters line which should produce the max_x, y and n values
 * @return OK - Parameters have been read and verified
 * @return ERROR - Parameters are invalid
 */
int read_params() {
    int ints[4] = {0, 0, 0, 0};
    int count = 0;
    char buffer[4096];


    #ifdef print_flow
        printf("Reading parameters line\n");
    #endif

    fgets(buffer, sizeof(buffer), stdin);
    #ifdef print_diagnostic
        printf("%s\n", buffer);
    #endif

    char *token = strtok(buffer, " \t\n");  //split by whitespace
    while (token != NULL && count < 4) {
        if (!is_pos_integer(token)) {
            printf("Corrupt parameter data - aborting...\n");

            return ERROR;
        }

        ints[count] = atoi(token);  //convert token to integer
        token = strtok(NULL, " \t\n");
        ++count;
    }

    if (count != 3) {
        printf("Wrong number of parameters - aborting...\n");

        return ERROR;
    }

    m_max_x = ints[0];
    m_max_y = ints[1];
    m_max_n = ints[2];
	m_num_plots = m_max_x * m_max_y;

    #ifdef print_diagnostic
        printf("x = %d, y = %d, n = %d, plots = %d\n", m_max_x, m_max_y, m_max_n, m_num_plots);
    #endif

    if (m_max_x < 1 || m_max_x > 1000) {
        printf("x size is invalid: %d - aborting...\n", m_max_x);
        return ERROR;
    }

    if (m_max_y < 1 || m_max_y > 1000) {
        printf("y size is invalid: %d - aborting...\n", m_max_y);
        return ERROR;
    }

    if (m_max_n < 1 || m_max_n > 10000) {
        printf("n size is invalid: %d - aborting...\n", m_max_n);
        return ERROR;
    }

    if (m_max_n > m_num_plots) {
        printf("n size is greater than number of plots - aborting...\n");
        return ERROR;
    }


	if (BUILD_ALL)	//we're building on every plot
		m_use_heuristics = 0;
	if (BRUTE_FORCE)	//we can brute force this
		m_use_heuristics = 0;

	#ifdef print_flow
		printf("Large Implementation = %d\n", m_use_heuristics);
	#endif

	//allocate the lookup table if we're doing this the hard way
	if (m_use_heuristics) {
		#ifdef print_flow
			printf("Allocating large implementaion memory\n");
		#endif

		m_builds = malloc(m_max_n*sizeof(build_t));
		if (m_builds == NULL)  return ERROR;
	}


    return OK;
}


/**
 * @brief read_table:
 *        Reads in the table of l values for each plot from stdin
 *        Extra table lines are ignored
 * @return OK - Table data loaded correctly
 * @return ERROR - Table data is invalid
 */
int read_table() {
    char buffer[8192];


    int *ints = (int *)malloc((m_max_x+1) * sizeof(int));
	if (ints == NULL) return ERROR;

    #ifdef print_diagnostic
        printf("Reading %d lines\n", m_max_y);
    #endif

    int line = 1;
    for (int i = 0; i < m_max_y; ++i) {      
        fgets(buffer, sizeof(buffer), stdin);
        #ifdef print_diagnostic
            printf("%s\n", buffer);
        #endif
        
        char *token = strtok(buffer, " \t\n");  //split by whitespace
        int count = 0;
        while (token != NULL) {
            if (!is_pos_integer(token)) {
                printf("Corrupt table data - aborting...\n");
                free(ints);

                return ERROR;
            }

            ints[count] = atoi(token);  //convert token to integer
            if (ints[count] < 0 || ints[count] > 100) {
                printf("Invalid l value: %d - aborting...\n", ints[count]);
                free(ints);

                return ERROR;
            }
            token = strtok(NULL, " \t\n");
            ++count;
        }

        if (count != m_max_x) {
            printf("Wrong number of columns in table - aborting...\n");
            free(ints);

            return ERROR;
        }

		for (int i = 0; i < count; ++i) {
			#ifdef print_diagnostic
				printf("Inserting: %d, %d, %d\n", ints[i], i+1, line);
			#endif

			insert_plot(ints[i], i+1, line);
		}

        ++line;
    }

	#ifdef print_diagnostic
		display_lookup_table();
	#endif

    free(ints);


    return OK;
}


/**
 * @brief load_data:
 *        Reads the parameter and table data from stdin
 * @return OK - Data loaded correctly
 * @return ERROR - Parameters or table are invalid
 */
int load_data() {
    //read and sanity check the first line of the file which should be in the form of:
    //x y n
    if (!read_params())
        return ERROR;
    
    //allocate the plots array which will be sorted
    m_plots = malloc(m_num_plots*sizeof(plot_t));
	if (m_plots == NULL) return ERROR;
    m_optimal_solution = malloc(m_num_plots*sizeof(plot_t));
	if (m_optimal_solution == NULL) return ERROR;

    //read each of the next y lines of the table
    if (!read_table()) {
        if (m_plots != NULL) free(m_plots);
        if (m_optimal_solution != NULL) free(m_optimal_solution);
		if (m_builds != NULL) free(m_builds);

        return ERROR;
    }      

    return OK;
}


int main(int argc, char* argv[]) {
    #ifdef print_flow
        printf("size of int is: %ld bytes\n", sizeof(int));
        printf("size of plot_t is: %ld bytes\n", sizeof(plot_t));
        printf("size of build_t is: %ld bytes\n", sizeof(build_t));
        printf("\n");
    #endif


    #ifdef print_run_time
		struct timespec begin, end;
		double elapsed;
		clock_gettime(CLOCK_MONOTONIC, &begin);
    #endif


    if (!load_data()) {
        #ifdef print_flow
            printf("File load failed - aborting...\n");
        #endif
        
        return -1;
    }


    #ifdef print_flow
        printf("File loaded!\n\n");
        printf("Number of plots: %d\n", m_num_plots);
        printf("Number of builds: %d\n", m_max_n);
    #endif


    find_solution();
    print_results();


    #ifdef print_diagnostic
        display_plots_table();
    #endif


    if (m_plots != NULL) free(m_plots);
    if (m_optimal_solution != NULL) free(m_optimal_solution);
	if (m_builds != NULL) free(m_builds);


    #ifdef print_run_time
		clock_gettime(CLOCK_MONOTONIC, &end);
		elapsed = end.tv_sec - begin.tv_sec;
		elapsed += (end.tv_nsec - begin.tv_nsec) / 1000000000.0;
		printf("Execution time: %f seconds\n", elapsed);
    #endif


    #ifdef print_flow
        printf("\nPROGRAM RUN COMPLETE\n");
    #endif

    return 0;
}