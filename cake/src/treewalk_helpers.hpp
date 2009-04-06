#include <sstream>

extern std::ostringstream exception_msg_stream;

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
#define INIT int next_child_to_bind = 0
#define BIND2(node, name) org::antlr::runtime::tree::Tree *(name) = (node)->getChild(next_child_to_bind++);
#define BIND3(node, name, token) org::antlr::runtime::tree::Tree *(name) = (node)->getChild(next_child_to_bind++); \
	if (!((name)->getType() == cakeJavaParser::token)) throw new ::cake::SemanticError((name), \
		JvNewStringUTF(((exception_msg_stream << __FILE__ << ":" << __LINE__ << " expected a token of class " #token), exception_msg_stream.str().c_str()) ));

/* Skip over tokens we're not interested in. */
#define SELECT_NOT(token) if (n->getType() == (cakeJavaParser::token)) continue
#define SELECT_ONLY(token) if (n->getType() != (cakeJavaParser::token)) continue

/* Make a C-style string out of a Java one. */
#define CCP(p) jtocstring_safe((p))

/* Throw a semantic error for token n */
#define SEMANTIC_ERROR(n) throw new ::cake::SemanticError( \
							(n), JvNewStringUTF("Malformed AST: found an unexpected token: ")->concat( \
								(n)->getText()))
