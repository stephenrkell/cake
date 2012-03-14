#ifndef CAKE_COMMON_HPP_
#define CAKE_COMMON_HPP_

//#include <iostream>
#include <cassert> // is this okay?

namespace cake 
{ 
    //typedef struct { int data; } unspecified_wordsize_type;
    typedef struct __cake_dummy_struct 
	{ 
		struct __cake_dummy_struct *self;
		operator void*() { return self; }
	} unspecified_wordsize_type;
#if defined (X86_64) || (defined (__x86_64__))
	typedef long wordsize_integer_type;
#else
	typedef int wordsize_integer_type;
#endif
}

#endif
