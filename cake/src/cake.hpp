#include <gcj/cni.h>
#include <boost/static_assert.hpp>
#include <boost/type_traits/is_pointer.hpp>
#include <boost/type_traits/remove_pointer.hpp>
#include <java/lang/System.h>
#include <java/lang/String.h>
#include <java/io/File.h>
#include <java/io/FileInputStream.h>
#include <java/lang/ClassCastException.h>
#include <org/antlr/runtime/tree/Tree.h>
#include <org/antlr/runtime/tree/CommonTree.h>
#include <vector>
#include <map>

namespace cake
{
	class request
	{
		/* Source file */
		jstring in_filename;
		java::io::File *in_fileobj;
		java::io::FileInputStream *in_file;
		
		/* AST */
		org::antlr::runtime::tree::CommonTree *ast;
		
		/* */
		
		void depthFirst(org::antlr::runtime::tree::Tree *t);
		void toplevel(org::antlr::runtime::tree::Tree *t);
		
		class module {};
		
		std::map<std::string, module> module_tbl;		
		std::map<std::string, std::vector<std::string> > module_alias_tbl;
		
		void pass1_visit_alias_declaration(org::antlr::runtime::tree::Tree *t);
		
		void populate_alias_list(std::vector<std::string>& list,
			org::antlr::runtime::tree::Tree *t);
			
	public:
		request(const char *filename);
		int process();
	};
	
	const char *token_name(jint t);
}

inline const char *jtocstring(java::lang::String *s)
{
	static char *buf;
	static jsize buf_len;
	
	if (buf == 0 || (jsize) s->length() < buf_len)
	{
		if (buf != 0) delete[] buf;
		buf = new char[s->length() + 1];
	}
	jsize len = JvGetStringUTFRegion(s, 0, (jsize) s->length(), buf);
	buf[len] = '\0';
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

        if (::boost::remove_pointer<T>::type::class$.isAssignableFrom (o->getClass ()))
                return reinterpret_cast<T>(o);
        else
                throw new java::lang::ClassCastException;
}

/*
template<class T>
inline T *jcast (java::lang::Object *o)
{
        if (T::class$.isAssignableFrom (o->getClass ()))
                return reinterpret_cast<T*>(o);
        else
                throw new java::lang::ClassCastException;
}
*/

