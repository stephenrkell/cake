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
        int RuleTag,
        bool DirectionIsFromFirstToSecond
    > struct corresponding_type_to_first
    {}; /* we specialize this for various InSeconds */ 
    template <
        typename ComponentPair, 
        typename InSecond, 
        int RuleTag,
        bool DirectionIsFromSecondToFirst
    > struct corresponding_type_to_second
    {}; /* we specialize this for various InFirsts */ 
    template <
        typename ComponentPair, 
        typename InFirstIsAPtr, 
        int RuleTag,
        bool DirectionIsFromFirstToSecond
    > struct corresponding_type_to_first <ComponentPair, InFirstIsAPtr*, RuleTag, DirectionIsFromFirstToSecond>
    { typedef void *in_second; }; /* we specialize this for various InSeconds */ 
    template <
        typename ComponentPair, 
        typename InSecondIsAPtr, 
        int RuleTag,
        bool DirectionIsFromSecondToFirst
    > struct corresponding_type_to_second<ComponentPair, InSecondIsAPtr*, RuleTag, DirectionIsFromSecondToFirst> 
    { typedef void *in_first; }; /* we specialize this for various InFirsts */ 

namespace cake
{
	/* These represent value correspondences which Cake programmers don't
	 * have to write explicitly: between base types and pointers. */
#define template_head4_map_keyed_on_first_module(InFirst_typename) \
    template < \
        typename ComponentPair,  \
        typename InFirst_typename,  \
        int RuleTag, \
        bool DirectionIsFromFirstToSecond \
    > struct corresponding_type_to_first 

#define template_head4_map_keyed_on_second_module(InSecond_typename) \
    template < \
        typename ComponentPair,  \
        typename InSecond_typename,  \
        int RuleTag, \
        bool DirectionIsFromSecondToFirst \
    > struct corresponding_type_to_second 
	
#define template_head3_map_keyed_on_first_module \
    template < \
        typename ComponentPair,  \
        int RuleTag, \
        bool DirectionIsFromFirstToSecond \
    > struct corresponding_type_to_first 

#define template_head3_map_keyed_on_second_module \
    template < \
        typename ComponentPair,  \
        int RuleTag, \
        bool DirectionIsFromSecondToFirst \
    > struct corresponding_type_to_second 

// empty mappings -- by default, there is no corresponding type

template_head4_map_keyed_on_first_module(InFirst) {};
template_head4_map_keyed_on_second_module(InSecond) {};
	
// mappings for pointers -- by default, all pointer types correspond to void*
template_head4_map_keyed_on_first_module(InFirstIsAPtr)
<ComponentPair, InFirstIsAPtr*, RuleTag, DirectionIsFromFirstToSecond>
: public corresponding_type_to_first<ComponentPair, void, RuleTag,
	DirectionIsFromFirstToSecond> {
	typedef void *in_second;
};	
template_head4_map_keyed_on_second_module(InSecondIsAPtr)
<ComponentPair, InSecondIsAPtr*, RuleTag, DirectionIsFromSecondToFirst>
: public corresponding_type_to_second<ComponentPair, void, RuleTag,
	DirectionIsFromSecondToFirst> {
typedef void *in_first;
};

// mappings for base types -- by default, all base types correspond to themselves
#define pair_of_mappings(base_type) \
template_head3_map_keyed_on_first_module \
<ComponentPair, base_type, RuleTag, DirectionIsFromFirstToSecond> \
: public corresponding_type_to_first<ComponentPair, void, RuleTag, \
	DirectionIsFromFirstToSecond> { \
	typedef base_type in_second; \
};	 \
template_head3_map_keyed_on_second_module \
<ComponentPair, base_type, RuleTag, DirectionIsFromSecondToFirst> \
: public corresponding_type_to_second<ComponentPair, void, RuleTag, \
	DirectionIsFromSecondToFirst> { \
	typedef base_type in_first; \
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

	/* All value_convert operator()s MUST have the same ABI!
	 * Pointer or reference "from", then pointer "to". 
 	 * Return values vary the ABI depending on the returned object size; 
	 * we generally get around this by only using return values in the
	 * class templates. The function template (at the bottom) ignores
	 * the return value. */

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
			assert(found_co_object);
			
			if (p_to) *p_to = reinterpret_cast<__typeof(*p_to)>(found_co_object);
			return reinterpret_cast<__typeof(*p_to)>(found_co_object);
			
// 			// ensure a co-object exists
// 			struct found_co_object_group *co_object_group;
// 			void *co_object = find_co_object(
// 				from, REP_ID( from_module ), REP_ID( to_module ),
// 				&found_co_object_rec, -1);
// 			if (!co_object) 
// 			{
// 				/* Need to walk object graph here,
// 				 * firstly to ensure that all objects reachable from the new object
// 				 * are allocated,
// 				 * and secondly to ensure that they are
// 				 * initialized/updated.
// 				 * FIXME: how to ensure that we don't duplicate work from the 
// 				 * sync_all step? Ideally we would allocate before the sync-all.
// 				 * Is that feasible? YES. It all happens in the crossover 
// 				 * environment generation.
// 				 */
// 				// FIRST JOB: make walk_bfs work with DWARF / libprocessimage
// 				walk_bfs (
// 					REP_GTK_12, /* object_rep */ // i.e. key for looking up conversions
// 					arg1, /* object */ // ok
// 					FORM_GDK_WINDOW, /* object_form */ // we don't need this now!
// 					REP_GTK_20, /* co_object_rep */ // i.e. key for looking up conversions
// 					allocate_co_object_idem, /* (*on_blacken)(int, void*, int, int, int) */
// 					REP_GTK_12, /* arg_n_minus_1 */ // 
// 					REP_GTK_20); /* arg_n */ // 
// 				
				/* How is the alloc_co_object going to look up correspondences?
				 * Well,
				 * It's going to use a run-time equivalent of our corresponding_type
				 * template typedef tables. 
				 * It's a table keyed on
				 * <source-component, source-type-identifier, dest-component>
				 * and yielding 
				 * <dest-type-identifier, conversion-function> 
				 * AND (FIXME) must make sure that all conversion functions are
				 * - instantiated, and
				 * - have same/unifiable signatures.
				 *
				 * Thesis says:
				 * - generate a table mapping from pairs of data types...
				 *   ... to the template function instances which perform the
				 *       value conversion
				 * - include in the table all conversions defined between all
				 *   data types related in the Cake file
				 * 
				 * Q. How are we doing object schema discovery these days?
				 * A. By explicit allocation_site annotations.
				 * So object schema discovery is going to give us the DWARF type
				 * as defined in the allocating compilation unit.
				 * We need to canonicalise this to a unique type 
				 * ... at the component (.o / .so) level.
				 * Computing type equivalences can take many seconds.
				 * So this means writing a simple tool that can compute
				 * (and dump to disk) these equivalences.
				 *
				 * In turn, libprocessimage has to be able to find these.
				 * How? 
				 * Use a /usr/lib/debug-like filesystem,
				 * whose prefix is given by an environment variable.
				 * Each executable or shared object is a directory in this filesystem.
				 * Within this, there is one directory per compiler "producer" string.
				 * Under these directories,
				 * the directory tree reproduces the *build* directory structure(s)
				 * in the executable or shared object.
				 * Each DWARF compilation unit 
				 * in the executable / shared object
				 * may have a symlink in this filesystem.
				 * These symlinks point to equivalence databases,
				 * which may reside anywhere under the executable / shared object's
				 * directory. 
				 * The set of symlinks defines the set of compilation units 
				 * up to whose scope the equivalence is complete.
				 * WAIT. We don't need any of this, because our kind of "equivalence"
				 * is also *name*-equivalence.
				 *
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

	/* Q: When do we specialise this *struct template*? 
     *    (Note that we can't specialise the static functions.)
     *    (Note also that both functions are identical!)
     * A: Once per pair of components?
     * (Is the purpose of this class to select RuleTags?)
     * Yes, i.e. it's a mapping from per-component-pair 
     * (Does that mean it doesn't have RuleTags of its own?)
     * -- No, just that its default rule (0) might not map 
     * to value_convert<> rule 0.
     *  */
    template <typename FirstComponentTag, typename SecondComponentTag>
    struct component_pair {
        template <
		    typename To /*= typename cake::corresponding_cxx_type<FirstComponentTag, SecondComponentTag, Arg, 0>::t*/,
            typename From = ::cake::unspecified_wordsize_type, 
            int RuleTag = 0
        >
        static 
        To
        value_convert_from_first_to_second(From arg)
        {
    	    return value_convert<From, 
                To,
				FirstComponentTag,
				SecondComponentTag,
                RuleTag
                >().operator()(arg);
        }
        template <
            typename To /*= typename cake::corresponding_cxx_type<FirstComponentTag, SecondComponentTag, Arg, 0>::t*/,
            typename From = ::cake::unspecified_wordsize_type, 
            int RuleTag = 0
        >
        static 
        To
        value_convert_from_second_to_first(From arg)
        {
    	    return value_convert<From, 
                To,
				SecondComponentTag,
				FirstComponentTag,
                RuleTag
                >().operator()(arg);
        }	
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
