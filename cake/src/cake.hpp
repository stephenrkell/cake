#include <gcj/cni.h>
#include <boost/static_assert.hpp>
#include <boost/type_traits/is_pointer.hpp>
#include <boost/type_traits/remove_pointer.hpp>
#include <java/lang/System.h>
#include <java/lang/String.h>
#include <java/io/File.h>
#include <java/io/FileInputStream.h>
#include <java/lang/ClassCastException.h>
#include <org/antlr/runtime/ANTLRInputStream.h>
#include <org/antlr/runtime/ANTLRStringStream.h>
#include <org/antlr/runtime/CommonTokenStream.h>
#include <org/antlr/runtime/TokenStream.h>
#include <org/antlr/runtime/tree/Tree.h>
#include <org/antlr/runtime/tree/CommonTree.h>
#undef EOF
#include <cakeJavaLexer.h>
#include <cakeJavaParser.h>
#include <cake/SemanticError.h>
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

		/* Parsing apparatus */		
		org::antlr::runtime::ANTLRInputStream *stream;
		::cakeJavaLexer *lexer;
		org::antlr::runtime::CommonTokenStream *tokenStream;
		::cakeJavaParser *parser;
		
		/* AST */
		org::antlr::runtime::tree::CommonTree *ast;
		
		/* */		
		void depthFirst(org::antlr::runtime::tree::Tree *t);
		
		/* */
		void toplevel(org::antlr::runtime::tree::Tree *t);
		
		class module {};
		
		std::map<std::string, module> module_tbl;		
		std::map<std::string, std::vector<std::string> > module_alias_tbl;
		
		/* processing alias declarations */
		void pass1_visit_alias_declaration(org::antlr::runtime::tree::Tree *t);
		
		/* processing exists declarations */
		void add_exists(const char *module_constructor_name,
			const char *filename,
			std::string module_ident);
			
		/* processing derive declarations */
		void add_derive_rewrite(const char *derived_ident,
			std::string filename_text,
			org::antlr::runtime::tree::Tree *derive_body);
		
					
	public:
		request(const char *filename);
		int process();
		
		/*class SemanticError : public ::java::lang::Exception
		{
		public:
			org::antlr::runtime::tree::Tree *t;
			java::lang::String *msg;
			SemanticError(org::antlr::runtime::tree::Tree *t, java::lang::String *msg)
				: t(t), msg(msg) {}
			static ::java::lang::Class class$;
		};*/
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
