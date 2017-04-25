#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "common.h"
#include "point.h"
#include "sorted_points.h"

struct sorted_points {
	/* you can define this struct to have whatever fields you want. */
    double x;
    double y;
    double length;
    struct sorted_points* next;
};

struct sorted_points *
sp_init()
{
	struct sorted_points *sp;

	sp = (struct sorted_points *)malloc(sizeof(struct sorted_points));
	assert(sp);

	TBD();

	return sp;
}

void
sp_destroy(struct sorted_points *sp)
{
	TBD();
	free(sp);
}

int
sp_add_point(struct sorted_points *sp, double x, double y)
{
	//TBD();
	struct sorted_points *temp;
	temp = (struct sorted_points *)malloc(sizeof(struct sorted_points));
	if(temp == NULL)
	{
		return 0;
	}
	temp->x = x;
	temp->y = y;
	
	
	return 1;
}

int
sp_remove_first(struct sorted_points *sp, struct point *ret)
{
	TBD();
	return 0;
}

int
sp_remove_last(struct sorted_points *sp, struct point *ret)
{
	TBD();
	return 0;
}

int
sp_remove_by_index(struct sorted_points *sp, int index, struct point *ret)
{
	TBD();
	return 0;
}

int
sp_delete_duplicates(struct sorted_points *sp)
{
	TBD();
	return -1;
}
