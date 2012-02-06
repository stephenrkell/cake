extern "C" {
#include "repman.h"
}
#include "runtime.hpp"
#include <boost/type_traits/remove_const.hpp>
#include <boost/type_traits/remove_reference.hpp>
#define REP_ID(ident) (ident::marker::rep_id)

    template <
        typename ComponentPair, 
        typename InFirst, 
//        int RuleTag,
        bool DirectionIsFromFirstToSecond
    > struct corresponding_type_to_first
    {}; /* we specialize this for various InSeconds */ 
    template <
        typename ComponentPair, 
        typename InSecond, 
//        int RuleTag,
        bool DirectionIsFromSecondToFirst
    > struct corresponding_type_to_second
    {}; /* we specialize this for various InFirsts */ 
    template <
        typename ComponentPair, 
        typename InFirstIsAPtr, 
//        int RuleTag,
        bool DirectionIsFromFirstToSecond
    > struct corresponding_type_to_first <ComponentPair, InFirstIsAPtr*, /*RuleTag, */ DirectionIsFromFirstToSecond>
    { typedef void *__cake_default_to___cake_default_in_second; }; /* we specialize this for various InSeconds */ 
    template <
        typename ComponentPair, 
        typename InSecondIsAPtr, 
//        int RuleTag,
        bool DirectionIsFromSecondToFirst
    > struct corresponding_type_to_second<ComponentPair, InSecondIsAPtr*, /*RuleTag, */ DirectionIsFromSecondToFirst> 
    { typedef void *__cake_default_to___cake_default_in_first; }; /* we specialize this for various InFirsts */ 

namespace cake
{
	/* These represent value correspondences which Cake programmers don't
	 * have to write explicitly: between base types and pointers. */
#define template_head4_map_keyed_on_first_module(InFirst_typename) \
    template < \
        typename ComponentPair,  \
        typename InFirst_typename,  \
/*        int RuleTag,*/ \
        bool DirectionIsFromFirstToSecond \
    > struct corresponding_type_to_first 

#define template_head4_map_keyed_on_second_module(InSecond_typename) \
    template < \
        typename ComponentPair,  \
        typename InSecond_typename,  \
/*        int RuleTag,*/ \
        bool DirectionIsFromSecondToFirst \
    > struct corresponding_type_to_second 
	
#define template_head3_map_keyed_on_first_module \
    template < \
        typename ComponentPair,  \
/*        int RuleTag,*/ \
        bool DirectionIsFromFirstToSecond \
    > struct corresponding_type_to_first 

#define template_head3_map_keyed_on_second_module \
    template < \
        typename ComponentPair,  \
/*        int RuleTag,*/ \
        bool DirectionIsFromSecondToFirst \
    > struct corresponding_type_to_second 

// empty mappings -- by default, there is no corresponding type

template_head4_map_keyed_on_first_module(InFirst) {};
template_head4_map_keyed_on_second_module(InSecond) {};
	
// mappings for pointers -- by default, all pointer types correspond to void*
template_head4_map_keyed_on_first_module(InFirstIsAPtr)
<ComponentPair, InFirstIsAPtr*, /*RuleTag,*/ DirectionIsFromFirstToSecond>
: public corresponding_type_to_first<ComponentPair, void, /*RuleTag,*/
	DirectionIsFromFirstToSecond> {
	typedef void *__cake_default_to___cake_default_in_second;
         struct rule_tag_in_second_given_first_artificial_name___cake_default { enum __cake_rule_tags {
__cake_default = 0         }; };
};	
template_head4_map_keyed_on_second_module(InSecondIsAPtr)
<ComponentPair, InSecondIsAPtr*, /*RuleTag,*/ DirectionIsFromSecondToFirst>
: public corresponding_type_to_second<ComponentPair, void, /*RuleTag,*/
	DirectionIsFromSecondToFirst> {
typedef void *__cake_default_to___cake_default_in_first;
         struct rule_tag_in_first_given_second_artificial_name___cake_default { enum __cake_rule_tags {
__cake_default = 0         }; };
};

// mappings for base types -- by default, all base types correspond to themselves
#define pair_of_mappings(base_type) \
template_head3_map_keyed_on_first_module \
<ComponentPair, base_type, /*RuleTag,*/ DirectionIsFromFirstToSecond> \
: public corresponding_type_to_first<ComponentPair, void, /*RuleTag,*/ \
	DirectionIsFromFirstToSecond> { \
	typedef base_type __cake_default_to___cake_default_in_second; \
         struct rule_tag_in_second_given_first_artificial_name___cake_default { enum __cake_rule_tags { \
__cake_default = 0         }; }; \
};	 \
template_head3_map_keyed_on_second_module \
<ComponentPair, base_type, /*RuleTag,*/ DirectionIsFromSecondToFirst> \
: public corresponding_type_to_second<ComponentPair, void, /*RuleTag,*/ \
	DirectionIsFromSecondToFirst> { \
	typedef base_type __cake_default_to___cake_default_in_first; \
         struct rule_tag_in_first_given_second_artificial_name___cake_default { enum __cake_rule_tags { \
__cake_default = 0         }; }; \
}

pair_of_mappings(bool);
pair_of_mappings(char);
pair_of_mappings(wchar_t);
pair_of_mappings(unsigned char);
pair_of_mappings(short);
pair_of_mappings(unsigned short);
pair_of_mappings(long);
pair_of_mappings(unsigned long);
pair_of_mappings(float);
pair_of_mappings(double);
pair_of_mappings(long double);

/* Now we can define the template specialization for base types.
 * Note that this works because we index these templates at the 
 * point of use, specifying a ComponentPair and all the other stuff.
 * In particular, we don't define a partial specialization for *just*
 * the ComponentPair -- whenever we specify a ComponentPair, we also specify
 * a type. So these will get overridden effectively only if a ComponentPair
 * defines its own rules for a particular type */

