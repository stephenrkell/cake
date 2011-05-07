#include <iostream>
#include <cassert>
namespace cake 
{ 
    //typedef struct { int data; } unspecified_wordsize_type;
    typedef struct __cake_dummy_struct { struct __cake_dummy_struct *self; } unspecified_wordsize_type;
#if defined (X86_64) || (defined (__x86_64__))
	typedef long wordsize_integer_type;
#else
	typedef int wordsize_integer_type;
#endif
}

#include "prelude/pairwise.hpp"
#include "prelude/style.hpp"

//    template <class ArgComponentTag, class OtherComponentTag, class ArgType, int RuleTag = 0>
//    struct corresponding_cxx_type //<ArgComponentTag, OtherComponentTag, ArgType, RuleTag>
//    {
//    	typedef unspecified_wordsize_type t;
//    };
    
    //template <class FirstComponentTag, class SecondComponentTag>
    

//} // end namespace cake

