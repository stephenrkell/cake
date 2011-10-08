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

#include <srk31/algorithm.hpp>

namespace cake
{
	using boost::shared_ptr;
	using dwarf::spec::type_die;
	using std::vector;

	extern const char *guessed_system_library_path;
	extern const char *guessed_system_library_prefix;
	std::string new_anon_ident();	
	std::string new_tmp_filename(std::string& module_constructor_name);
	extern std::ostringstream exception_msg_stream;
	std::string unescape_ident(const std::string& ident);
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
		operator std::string () const { std::ostringstream s; s << *this; return s.str(); }
	};
	std::ostream& operator<<(std::ostream&, const definite_member_name&);
	definite_member_name read_definite_member_name(antlr::tree::Tree *memberName);
	antlr::tree::Tree *make_definite_member_name_expr(const definite_member_name& arg);
	antlr::tree::Tree *make_ident_expr(const std::string& ident);
	std::string cake_token_text_from_ident(const std::string& arg);
	bool is_cake_keyword(const std::string& arg);
	
	std::string get_event_pattern_call_site_name(antlr::tree::Tree *t);
    
    antlr::tree::Tree *make_simple_event_pattern_for_call_site(
    	const std::string& name);
    
    antlr::tree::Tree *make_simple_sink_expression_for_event_name(
    	const std::string& event_name);
    antlr::tree::Tree *make_simple_corresp_expression(
    	const std::vector<std::string>& ident,
		boost::optional<std::vector<std::string>& > rhs_ident = boost::optional<std::vector<std::string>& >());
    boost::optional<std::string> pattern_is_simple_function_name(antlr::tree::Tree *t);
    boost::optional<std::string> source_pattern_is_simple_function_name(antlr::tree::Tree *t);
    boost::optional<std::string> sink_expr_is_simple_function_name(antlr::tree::Tree *t);
	
	vector<shared_ptr<type_die> > 
	type_synonymy_chain(shared_ptr<type_die> d);

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
using srk31::array_len;
using srk31::copy_if;
}

#endif
