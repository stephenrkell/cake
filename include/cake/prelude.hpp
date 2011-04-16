#include <iostream>
#include <cassert>
namespace cake 
{ 
    //typedef struct { int data; } unspecified_wordsize_type;
    typedef struct {} dummy_struct;
    typedef dummy_struct *unspecified_wordsize_type;
#if defined (X86_64) || (defined (__x86_64__))
	typedef long wordsize_integer_type;
#else
	typedef int wordsize_integer_type;
#endif
}

#include "prelude/pairwise.hpp"
#include "prelude/style.hpp"
#define REP_ID(ident) (ident::rep_id)

//    template <class ArgComponentTag, class OtherComponentTag, class ArgType, int RuleTag = 0>
//    struct corresponding_cxx_type //<ArgComponentTag, OtherComponentTag, ArgType, RuleTag>
//    {
//    	typedef unspecified_wordsize_type t;
//    };
    
    //template <class FirstComponentTag, class SecondComponentTag>
    

//} // end namespace cake

