//#include <gcj/cni.h>
//#undef EOF
//#include "cake/cakeJavaLexer.h"
//#include "cake/cakeJavaParser.h"
//#include "cake/cakeJavaParser$toplevel_return.h"

// I'd like to do this namespace trick...
// but it interacts badly with the preprocessor:
// any standard headers included within the namespace
// get declared within that namespace and never subsequently
// get redeclared at toplevel.
//namespace antlr
//{

#ifndef __CAKE_PARSER_HPP
#define __CAKE_PARSER_HPP

extern "C" {
#include "antlr3.h"
}
//}
//namespace cake
//{
//using namespace antlr;
extern "C" {
#include "cakeCLexer.h"
#include "cakeCParser.h"
}
//}

namespace antlr { 
	typedef ANTLR3_TOKEN_SOURCE TokenSource;
    typedef ANTLR3_COMMON_TOKEN CommonToken;
    typedef ANTLR3_INPUT_STREAM ANTLRInputStream;
    typedef ANTLR3_COMMON_TOKEN_STREAM CommonTokenStream;
namespace tree { 
	typedef ANTLR3_BASE_TREE Tree; 
    typedef ANTLR3_COMMON_TREE CommonTree;
} }

#include <sstream>

/* Since antlr doesn't provide us with named tree elements, or a convenient way of
 * querying for subtrees (except using tree grammars), let's define some nasty
 * macros which avoid much of the pain. */

/* Evaluate to true, but with a side-effect of performing an assignment. */
#define ASSIGN_AS_COND(name, value) \
	(((name) = (value)) == (name))

/* For-loop over all children of a node, binding some useful variable names
 * globally: childcount
 * per-iteration: i (child index), n (child tree), text (text) */	
#define FOR_ALL_CHILDREN(t) unsigned i = 0; \
	FOR_BODY(t)	

#define FOR_REMAINING_CHILDREN(t) unsigned i = next_child_to_bind; \
	FOR_BODY(t)
	
#define FOR_BODY(t) \
	antlr::tree::Tree *__tree_head_pointer = reinterpret_cast<antlr::tree::Tree *>(t); /* because our tree may well alias 'n' */ \
	unsigned childcount; \
	const char * text; \
	antlr::tree::Tree *n; \
	for (childcount = __tree_head_pointer->getChildCount(__tree_head_pointer), \
		n = ((childcount > 0) ? reinterpret_cast<antlr::tree::Tree*>(__tree_head_pointer->getChild(__tree_head_pointer, 0)) : 0), \
		text = (n != 0) ? reinterpret_cast<char*>(n->getText(n)->chars) : "(null)"; \
	i < childcount && ASSIGN_AS_COND(n, reinterpret_cast<antlr::tree::Tree*>(__tree_head_pointer->getChild(__tree_head_pointer, i))) && \
		ASSIGN_AS_COND(text, (n != 0) ? reinterpret_cast<char*>(n->getText(n)->chars) : "(null)"); \
	i++)

/* Before binding a sequence of children, do INIT. 
 * Don't do INIT more than once in the same scope -- start another scope instead. */
#define INIT int next_child_to_bind __attribute__(( unused )) = 0 
#define BIND2(node, name) antlr::tree::Tree *(name) = reinterpret_cast<antlr::tree::Tree*>((node)->getChild((node), next_child_to_bind++));
#define BIND3(node, name, token) antlr::tree::Tree *(name) = reinterpret_cast<antlr::tree::Tree*>((node)->getChild((node), next_child_to_bind++)); \
	if ((name) == 0) throw cake::SemanticError( \
		(name), \
		"no child node!"); \
	if (!((name)->getType((name)) == token)) throw cake::SemanticError((name), \
		((cake::exception_msg_stream << "expected a token of class " #token \
		" (" __FILE__ ":" << __LINE__ << "); found token " << CCP((name)->getText((name))) \
		<< " class id " << (int) (name)->getType((name))) \
		, exception_msg_stream.str() )); \
	//std::cerr << "DEBUG: " __FILE__ ":" << __LINE__ << " bound a token of type " << (int) ((name)->getType()) << "(" #token ") to name " #name \
	//	<< ", text " << CCP((name)->getText()) << std::endl

/* Skip over tokens we're not interested in. */
#define SELECT_NOT(token) if (n->getType(n) == (token)) continue
#define SELECT_ONLY(token) if (n->getType(n) != (token)) continue

/* Make a C-style string out of a Java one. */
//#define CCP(p) jtocstring_safe((p))
#define CCP(p) (reinterpret_cast<char*>((p->chars)))

/* Throw a semantic error for token n */
#define SEMANTIC_ERROR(n) throw cake::SemanticError( \
							(n), std::string("Malformed AST: found an unexpected token: ")+std::string(CCP((n)->getText((n)))))

#define ALIAS2(node, name) antlr::tree::Tree *& name = (node)
#define ALIAS3(node, name, token) \
	antlr::tree::Tree *& name = (node); \
	if (!((name)->getType((name)) == token)) throw cake::SemanticError((name), \
	((cake::exception_msg_stream << "expected a token of class " #token \
	" (" __FILE__ ":" << __LINE__ << "); found token " << CCP((name)->getText((name))) \
	<< " class id " << (int) (name)->getType(name)) \
	, exception_msg_stream.str() ));

#define RAISE(node, msg) throw cake::SemanticError((node), std::string( \
					msg ": ") \
						+ std::string(CCP((node)->getText(node))))

#define RAISE_INTERNAL(node, msg) throw cake::InternalError((node), std::string( \
					msg ": ") \
						+std::string(CCP((node)->getText(node))))

namespace cake {
	class TreewalkError
    {
    	antlr::tree::Tree *m_t;
        std::string m_msg;
    public:
    	TreewalkError(antlr::tree::Tree *t, std::string msg) : m_t(t), m_msg(msg) {}
        std::string message() {
        	std::ostringstream s;
            s << "TreewalkError";
            if (m_t != 0) 
            {
            	antlr::CommonToken *tok = m_t->getToken(m_t);
            	s << " at input position "
                	<< tok->getLine(tok)
                    << ":" 
                    << tok->getCharPositionInLine(tok);
            }
            s << ": " << m_msg;
            return s.str();
        }
    };
	class SemanticError
    {
    	antlr::tree::Tree *m_t;
        std::string m_msg;
    public:
    	SemanticError(antlr::tree::Tree *t, std::string msg) : m_t(t), m_msg(msg) {}
        std::string message() {
        	std::ostringstream s;
            s << "SemanticError";
            if (m_t != 0) 
            {
            	antlr::CommonToken *tok = m_t->getToken(m_t);
            	s << " at input position "
                	<< tok->getLine(tok)
                    << ":" 
                    << tok->getCharPositionInLine(tok);
            }
            s << ": " << m_msg;
            return s.str();
        }
    };
	class InternalError
    {
    	antlr::tree::Tree *m_t;
        std::string m_msg;
    public:
    	InternalError(antlr::tree::Tree *t, std::string msg) : m_t(t), m_msg(msg) {}
        std::string message() {
        	std::ostringstream s;
            s << "InternalError";
            if (m_t != 0) 
            {
            	antlr::CommonToken *tok = m_t->getToken(m_t);
            	s << " at input position "
                	<< tok->getLine(tok)
                    << ":" 
                    << tok->getCharPositionInLine(tok);
            }
            s << ": " << m_msg;
            return s.str();
        }
    };


}

#endif