	/* All value_convert operator()s MUST have the same ABI, so that they
	 * can be dispatched to by the runtime!
	 *
	 * That ABI is:
	 * Pointer or reference "from", then pointer "to". 
	 *
	 * Return values are an exception. These may vary the ABI, so
	 * returning by value is okay.
	 * We generally get around this as follows.
	 * The runtime table entries are populated by instances of value_convert_function.
	 * This is defined by a function template (at the bottom) which ignores
	 * the return value. Therefore, the class template specializations may each have different
	 * return value ABIs, but the function template instances will not be sensitive to the
	 * differences. */

	template <typename From, typename To, typename FromComponent, typename ToComponent, int RuleTag>  
    struct value_convert 
    { 
    	To operator()(const From& from, To *p_to = 0) const 
        { 
			// rely on compiler's default conversions 
			if (p_to) *p_to = from; return from; 
        } 
    }; 
	template <typename FromIsAPtr, typename ToIsAPtr, typename FromComponent, typename ToComponent, int RuleTag>
    struct value_convert<FromIsAPtr*, ToIsAPtr*, FromComponent, ToComponent, RuleTag>
    { 
    	ToIsAPtr* operator()(FromIsAPtr* from, ToIsAPtr **p_to = 0) const 
        { 
			print_object(from);
			
			struct co_object_group *found_group;
			void *found_co_object;
			
			found_co_object = find_co_object(from,
				FromComponent::rep_id, ToComponent::rep_id,
				&found_group);
			
			if (!found_co_object)
			{
				fprintf(stderr, "Warning: assuming object at %p is its own co-object.\n",
					from);
				if (p_to) *p_to = reinterpret_cast<__typeof(*p_to)>(from);
				return reinterpret_cast<__typeof(*p_to)>(from);
			}
			else
			{
				if (p_to) *p_to = reinterpret_cast<__typeof(*p_to)>(found_co_object);
				return reinterpret_cast<__typeof(*p_to)>(found_co_object);
			}
				/*
				 * Cake compiler:
				 * How do we identify source/sink data types
				 * in the tables we output?
				 * 
				 * We have to account for
				 * - 1. canonicalisation. We assume that within a Cake component,
				 *      we have some canonical name for a data type. This is just
				 *      a concatenation of strings (compile directory, compiler). EASY. 
				 *      Except:
				 *      the "compilation directory" idea doesn't quite canonicalise enough,
				 *      because in the same component there will be multiple directories
				 *      (usually with a common prefix). That's okay -- we can scan all
				 *      compilation units in the component and use the common prefix.
				 * - 2. runtime discoverability. The runtime has to do lookups
				 *      in these tables, so that given a compilation-unit-level
				 *      DWARF type (e.g. from heap object discovery) 
				 *      it can index the table correctly.
				 *      Again, this is easy. The runtime can discover 
				 *      a name for the data type. Then it needs to compute the 
				 *      two strings that we used earlier. Unfortunately, it doesn't
				 *      know where component boundaries are. So we have to emit some
				 *      metadata to tell it. In the wrapper file, we can emit
				 *      a set of tuples
				 *      <compilation-unit-name, full-compil-directory-name, compiler-ident>
				 *      as a string with a name __cake_component_<component-name>.
				 *      It can then do the longest-common-prefix calculation on the
				 *      full-compil-directory-name
				 * 
				 * For the Cake runtime, we must be able to map from
				 * a Cake component identifier to a set of these.
				 *  */
				
//			}
				
        } 
    }; 
    // HACK: allow conversion from "unspecified" to/from any pointer type
	// FIXME: These should all compile to an instance of the pointer-pointer case.
	// Currently they don't do the right thing if both args point to rep-incompatible objects. 
	// We will probably have to parameterise on modules,
	// so that we can select the "default" corresponding type. Hmm.
    template <typename FromPtr, typename FromComponent, typename ToComponent> 
    struct value_convert<FromPtr*, unspecified_wordsize_type, FromComponent, ToComponent, 0> 
    { 
        unspecified_wordsize_type operator ()(FromPtr* from, unspecified_wordsize_type *p_to = 0) const // NOT a reference 
        { 
        	unspecified_wordsize_type ret;
            ret = *reinterpret_cast<unspecified_wordsize_type*>(&from);  
			if (p_to) *p_to = ret;
            return ret;
        } 
	}; 
    template <typename ToPtr, typename FromComponent, typename ToComponent> 
    struct value_convert<unspecified_wordsize_type, ToPtr*, FromComponent, ToComponent, 0> 
    { 
        ToPtr* operator ()(const unspecified_wordsize_type& from, ToPtr **p_to = 0) const 
        { 
			auto ret = *reinterpret_cast<ToPtr* const*>(&from);
			if (p_to) *p_to = ret;
    	    return ret;
        } 
	}; 
	// another HACK: same but for wordsize integers
    template <typename FromPtr, typename FromComponent, typename ToComponent> 
    struct value_convert<FromPtr*, wordsize_integer_type, FromComponent, ToComponent, 0> 
    { 
        wordsize_integer_type operator ()(FromPtr* from, wordsize_integer_type *p_to = 0) const // NOT a reference 
        { 
        	wordsize_integer_type ret;
            ret = *reinterpret_cast<wordsize_integer_type*>(&from);  
			if (p_to) *p_to = ret;
            return ret;
        } 
	}; 
    template <typename ToPtr, typename FromComponent, typename ToComponent> 
    struct value_convert<wordsize_integer_type, ToPtr*, FromComponent, ToComponent, 0> 
    { 
        ToPtr* operator ()(const wordsize_integer_type& from, ToPtr **p_to = 0) const 
        { 
    	    auto ret = *reinterpret_cast<ToPtr**>(&from);
			if (p_to) *p_to = ret;
			return ret;
        } 
	}; 
	// conversions between wordsize integers and wordsize opaque data
    template <typename FromComponent, typename ToComponent> 
    struct value_convert<wordsize_integer_type, unspecified_wordsize_type, FromComponent, ToComponent, 0> 
    { 
        unspecified_wordsize_type operator ()(const wordsize_integer_type& from, unspecified_wordsize_type *p_to = 0) const 
        { 
			assert(sizeof (wordsize_integer_type) == sizeof (unspecified_wordsize_type));
        	unspecified_wordsize_type ret 
             = *reinterpret_cast<const unspecified_wordsize_type*>(&from);  
			if (p_to) *p_to = ret;
            return ret;
        } 
	}; 
#if defined (X86_64) || (defined (__x86_64__))
    template <typename FromComponent, typename ToComponent> 
    struct value_convert<int, unspecified_wordsize_type, FromComponent, ToComponent, 0> 
    { 
        unspecified_wordsize_type operator ()(const int& from, unspecified_wordsize_type *p_to = 0) const 
        { 
			auto tmp_long = static_cast<long>(from);
        	unspecified_wordsize_type ret 
             = *reinterpret_cast<unspecified_wordsize_type*>(&tmp_long);
			if (p_to) *p_to = ret;
            return ret;
        } 
	}; 
#endif
    template <typename FromComponent, typename ToComponent> 
    struct value_convert<unspecified_wordsize_type, wordsize_integer_type, FromComponent, ToComponent, 0> 
    { 
        wordsize_integer_type operator ()(const unspecified_wordsize_type& from, wordsize_integer_type *p_to = 0) const 
        {
			assert(sizeof (wordsize_integer_type) == sizeof (unspecified_wordsize_type));
    	    auto ret = *reinterpret_cast<const wordsize_integer_type*>(&from);
			if (p_to) *p_to = ret;
			return ret;
        } 
	}; 
#if defined (X86_64) || (defined (__x86_64__))
    template <typename FromComponent, typename ToComponent> 
    struct value_convert<unspecified_wordsize_type, int, FromComponent, ToComponent, 0> 
    { 
        int operator ()(const unspecified_wordsize_type& from, int *p_to = 0) const 
        {
    	    auto ret = static_cast<int>(*reinterpret_cast<const long*>(&from));
			if (p_to) *p_to = ret;
			return ret;
        } 
	}; 
#endif
   // handle those pesky zero-length array
    template <typename FromZeroArray, typename T, typename FromComponent, typename ToComponent> 
    struct value_convert<FromZeroArray[0], T, FromComponent, ToComponent, 0> 
    { 
        T operator ()(FromZeroArray (&from)[0], T *p_to = 0) const // NOT a reference 
        { 
        	T ret;
            ret = *reinterpret_cast<T*>(&from);
			if (p_to) *p_to = ret;
            return ret;
        } 
	}; 
    template <typename T, typename ToZeroArray, typename FromComponent, typename ToComponent> 
    struct value_convert<T, ToZeroArray[0], FromComponent, ToComponent, 0> 
    { 
		typedef ToZeroArray ToType[0];
        void operator ()(const T& from, ToType *p_to = 0) const 
        { 
    	    //return *reinterpret_cast<ToZeroArray*>(&from/*.data*/);
			assert(false); // FIXME: we should memcpy, but we don't know how much
        } 
	}; 

