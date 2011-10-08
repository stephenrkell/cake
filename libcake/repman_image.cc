#include "repman_image.hh"

void print_object(void *obj) /* defined in repman.h */
{
	pmirror::self.print_object(std::cerr, obj);
}
