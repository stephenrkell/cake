#include <stdio.h>
#include <assert.h>

struct gizmo
{
    int amplitude;
    int base;
    
    struct {
        double x;
        double y;
    } contained;   

    double density;
	
	double unuzed;
};

static int called_already;

int pass(struct gizmo *pg)
{
   printf("Received a gizmo, amplitude %d, base %d, containing (%f, %f), density %f, unuzed %f.\n",
        pg->amplitude, pg->base, pg->contained.x, pg->contained.y, pg->density, pg->unuzed);
   printf("Clearing amplitude...\n");
   pg->amplitude = 0;
   // also clobber unuzed, and check it doesn't get re-set to 69105
   assert((!called_already && pg->unuzed == 69105.0)
        || (called_already && pg->unuzed == 69999.0));
   pg->unuzed = 69999.0;
   called_already = 1;
   return 0;
}
