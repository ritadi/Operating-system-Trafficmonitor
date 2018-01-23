#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "traffic.h"

extern struct intersection isection;

/**
 * Populate the car lists by parsing a file where each line has
 * the following structure:
 *
 * <id> <in_direction> <out_direction>
 *
 * Each car is added to the list that corresponds with 
 * its in_direction
 * 
 * Note: this also updates 'inc' on each of the lanes
 */
void parse_schedule(char *file_name) {
	int id;
	struct car *cur_car;
	struct lane *cur_lane;
	enum direction in_dir, out_dir;
	FILE *f = fopen(file_name, "r");

	/* parse file */
	while (fscanf(f, "%d %d %d", &id, (int*) &in_dir, (int*) &out_dir) == 3) {

		/* construct car */
		cur_car = malloc(sizeof(struct car));
		cur_car->id = id;
		cur_car->in_dir = in_dir;
		cur_car->out_dir = out_dir;

		/* append new car to head of corresponding list */
		cur_lane = &isection.lanes[in_dir];
		cur_car->next = cur_lane->in_cars;
		cur_lane->in_cars = cur_car;
		cur_lane->inc++;
	}

	fclose(f);
}

/**
 * TODO: Fill in this function
 *
 * Do all of the work required to prepare the intersection
 * before any cars start coming
 * 
 */
void init_intersection() {
	//have to init:
	//	pthread_mutex_t lock in lane
	//	pthread_cond_t  producer_cv, consumer_cv in lane
	//	pthread_mutex_t quad[4] in intersection
	//	elements in lane

	int i;
	for (i = 0; i < 4; i++) {
		pthread_mutex_init(&isection.quad[i], NULL);
	}

	for (i = 0; i < 4; i++) {
		pthread_mutex_init(&isection.lanes[i].lock, NULL);
		pthread_cond_init(&isection.lanes[i].producer_cv, NULL);
		pthread_cond_init(&isection.lanes[i].consumer_cv, NULL);
		isection.lanes[i].inc = 0;
		isection.lanes[i].passed = 0;
		isection.lanes[i].in_buf = 0;
		isection.lanes[i].capacity = LANE_LENGTH; // max 10 cars
		isection.lanes[i].head = 0;
		isection.lanes[i].tail = 0;
		isection.lanes[i].buffer = malloc(sizeof(struct car*) * LANE_LENGTH); // 10 car circular buffer

	}

}

/**
 * TODO: Fill in this function
 *
 * Populates the corresponding lane with cars as room becomes
 * available. Ensure to notify the cross thread as new cars are
 * added to the lane.
 * 
 */
void *car_arrive(void *arg) {
	//producer

	struct car *current; // cur_car
	struct lane *l = (struct lane*) arg;
	current = l->in_cars;
	while (l->inc != 0) {

		pthread_mutex_lock(&l->lock);

		// do stuff
		while (l->in_buf == l->capacity) {/*while buffer is full, wait for consumer*/

			pthread_cond_wait(&l->consumer_cv, &l->lock); // wait for space

		}

		l->buffer[l->tail] = current;
		l->tail++;
		l->tail = l->tail % LANE_LENGTH; //circular buffer; if tail goes over LANE_LENGTH
		l->in_buf++;
		l->inc--;

		current = current->next;

		pthread_cond_signal(&l->producer_cv);

		pthread_mutex_unlock(&l->lock);

	}
	return NULL;
}

/**
 * TODO: Fill in this function
 *
 * Moves cars from a single lane across the intersection. Cars
 * crossing the intersection must abide the rules of the road
 * and cross along the correct path. Ensure to notify the
 * arrival thread as room becomes available in the lane.
 *
 * Note: After crossing the intersection the car should be added
 * to the out_cars list of the lane that corresponds to the car's
 * out_dir. Do not free the cars!
 *
 * 
 * Note: For testing purposes, each car which gets to cross the 
 * intersection should print the following three numbers on a 
 * new line, separated by spaces:
 *  - the car's 'in' direction, 'out' direction, and id.
 * 
 * You may add other print statements, but in the end, please 
 * make sure to clear any prints other than the one specified above, 
 * before submitting your final code. 
 */
void *car_cross(void *arg) {
	//check cv
	//check if car in buffer
	//move from buffer
	//if buffer was full, notify cv
	//check quadrants lock
	//lock quadrants needed
	//move to out_car of right direction
	//print stuff
	int i;
	struct lane *l = (struct lane*) arg;

	while (l->inc != 0 || l->in_buf != 0) {/*either cars in in_cars or cars in buffer*/

		pthread_mutex_lock(&l->lock);

		while (l->in_buf == 0) {/*while there is no car in buf, wait for producer*/
			pthread_cond_wait(&l->producer_cv, &l->lock);
		}

		struct car *curr_car = l->buffer[l->head];

		int *p = compute_path(curr_car->in_dir, curr_car->out_dir);

		for (i = 0; i < 4; i++) {
			if (p[i] != 0) {
				pthread_mutex_lock(&isection.quad[p[i] - 1]);/*quad name starts with 1 while index starts with 0*/
			}
		}

		curr_car->next = l->out_cars;
		l->out_cars = curr_car;
		l->head++;
		l->head = l->head % LANE_LENGTH;/*circular buffer*/
		l->passed++;
		l->in_buf--;

		fprintf(stderr, "%d %d %d\n", curr_car->in_dir, curr_car->out_dir,
				curr_car->id);

		for (i = 3; i >= 0; i--) {
			if (p[i] != 0) {
				pthread_mutex_unlock(&isection.quad[p[i] - 1]);/*quad name starts with 1 while index starts with 0*/
			}
		}
		pthread_cond_signal(&l->consumer_cv);
		pthread_mutex_unlock(&l->lock);
	}

	return NULL;
}

/**
 * TODO: Fill in this function
 *
 * Given a car's in_dir and out_dir return a sorted 
 * list of the quadrants the car will pass through.
 * 
 */
int *compute_path(enum direction in_dir, enum direction out_dir) {
	int i;
	int *path = malloc(sizeof(int) * 5);

	for (i = 0; i < 5; i++) { //init array to 0
		path[i] = 0;
	}
	if (in_dir == out_dir) {	//if U-turn
		for (i = 1; i < 5; i++) {
			path[i - 1] = i;
		}
	} else if (in_dir == NORTH) {
		path[0] = 2;// if start from north, car must pass q2 first and never pass q1
		if (out_dir == SOUTH) {
			path[1] = 3;
		} else if (out_dir == EAST) {
			path[1] = 3;
			path[2] = 4;
		}
	} else if (in_dir == WEST) {
		if (out_dir == NORTH) {
			path[0] = 1;
			path[1] = 3;
			path[2] = 4;
		} else if (out_dir == SOUTH) {
			path[0] = 3;
		} else if (out_dir == EAST) {
			path[0] = 3;
			path[1] = 4;
		}
	} else if (in_dir == SOUTH) {
		if (out_dir == NORTH) {
			path[0] = 1;
			path[1] = 4;
		} else if (out_dir == WEST) {
			path[0] = 1;
			path[1] = 2;
			path[2] = 4;
		} else if (out_dir == EAST) {
			path[0] = 4;
		}
	} else if (in_dir == EAST) {
		path[0] = 1;	// if start from east, car must pass q1 first
		if (out_dir == WEST) {
			path[0] = 1;
			path[1] = 2;
		} else if (out_dir == SOUTH) {
			path[0] = 1;
			path[1] = 2;
			path[2] = 3;
		}
	}

	return path;
}

