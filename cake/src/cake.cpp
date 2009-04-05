#include <string>
#include <iostream>
#include <cassert>
#include <java/lang/Exception.h>
#include <java/lang/System.h>
#include <java/io/PrintStream.h>
#include <org/antlr/runtime/ANTLRInputStream.h>
#include <org/antlr/runtime/ANTLRStringStream.h>
#include <org/antlr/runtime/CommonTokenStream.h>
#include <org/antlr/runtime/TokenStream.h>
#include <org/antlr/runtime/tree/BaseTree.h>
#include <org/antlr/runtime/tree/CommonTree.h>
#undef EOF
#include <org/antlr/runtime/CharStream.h>
#include <org/antlr/runtime/IntStream.h>
#include "cake.hpp"
#include "parser.hpp"

namespace cake
{
	request::request(const char *filename)
		: in_filename(JvNewStringUTF(filename)), 
		  in_fileobj(new java::io::File(in_filename)), 
		  in_file(new java::io::FileInputStream(in_fileobj))/*,
		  ast(build_ast(in_file))/*,
		  alias(*/
	{
		
	}
	
	int request::process()
	{
		using namespace org::antlr::runtime;
		using namespace java::lang;
		// invoke the parser
		try
		{
			ANTLRInputStream *stream = new ANTLRInputStream(in_file);
			::cakeJavaLexer *lexer = new ::cakeJavaLexer((CharStream*) stream);
        	CommonTokenStream *tokenStream = new CommonTokenStream((TokenSource*) lexer);
        	::cakeJavaParser *parser = new ::cakeJavaParser((TokenStream*) tokenStream);
        	tree::CommonTree *tree = jcast<tree::CommonTree*>(parser->toplevel()->getTree());
			System::out->print(JvNewStringUTF("Beginning depth-first traversal of AST:\n"));
			toplevel((tree::Tree*) tree);			
		}
		catch (Exception *ex)
		{
			System::out->print(ex->toString());
		}
		
		return 0;
	}
	
	void request::depthFirst(org::antlr::runtime::tree::Tree *t)
	{
		jint childCount = t->getChildCount();
		std::cout 	<< "found a node with " << (int) childCount << " children" << ", "
				 	<< "type: " << (int) t->getType() << std::endl;
		for (jint i = 0; i < t->getChildCount(); i++)
		{
			depthFirst(t->getChild(i));
		}
		//std::wcout 	<< "node: token: " /*<< JvGetStringChars(t->getToken->getText())*/ << ", " 
		//			<< "type: " /*<< t->getToken->getType()*/ << std::endl;
		std::cout 	<< "text: " << jtocstring_safe (t->getText()) << std::endl;
		
	}
	
#define ASSIGN_AS_COND(name, value) \
	(((name) = (value)) == (name))
	
#define FOR_ALL_CHILDREN(t) jint i; jint childcount; \
	const char *text; org::antlr::runtime::tree::Tree *n; \
	for (i = 0, childcount = (t)->getChildCount(), \
		n = ((childcount > 0) ? (t)->getChild(0) : 0), \
		text = (n != 0) ? jtocstring_safe(n->getText()) : "(null)"; \
	i < childcount && ASSIGN_AS_COND(n, (t)->getChild(i)) && \
		ASSIGN_AS_COND(text, (n != 0) ? jtocstring_safe(n->getText()) : "(null)"); \
	i++)

#define INIT int next_child_to_bind = 0
#define BIND1(name) org::antlr::runtime::tree::Tree *(name) = t->getChild(next_child_to_bind++);
#define BIND2(name, token) org::antlr::runtime::tree::Tree *(name) = t->getChild(next_child_to_bind++); \
	assert((name)->getType() == cakeJavaParser::token)

#define SELECT_NOT(token) if (n->getType() == (cakeJavaParser::token)) continue
#define SELECT_ONLY(token) if (n->getType() != (cakeJavaParser::token)) continue

#define CCP(p) jtocstring_safe((p))
	
	/* FIXME: what's the right way to do this? tree parsers? Visitors? hand-crafted? */
	void request::toplevel(org::antlr::runtime::tree::Tree *t)
	{	
		INIT;
		FOR_ALL_CHILDREN(t)
		{
			SELECT_ONLY(KEYWORD_ALIAS);
			module_alias_tbl.insert(std::make_pair(
 				std::string(text),
 				std::vector<std::string>()
 				)
 			);
			pass1_visit_alias_declaration(n);

// 			assert(n->getChildCount() == 2); /* aliasDescription, IDENT */				
// 			module_alias_tbl.insert(std::make_pair(
// 				std::string(text),
// 				std::vector<std::string>()
// 				)
// 			);
// 			populate_alias_list(module_alias_tbl[text], n->getChild(1));
		}
// 		switch (tok)
// 		{
// 			case cakeJavaParser::KEYWORD_EXISTS:
// 				std::cout << "Found an 'exists' block." << std::endl;
// 
// 				break;
// 			default:
// 				std::cout << "Ignoring unknown token " << token_name(tok) << "." << std::endl;
// 				break; 
// 
// 
// 			/*case cakeParser::*/
// 
// 		}
	}
	
	void request::pass1_visit_alias_declaration(org::antlr::runtime::tree::Tree *t)
	{
		INIT;
		BIND1(aliasDescription);
		BIND2(aliasName, IDENT);
		{ FOR_ALL_CHILDREN(aliasDescription)
		{
			SELECT_ONLY(KEYWORD_ANY);
			{ INIT; FOR_ALL_CHILDREN(n)
			{
				BIND2(ident, IDENT);
				module_alias_tbl[CCP(aliasName->getText())].push_back(CCP(ident->getText()));			
			} }
			return;
		} }
		

		{ FOR_ALL_CHILDREN(aliasDescription)
		{
			SELECT_ONLY(IDENT);
			module_alias_tbl[CCP(aliasName->getText())].push_back(CCP(n->getText()));
			return;
		} }		
	}
	
	void request::populate_alias_list(std::vector<std::string>& list,
		org::antlr::runtime::tree::Tree *t)
	{
		switch (t->getType())
		{
			case cakeJavaParser::KEYWORD_ANY:
				assert(t->getChildCount() == 1); /* identList */
				
				for (int i = 0; i < t->getChild(0)->getChildCount(); i++)
				{
					list.push_back(jtocstring_safe(t->getChild(0)->getChild(i)->getText()));
				}
				break;
			case cakeJavaParser::IDENT:
				list.push_back(jtocstring_safe(t->getText()));
				break;
			default:
				assert(false);
				break;
		}		
	}	
}
