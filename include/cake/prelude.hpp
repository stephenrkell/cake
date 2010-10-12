#include <iostream>
#include <cassert>
namespace cake 
{ 
    //typedef struct { int data; } unspecified_wordsize_type;
    typedef struct {} dummy_struct;
    typedef dummy_struct *unspecified_wordsize_type;
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

