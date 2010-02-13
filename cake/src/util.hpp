#ifndef __CAKE_UTIL_HPP
#define __CAKE_UTIL_HPP

//#include <gcj/cni.h>
#include <boost/static_assert.hpp>
#include <boost/type_traits/is_pointer.hpp>
#include <boost/type_traits/remove_pointer.hpp>
//#include <java/lang/ClassCastException.h>
//#include <java/lang/String.h>
#include <string>
#include <vector>
#include <sstream>

#include <dwarfpp/encap.hpp>

#include "parser.hpp"

namespace cake
{
	extern const char *guessed_system_library_path;
	extern const char *guessed_system_library_prefix;
	std::string new_anon_ident();	
	std::string new_tmp_filename(std::string& module_constructor_name);
	extern std::ostringstream exception_msg_stream;
	std::string unescape_ident(std::string& ident);
	std::string unescape_string_lit(std::string& lit);
	std::pair<std::string, std::string> read_object_constructor(antlr::tree::Tree *t);
	std::string lookup_solib(std::string const& basename);
	extern std::string solib_constructor;

	//typedef std::vector<std::string> definite_member_name;
	class definite_member_name : public dwarf::encap::pathname
	{
		typedef std::allocator<std::string> A;
	public:
		// repeat vector constructors
		explicit definite_member_name(const A& a = A())
			: std::vector<std::string>(a) {}
		explicit definite_member_name(size_type n, const std::string& val = std::string(), const A& a = A())
			: std::vector<std::string>(n, val, a) {}
		template <class In> definite_member_name(In first, In last, const A& a = A())
			: std::vector<std::string>(first, last, a) {}
		definite_member_name(const definite_member_name& x)
			: std::vector<std::string>(x) {}
		definite_member_name(const std::vector<std::string>& x)
			: std::vector<std::string>(x) {}
        definite_member_name(antlr::tree::Tree *t);
            
		friend std::ostream& operator<<(std::ostream&, const definite_member_name&);
	};
	std::ostream& operator<<(std::ostream&, const definite_member_name&);
	definite_member_name read_definite_member_name(antlr::tree::Tree *memberName);

	std::string get_event_pattern_call_site_name(antlr::tree::Tree *t);
    
    antlr::tree::Tree *make_simple_event_pattern_for_call_site(
    	const std::string& name);
    
    antlr::tree::Tree *make_simple_sink_expression_for_event_pattern(
    	const std::string& event_pattern);
        
    boost::optional<std::string> pattern_is_simple_function_name(antlr::tree::Tree *t);
    boost::optional<std::string> source_pattern_is_simple_function_name(antlr::tree::Tree *t);
    boost::optional<std::string> sink_expr_is_simple_function_name(antlr::tree::Tree *t);

    //boost::optional<definite_member_name> 
    //dwarf_fq_member_name(dwarf::abstract::Die_abstract_base<>& d);
        
// 	inline const char *jtocstring(java::lang::String *s)	
// 	{
// 		static char *buf;
// 		static jsize buf_len;
// 		jsize string_len = JvGetStringUTFLength(s);
// 
// 		if (buf == 0 || string_len >= buf_len)
// 		{
// 			if (buf != 0) delete[] buf;
// 			buf_len = string_len + 1;
// 			buf = new char[buf_len];
// 		}
// 		JvGetStringUTFRegion(s, 0, string_len, buf);
// 		buf[string_len] = '\0';
// 		return buf;
// 	}
// 
// 	inline const char *jtocstring_safe(java::lang::String *s)
// 	{
// 		if (s == 0) return "(null)";
// 		return jtocstring(s);
// 	}
// 	
// 	/* Gratefully stolen from Waba
// 	 * http://www.waba.be/page/java-integration-through-cni-1.xhtml */
// 
// 	template<class T>
// 	inline T jcast (java::lang::Object *o)
// 	{  
//         	BOOST_STATIC_ASSERT ((::boost::is_pointer<T>::value));
// 			if (o == 0) return 0;
//         	if (::boost::remove_pointer<T>::type::class$.isAssignableFrom (o->getClass ()))
//                 	return reinterpret_cast<T>(o);
//         	else
//                 	throw new java::lang::ClassCastException;
// 	}
// 	
 	template<class T, size_t s> size_t array_len(T (&arg)[s]) { return s; }
	
	/* copy_if should be in the standard algorithms, but it isn't. */
	template<class In, class Out, class Pred>
	Out copy_if(In first, In last, Out res, Pred p)
	{
		while (first != last)
		{
			if (p(*first)) *res++ = *first;
			++first;
		}
		return res;
	}	
}

#endif
