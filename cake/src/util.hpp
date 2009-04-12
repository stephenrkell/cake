#include <gcj/cni.h>
#include <boost/static_assert.hpp>
#include <boost/type_traits/is_pointer.hpp>
#include <boost/type_traits/remove_pointer.hpp>
#include <java/lang/ClassCastException.h>
#include <java/lang/String.h>
#include <string>
#include <vector>
#include <sstream>

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

	typedef std::vector<std::string> definite_member_name;
	definite_member_name read_definite_member_name(antlr::tree::Tree *memberName);

	inline const char *jtocstring(java::lang::String *s)	
	{
		static char *buf;
		static jsize buf_len;
		jsize string_len = JvGetStringUTFLength(s);

		if (buf == 0 || string_len >= buf_len)
		{
			if (buf != 0) delete[] buf;
			buf_len = string_len + 1;
			buf = new char[buf_len];
		}
		JvGetStringUTFRegion(s, 0, string_len, buf);
		buf[string_len] = '\0';
		return buf;
	}

	inline const char *jtocstring_safe(java::lang::String *s)
	{
		if (s == 0) return "(null)";
		return jtocstring(s);
	}
	
	/* Gratefully stolen from Waba
	 * http://www.waba.be/page/java-integration-through-cni-1.xhtml */

	template<class T>
	inline T jcast (java::lang::Object *o)
	{  
        	BOOST_STATIC_ASSERT ((::boost::is_pointer<T>::value));
			if (o == 0) return 0;
        	if (::boost::remove_pointer<T>::type::class$.isAssignableFrom (o->getClass ()))
                	return reinterpret_cast<T>(o);
        	else
                	throw new java::lang::ClassCastException;
	}
}
