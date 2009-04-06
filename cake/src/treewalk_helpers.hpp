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
#define FOR_ALL_CHILDREN(t) jint i; jint childcount; \
	const char *text; org::antlr::runtime::tree::Tree *n; \
	for (i = 0, childcount = (t)->getChildCount(), \
		n = ((childcount > 0) ? (t)->getChild(0) : 0), \
		text = (n != 0) ? jtocstring_safe(n->getText()) : "(null)"; \
	i < childcount && ASSIGN_AS_COND(n, (t)->getChild(i)) && \
		ASSIGN_AS_COND(text, (n != 0) ? jtocstring_safe(n->getText()) : "(null)"); \
	i++)

/* Before binding a sequence of children, do INIT. 
 * Don't do INIT more than once in the same scope -- start another scope instead. */
#define INIT int next_child_to_bind __attribute__(( unused )) = 0 
#define BIND2(node, name) org::antlr::runtime::tree::Tree *(name) = (node)->getChild(next_child_to_bind++);
#define BIND3(node, name, token) org::antlr::runtime::tree::Tree *(name) = (node)->getChild(next_child_to_bind++); \
	if ((name) == 0) throw new ::cake::SemanticError( \
		(name), \
		JvNewStringUTF("no child node!")); \
	if (!((name)->getType() == cakeJavaParser::token)) throw new ::cake::SemanticError((name), \
		JvNewStringUTF(((cake::exception_msg_stream << "expected a token of class " #token \
		" (" __FILE__ ":" << __LINE__ << "); found token " << CCP((name)->getText()) \
		<< " class id " << (int) (name)->getType()) \
		, exception_msg_stream.str().c_str()) )); \
	std::cerr << "DEBUG: " __FILE__ ":" << __LINE__ << " bound a token of type " << (int) ((name)->getType()) << "(" #token ") to name " #name \
		<< ", text " << CCP((name)->getText()) << std::endl

/* Skip over tokens we're not interested in. */
#define SELECT_NOT(token) if (n->getType() == (cakeJavaParser::token)) continue
#define SELECT_ONLY(token) if (n->getType() != (cakeJavaParser::token)) continue

/* Make a C-style string out of a Java one. */
#define CCP(p) jtocstring_safe((p))

/* Throw a semantic error for token n */
#define SEMANTIC_ERROR(n) throw new ::cake::SemanticError( \
							(n), JvNewStringUTF("Malformed AST: found an unexpected token: ")->concat( \
								(n)->getText()))

#define ALIAS2(node, name) org::antlr::runtime::tree::Tree *& name = (node)
#define ALIAS3(node, name, token) \
	org::antlr::runtime::tree::Tree *& name = (node); \
	if (!((name)->getType() == cakeJavaParser::token)) throw new ::cake::SemanticError((name), \
	JvNewStringUTF(((cake::exception_msg_stream << "expected a token of class " #token \
	" (" __FILE__ ":" << __LINE__ << "); found token " << CCP((name)->getText()) \
	<< " class id " << (int) (name)->getType()) \
	, exception_msg_stream.str().c_str()) ));
