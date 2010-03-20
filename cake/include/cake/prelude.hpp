#include <iostream>
namespace cake 
{ 
    //typedef struct { int data; } unspecified_wordsize_type;
    typedef struct {} dummy_struct;
    typedef dummy_struct *unspecified_wordsize_type;
    
    template <typename Arg1, typename Arg2, int RuleTag = 0>
    struct equal
    {
        bool operator()(const Arg1& arg1, const Arg2& arg2) const
        {
                return arg1 == arg2;
        }
    };
    template <>
    struct equal<char *, char *, 0>
    {
        bool operator()(const char *const& arg1, const char *const& arg2) const
        {
            // could use strcmp, but I want to keep things self-contained for now
            const char *pos1 = arg1;
            const char *pos2 = arg2;
            while (*pos1 != '\0' && *pos2 != '\0' && *pos1 == *pos2)
            {
                ++pos1; ++pos2;
            }
            if (*pos1 != *pos2) return false; else return true;
        }
    };
    template <>
    struct equal<unspecified_wordsize_type, unspecified_wordsize_type, 0>
    {
        bool operator()(const unspecified_wordsize_type& arg1, 
        	const unspecified_wordsize_type& arg2) const
        {
            // FIXME: can make this a compile-time fail?
            throw "Insufficient type information to test equality.";
            return false;
        }        
    };
    template <typename Arg1>
    struct equal<Arg1, unspecified_wordsize_type, 0>
    {
        bool operator()(const Arg1& arg1, const unspecified_wordsize_type arg2) const
        {
                return equal<Arg1, Arg1, 0>()(arg1, static_cast<Arg1>(reinterpret_cast<int>(arg2)));
        }
    };
    template <typename Arg1>
    struct equal<Arg1*, unspecified_wordsize_type, 0>
    {
        bool operator()(Arg1 *const & arg1, const unspecified_wordsize_type arg2) const
        {
                return equal<Arg1*, Arg1*, 0>()(arg1, reinterpret_cast<Arg1*>(arg2));
        }
    };    
    template <typename Arg2>
    struct equal<unspecified_wordsize_type, Arg2, 0>
    {
        bool operator()(const unspecified_wordsize_type arg1, const Arg2& arg2) const
        {
                return equal<Arg2, Arg2, 0>()(static_cast<Arg2>(reinterpret_cast<int>(arg1)), arg2);
        }
    };
    template <typename Arg2>
    struct equal<unspecified_wordsize_type, Arg2*, 0>
    {
        bool operator()(const unspecified_wordsize_type arg1, Arg2 *const& arg2) const
        {
                return equal<Arg2*, Arg2*, 0>()(reinterpret_cast<Arg2*>(arg1), arg2);
        }
    };
    
	template <typename From, typename To, int RuleTag = 0>  
    struct value_convert 
    { 
    	To operator()(const From& from) const 
        { 
	    	return from; // rely on compiler's default conversions 
        } 
    }; 
    // HACK: allow conversion from "unspecified" to/from any pointer type
    template <typename FromPtr> 
    struct value_convert<FromPtr*, unspecified_wordsize_type, 0> 
    { 
        unspecified_wordsize_type operator ()(FromPtr* from) const // NOT a reference 
        { 
        	unspecified_wordsize_type ret;
            ret = reinterpret_cast<unspecified_wordsize_type>(from);  
            return ret;
        } 
	}; 
    template <typename ToPtr> 
    struct value_convert<unspecified_wordsize_type, ToPtr*, 0> 
    { 
        ToPtr* operator ()(const unspecified_wordsize_type& from) const 
        { 
    	    return reinterpret_cast<ToPtr*>(from.data);
        } 
	}; 
    template <> 
    struct value_convert<int, unspecified_wordsize_type, 0> 
    { 
        unspecified_wordsize_type operator ()(const int& from) const 
        { 
        	unspecified_wordsize_type ret 
             = reinterpret_cast<unspecified_wordsize_type>(from);  
            return ret;
        } 
	}; 
    template <> 
    struct value_convert<unspecified_wordsize_type, int, 0> 
    { 
        int operator ()(const unspecified_wordsize_type& from) const 
        { 
    	    return reinterpret_cast<int>(from);
        } 
	}; 
    
    template <int StyleId> 
    struct style_traits {};
    
    template <>
    struct style_traits<0> // default style
    {
    	// define the style's DWARF types for each kind of literal in the Cake language
        typedef const char *STRING_LIT;
        typedef long double CONST_ARITH;
        
        template <typename LiteralType>
        LiteralType construct_literal(LiteralType arg)
        {
        	return arg;
        }
        
    };

} 
