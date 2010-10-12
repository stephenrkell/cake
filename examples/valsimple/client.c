#include <stdio.h>

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
    struct gadget g = { 0, 42, 'g', {53.0, 2.1}, 1000000.0}, *pg = &g;

 	printf("Passing a gadget, amplitude %d, base %d, containing (%f, %f), density %f.\n",
        pg->amplitude, pg->base, 
        (double)pg->contained.x, (double)pg->contained.y, (double)pg->density);
    
    return pass(pg);
}
