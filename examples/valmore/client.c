#include <stdio.h>
#include <stdlib.h>

struct gadget
{
 	int unused;
    
    int amplitude;
    char base;
    
    struct {
        float x;
        float y;
    } contained;   

    float density;
};
extern int pass(struct gadget *pg);
int main(void)
{
	/* HACK: injected a malloc for debugging */
	void *unused = malloc(42);

    struct gadget g = { 0, 42, 'g', {53.0, 2.1}, 1000000.0}, *pg = &g;

 	printf("Passing a gadget at %p, amplitude %d, base %d, containing (%f, %f), density %f.\n",
        pg, pg->amplitude, pg->base, 
        (double)pg->contained.x, (double)pg->contained.y, (double)pg->density);
    
    pass(pg);
	
 	printf("Now we have a gadget, unused %d, amplitude %d, base %d, containing (%f, %f), density %f; passing again.\n",
        pg->unused, pg->amplitude, pg->base, 
        (double)pg->contained.x, (double)pg->contained.y, (double)pg->density);
	
    pass(pg);

	
 	printf("Finally we have a gadget, unused %d, amplitude %d, base %d, containing (%f, %f), density %f.\n",
        pg->unused, pg->amplitude, pg->base, 
        (double)pg->contained.x, (double)pg->contained.y, (double)pg->density);
	return 0;
}
