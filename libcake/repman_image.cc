#include "repman_image.hh"

void print_object(const void *obj) /* defined in repman.h */
{
	pmirror::self.print_object(std::cerr, const_cast<void*>(obj));
}
