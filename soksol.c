/* A simple BFS-based Sokoban solver.
 * Author: Alexander Fenster, fenster@fenster.name, 2013
 * Reads the field from input.txt.
 *
 * Note that maximum queue size can be as much as
 *     binomial(number of cells, number of boxes) * (number of cells - number of boxes).
 * This number can be huge for large fields.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NMAX 16
#define MAXBOX 16

#define WALL 1
#define BOX  2
#define DEST 4
#define CURR 8
// 4 bits per cell, 2 cells per byte

#define START 0
#define LEFT  1
#define RIGHT 2
#define UP    3
#define DOWN  4 

const char *direction(unsigned char dir)
{
	switch (dir) {
	case START: return "start";
	case LEFT:  return "left";
	case RIGHT: return "right";
	case UP:    return "up";
	case DOWN:  return "down";
	default:    return "????";
	}
}

struct queueitem {
	unsigned char *position;
	unsigned char curr; // hi: x, lo: y
	unsigned int prev;
	unsigned char direction;
};

struct hashitem {
	unsigned int queueidx;
	struct hashitem *next;
};
#define HASHSIZE 313507 // just some prime number

struct taskdata {
	struct queueitem **queue;
	unsigned int queuesize;
	unsigned int queuecurr;
	unsigned int queuealloc;
	unsigned int M, N;
	unsigned int xstart, ystart;
	unsigned int solved;
	unsigned int codelength;
	struct hashitem *H[HASHSIZE];
};

unsigned int hashcode(struct queueitem *item, struct taskdata *data)
{
	unsigned int result = item->curr;
	unsigned int i;
	for (i = 0; i < data->codelength; i++) {
		result = result * 31 + item->position[i];
	}
	return (result % HASHSIZE);
}

void hashtable_free(struct taskdata *data)
{
	unsigned int i;
	for (i = 0; i < HASHSIZE; i++) {
		while (data->H[i]) {
			struct hashitem *p = data->H[i]->next;
			free(data->H[i]);
			data->H[i] = p;
		}
	}
}

int check_solved(unsigned char field[NMAX][NMAX], struct taskdata *data) 
{
	unsigned int i, j;
	for (i = 1; i <= data->M; i++) {
		for (j = 1; j <= data->N; j++) {
			if (field[j][i] & DEST && !(field[j][i] & BOX)) {
				return 0;
			}
		}
	}
	return 1;
}

int empty(unsigned char c)
{
	return (! ((c & WALL) || (c & BOX)) );
}

void copy_field(unsigned char dst[NMAX][NMAX], unsigned char src[NMAX][NMAX])
{
	memcpy(dst, src, NMAX * NMAX);
}

void print_position(unsigned char field[NMAX][NMAX], struct taskdata *data)
{
	unsigned int i, j;
	printf("+");
	for (j = 1; j <= data->N; j++) {
		printf("---+");
	}
	printf("\n");
	for (i = 1; i <= data->M; i++) {
		printf("|");
		for (j = 1; j <= data->N; j++) {
			char s[4] = "   ";
			if (field[i][j] & WALL) {
				s[0] = s[1] = s[2] = '#';
			}
			if (field[i][j] & BOX) {
				s[0] = '@';
			}
			if (field[i][j] & DEST) {
				s[1] = 'o';
			}
			if (field[i][j] & CURR) {
				s[2] = '%';
			}
			printf("%s|", s);
		}
		printf("\n");
		printf("+");
		for (j = 1; j <= data->N; j++) {
			printf("---+");
		}
		printf("\n");
	}
}

unsigned char *encode_position(unsigned char field[NMAX][NMAX], struct taskdata *data)
{
	unsigned char *buffer = malloc(data->codelength);
	unsigned int i, j;
	unsigned int idx = 0;
	int half = 0;
	unsigned char value = 0;
	for (i = 1; i <= data->M; i++) {
		for (j = 1; j <= data->N; j++) {
			if (half == 0) {
				value |= field[i][j];
				half = 1;
			}
			else {
				value |= (field[i][j] << 4);
				buffer[idx++] = value;
				half = 0;
				value = 0;
			}
		}
	}
	if (half == 1) {
		buffer[idx++] = value;
	}
	return buffer;
}

void decode_position(unsigned char *buffer, unsigned char field[NMAX][NMAX], struct taskdata *data)
{
	unsigned int i, j;
	int half = 0;
	unsigned char value = 0;
	unsigned int idx = 0;
	for (i = 1; i <= data->M; i++) {
		for (j = 1; j <= data->N; j++) {
			if (half == 0) {
				value = buffer[idx++];
				field[i][j] = value & 0x0f;
				half = 1;
			}
			else {
				field[i][j] = value >> 4;
				half = 0;
			}
		}
	}
}

unsigned char encode_curr(unsigned char x, unsigned char y)
{
	return ((x << 4) | y);
}

void decode_curr(unsigned char curr, unsigned char *x, unsigned char *y)
{
	*x = curr >> 4;
	*y = curr & 0x0f;
}

int differ(unsigned char *buffer1, unsigned char *buffer2, struct taskdata *data)
{
	return memcmp(buffer1, buffer2, data->codelength);
}

void enqueue_if_unique(struct queueitem *item, struct taskdata *data)
{
	unsigned int hcode = hashcode(item, data);
	struct hashitem *hitem;
	for (hitem = data->H[hcode]; hitem; hitem = hitem->next) {
		if (data->queue[hitem->queueidx]->curr != item->curr) {
			continue;
		}
		if (differ(data->queue[hitem->queueidx]->position, item->position, data)) {
			continue;
		}
		// just drop it
		free(item->position);
		free(item);
		return;
	}
	if (data->queuesize == data->queuealloc) {
		unsigned int newsize = data->queuealloc == 0 ? 1024 : 2 * data->queuealloc;
		data->queue = realloc(data->queue, sizeof(*data->queue) * newsize); // memory errors? don't care
		data->queuealloc = newsize;
	}
	data->queue[data->queuesize] = item;
	hitem = malloc(sizeof(*hitem));
	hitem->queueidx = data->queuesize;
	hitem->next = data->H[hcode];
	data->H[hcode] = hitem;
	if (data->queuesize % 100000 == 0) {
		printf("queue size: %d, still working...\n", data->queuesize);
		fflush(stdout);
	}
	data->queuesize++;
}

void dequeue(struct queueitem **item, unsigned int *idx, struct taskdata *data)
{
	*idx = data->queuecurr;
	*item = data->queue[data->queuecurr];
	data->queuecurr++;
}

int queue_empty(struct taskdata *data)
{
	return (data->queuecurr == data->queuesize);
}

void queue_free(struct taskdata *data)
{
	unsigned int i;
	printf("final queue size: %d\n", data->queuesize);
	for (i = 0; i < data->queuesize; i++) {
		free(data->queue[i]->position);
		free(data->queue[i]);
	}
	free(data->queue);
}

void show_solution(unsigned char field[NMAX][NMAX], unsigned int prev, unsigned char dir, struct taskdata *data)
{
	unsigned int *indices = malloc(data->queuesize * sizeof(unsigned int));
	unsigned char tmpfield[NMAX][NMAX];
	unsigned int length = 0;
	unsigned int i, curr;
	do {
		indices[length++] = prev;
		curr = prev;
		prev = data->queue[prev]->prev;
	} while (prev != curr);
	printf("Solution found! %d steps.\n", length);
	for (i = 0; length-- > 0; i++) {
		printf("======== Step %3d ======== %s\n", i, direction(data->queue[indices[length]]->direction));
		decode_position(data->queue[indices[length]]->position, tmpfield, data);
		print_position(tmpfield, data);
	}
	free(indices);
	printf("======== Step %3d ======== %s\n", i, direction(dir));
	print_position(field, data);
	printf("Solved!\n");
	data->solved = 1;
}

void process_coords(unsigned char field[NMAX][NMAX], 
                    unsigned char x, unsigned char y, unsigned char nx, unsigned char ny, 
		    unsigned int prev, unsigned char direction, struct taskdata *data)
{
	static unsigned char tmpfield[NMAX][NMAX];
	struct queueitem *next;
	if (data->solved) {
		return;
	}
	if (empty(field[ny][nx])) {
		copy_field(tmpfield, field);
		tmpfield[y][x] &= (~CURR);
		tmpfield[ny][nx] |= CURR;
		if (check_solved(tmpfield, data)) {
			show_solution(tmpfield, prev, direction, data);
			return;
		}
		next = malloc(sizeof(*next));
		next->position = encode_position(tmpfield, data);
		next->curr = encode_curr(nx, ny);
		next->prev = prev;
		next->direction = direction;
		enqueue_if_unique(next, data);
	}
	else if (field[ny][nx] & BOX) {
		char dx = nx - x;
		char dy = ny - y;
		unsigned char nextx = x + dx + dx;
		unsigned char nexty = y + dy + dy;
		if (empty(field[nexty][nextx])) {
			copy_field(tmpfield, field);
			tmpfield[y][x] &= (~CURR);
			tmpfield[ny][nx] |= CURR;
			tmpfield[ny][nx] &= (~BOX);
			tmpfield[nexty][nextx] |= BOX;
			if (check_solved(tmpfield, data)) {
				show_solution(tmpfield, prev, direction, data);
				return;
			}
			next = malloc(sizeof(*next));
			next->position = encode_position(tmpfield, data);
			next->curr = encode_curr(nx, ny);
			next->prev = prev;
			next->direction = direction;
			enqueue_if_unique(next, data);
		}
	}
}

void solve(unsigned char field[NMAX][NMAX], struct taskdata *data)
{
	struct queueitem *first;

	data->codelength = data->N * data->M / 2 + (data->N * data->M) % 2;
	first = malloc(sizeof(*first));
	first->position = encode_position(field, data);
	first->curr = encode_curr(data->xstart, data->ystart);
	first->prev = 0;
	first->direction = START;

	enqueue_if_unique(first, data);
	while (!queue_empty(data) && !data->solved) {
		struct queueitem *item;
		unsigned int queueidx;
		unsigned char x, y;

		dequeue(&item, &queueidx, data);
		decode_position(item->position, field, data);
		decode_curr(item->curr, &x, &y);

		// up
		if (y > 1) {
			process_coords(field, x, y, x, y - 1, queueidx, UP, data);
		}
		// down
		if (y < data->M) {
			process_coords(field, x, y, x, y + 1, queueidx, DOWN, data);
		}
		// left
		if (x > 1) {
			process_coords(field, x, y, x - 1, y, queueidx, LEFT, data);
		}
		// right
		if (x < data->N) {
			process_coords(field, x, y, x + 1, y, queueidx, RIGHT, data);
		}
	}

	if (!data->solved) {
		printf("No solution.\n");
	}

	queue_free(data);
	hashtable_free(data);
}

void input(unsigned char field[NMAX][NMAX], struct taskdata *data)
{
	unsigned int i, j;
	unsigned int boxcount = 0;
	unsigned int destcount = 0;
	FILE *f;

	for (i = 0; i < NMAX; i++) {
		for (j = 0; j < NMAX; j++) {
			field[i][j] = WALL;
		}
	}

	f = fopen("input.txt", "r");
	if (!f) {
		perror("fopen");
		exit(1);
	}
	fscanf(f, "%d %d\n", &data->M, &data->N);

	data->xstart = NMAX;
	data->ystart = NMAX;

	for (i = 1; i <= data->M; i++) {
		for (j = 1; j <= data->N; j++) {
			int c;
			unsigned char value = 0;
			do {
				c = fgetc(f);
			} while (c != EOF && isspace(c));
			if (c == EOF) {
				printf("Incomplete input\n");
				exit(1);
			}
			if (c == 'x') {
				value |= BOX;
				boxcount++;
			}
			else if (c == 'X') {
				value |= BOX;
				value |= DEST;
				boxcount++;
				destcount++;
			}
			else if (c == 's') {
				if (data->xstart != NMAX) {
					printf("Multiple start points defined\n");
					exit(1);
				}
				data->xstart = j;
				data->ystart = i;
				value |= CURR;
			}
			else if (c == 'S') {
				if (data->xstart != NMAX) {
					printf("Multiple start points defined\n");
					exit(1);
				}
				data->xstart = j;
				data->ystart = i;
				value |= CURR;
				value |= DEST;
				destcount++;
			}
			else if (c == 'o') {
				value |= DEST;
				destcount++;
			}
			else if (c == '#') {
				value |= WALL;
			}
			else if (c == '.') {
				value = 0;
			}
			else {
				printf("Unknown character %c\n", c);
				exit(1);
			}
			field[i][j] = value;
		}
	}

	fclose(f);
	
	if (boxcount != destcount) {
		printf("Number of boxes is not equal to number of destinations\n");
		exit(1);
	}

	if (data->xstart == NMAX) {
		printf("Start point is not defined\n");
		exit(1);
	}

	printf("Start: (%d, %d)\n", data->xstart, data->ystart);
	printf("Initial position:\n");
	print_position(field, data);
}

int main()
{
	unsigned char field[NMAX][NMAX];
	struct taskdata sdata;

	memset(&sdata, '\0', sizeof(sdata));
	
	input(field, &sdata);
	solve(field, &sdata);

	return 0;
}
