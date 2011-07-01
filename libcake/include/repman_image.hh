#ifndef LIBCAKE_REPMAN_IMAGE_HH_
#define LIBCAKE_REPMAN_IMAGE_HH_

extern "C"
{
	#include "repman.h"
}

#include <iostream>
#include <processimage/process.hpp>

namespace cake
{
	extern process_image image; /* defined in repman_components.cc */
}

#endif
