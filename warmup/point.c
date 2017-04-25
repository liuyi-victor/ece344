#include <assert.h>
#include "common.h"
#include "point.h"
#include <math.h>

void
point_translate(struct point *p, double x, double y)
{
    p->x += x; 
    p->y += y;
    return;
}

double
point_distance(const struct point *p1, const struct point *p2)
{
    
    return sqrt(((p2->x - p1->x)*(p2->x - p1->x)) + ((p2->y - p1->y) * (p2->y - p1->y)));
}

int
point_compare(const struct point *p1, const struct point *p2)
{
    double l1 = ((p1->x)*(p1->x)+(p1->y)*(p1->y));
    double l2 = ((p2->x)*(p2->x)+(p2->y)*(p2->y));
    if(l1 < l2)
        return -1;
    else if(l1 == l2)
        return 0;
    else
        return 1;
}
