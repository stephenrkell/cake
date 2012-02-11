#ifndef __CAKE_UTIL_HPP
#define __CAKE_UTIL_HPP

//#include <gcj/cni.h>
#include <boost/static_assert.hpp>
#include <boost/type_traits/is_pointer.hpp>
#include <boost/type_traits/remove_pointer.hpp>
#include <boost/regex.hpp>
//#include <java/lang/ClassCastException.h>
//#include <java/lang/String.h>
#include <string>
#include <vector>
#include <deque>
#include <sstream>

#include <dwarfpp/encap.hpp>

#include "parser.hpp"

#include <srk31/algorithm.hpp>

namespace cake
{
	using namespace dwarf;
	using boost::shared_ptr;
	using boost::optional;
	using dwarf::spec::type_die;
	using std::vector;
	using std::string;
	using std::ostringstream;
	using std::pair;
	
	class module_described_by_dwarf;
	typedef shared_ptr<module_described_by_dwarf> module_ptr;

	extern const char *guessed_system_library_path;
	extern const char *guessed_system_library_prefix;
	string new_anon_ident();	
	string new_tmp_filename(string& module_constructor_name);
	extern ostringstream exception_msg_stream;
	string unescape_ident(const string& ident);
	string unescape_string_lit(const string& lit);
	pair<string, string> read_object_constructor(antlr::tree::Tree *t);
	string lookup_solib(string const& basename);
	extern string solib_constructor;

	//typedef std::vector<std::string> definite_member_name;
	class definite_member_name : public dwarf::encap::pathname
	{
		typedef std::allocator<std::string> A;
	public:
		// repeat vector constructors
		explicit definite_member_name(const A& a = A())
			: vector<string>(a) {}
		explicit definite_member_name(size_type n, const string& val = string(), const A& a = A())
			: vector<string>(n, val, a) {}
		template <class In> definite_member_name(In first, In last, const A& a = A())
			: vector<string>(first, last, a) {}
		definite_member_name(const definite_member_name& x)
			: vector<string>(x) {}
		definite_member_name(const vector<string>& x)
			: vector<string>(x) {}
        definite_member_name(antlr::tree::Tree *t);
            
		friend std::ostream& operator<<(std::ostream&, const definite_member_name&);
		operator string () const { std::ostringstream s; s << *this; return s.str(); }
	};
	std::ostream& operator<<(std::ostream&, const definite_member_name&);
	definite_member_name read_definite_member_name(antlr::tree::Tree *memberName);
	antlr::tree::Tree *make_definite_member_name_expr(
		const definite_member_name& arg,
		bool include_header_node = false
		);
	antlr::tree::Tree *make_ident_expr(const string& ident);

	template<typename AntlrReturnedObject>
	antlr::tree::Tree *
	make_ast(
		const string& fragment, 
		AntlrReturnedObject (* cakeCParser::* parserFunction)(cakeCParser_Ctx_struct*)
	);
	
	antlr::tree::Tree *clone_ast(antlr::tree::Tree *t);
	
	antlr::tree::Tree *build_ast(
		antlr::Arboretum *treeFactory,
		int tokenType, const string& text, 
		const std::vector<antlr::tree::Tree *>& children = std::vector<antlr::tree::Tree *>());
	
	string cake_token_text_from_ident(const string& arg);
	bool is_cake_keyword(const string& arg);
	
	string get_event_pattern_call_site_name(antlr::tree::Tree *t);
	antlr::tree::Tree *
	instantiate_definite_member_name_from_pattern_match(
		antlr::tree::Tree *t,
		const boost::smatch& m);
		
    antlr::tree::Tree *make_simple_event_pattern_for_call_site(
    	const string& name);
    
    antlr::tree::Tree *make_simple_sink_expression_for_event_name(
    	const string& event_name);
    antlr::tree::Tree *make_simple_corresp_expression(
    	const vector<string>& ident,
		optional<vector<string>& > rhs_ident = optional<vector<string>& >());
    optional<string> pattern_is_simple_function_name(antlr::tree::Tree *t);
    optional<string> source_pattern_is_simple_function_name(antlr::tree::Tree *t);
    optional<string> sink_expr_is_simple_function_name(antlr::tree::Tree *t);
	
	vector<shared_ptr<type_die> > 
	type_synonymy_chain(shared_ptr<type_die> d);

	shared_ptr<dwarf::spec::basic_die>
	map_ast_context_to_dwarf_element(
		antlr::tree::Tree *node,
		module_ptr dwarf_context,
		bool must_be_immediate
	);
	
	int path_to_node(antlr::tree::Tree *ancestor,
	antlr::tree::Tree *target, std::deque<int>& out);
	
	bool subprogram_returns_void(
		shared_ptr<spec::subprogram_die> subprogram);

	bool treat_subprogram_as_untyped(
		shared_ptr<spec::subprogram_die> subprogram);
		
	bool arg_is_indirect(shared_ptr<spec::formal_parameter_die> p_fp);

	bool
	data_types_are_identical(shared_ptr<type_die> arg1, shared_ptr<type_die> arg2);
	
	boost::regex regex_from_pattern_ast(antlr::tree::Tree *t);
	
	antlr::tree::Tree *
	make_non_ident_pattern_event_corresp(
		bool is_left_to_right,
		const std::string& event_name,
		const boost::smatch& match,
		antlr::tree::Tree *sourcePattern,
		antlr::tree::Tree *sourceInfixStub,
		antlr::tree::Tree *sinkInfixStub,
		antlr::tree::Tree *sinkExpr,
		antlr::tree::Tree *returnEvent
	);

    //optional<definite_member_name> 
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
