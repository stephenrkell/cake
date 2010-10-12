#include <stdio.h>

struct gizmo
{
    int amplitude;
    int base;
    
    struct {
        double x;
        double y;
    } contained;   

    double density;
};

int pass(struct gizmo *pg)
{
 	printf("Received a gizmo, amplitude %d, base %d, containing (%f, %f), density %f.\n",
        pg->amplitude, pg->base, pg->contained.x, pg->contained.y, pg->density);
    return 0;
}