    template <typename FirstComponentTag, typename SecondComponentTag>
    struct component_pair {
//         template <
// 		    typename To /*= typename cake::corresponding_cxx_type<FirstComponentTag, SecondComponentTag, Arg, 0>::t*/,
//             typename From = ::cake::unspecified_wordsize_type, 
//             int RuleTag = 0
//         >
//         static 
//         To
//         value_convert_from_first_to_second(From arg)
//         {
//     	    return value_convert<From, 
//                 To,
// 				FirstComponentTag,
// 				SecondComponentTag,
//                 RuleTag
//                 >().operator()(arg);
//         }
//         template <
//             typename To /*= typename cake::corresponding_cxx_type<FirstComponentTag, SecondComponentTag, Arg, 0>::t*/,
//             typename From = ::cake::unspecified_wordsize_type, 
//             int RuleTag = 0
//         >
//         static 
//         To
//         value_convert_from_second_to_first(From arg)
//         {
//     	    return value_convert<From, 
//                 To,
// 				SecondComponentTag,
// 				FirstComponentTag,
//                 RuleTag
//                 >().operator()(arg);
//         }	
    };

	// now we can define a function template to wrap all these up
	template <typename Source, typename Sink, typename FromComponent, typename ToComponent, int RuleTag>
	inline
	void *
	value_convert_function(
		Source *from,
		Sink *to)
	{
		value_convert<Source, Sink, FromComponent, ToComponent, RuleTag>().operator()(*from, to);
		return to;
	}
}
