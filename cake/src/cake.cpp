#include <string>
#include <iostream>
#include <cassert>
#include <cstring>
#include <java/lang/Exception.h>
#include <java/lang/System.h>
#include <java/io/PrintStream.h>
#include <org/antlr/runtime/tree/BaseTree.h>
#include <org/antlr/runtime/tree/CommonTree.h>
#undef EOF
#include <org/antlr/runtime/CharStream.h>
#include <org/antlr/runtime/IntStream.h>
#include "cake.hpp"
#include "parser.hpp"
#include "util.hpp"
#include "treewalk_helpers.hpp"

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
		stream = new ANTLRInputStream(in_file);
		lexer = new ::cakeJavaLexer((CharStream*) stream);
        tokenStream = new CommonTokenStream((TokenSource*) lexer);
        parser = new ::cakeJavaParser((TokenStream*) tokenStream);
        tree::CommonTree *tree = jcast<tree::CommonTree*>(parser->toplevel()->getTree());
		toplevel((tree::Tree*) tree);			
		
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

	
	/* FIXME: what's the right way to do this? tree parsers? Visitors? hand-crafted? */
	void request::toplevel(org::antlr::runtime::tree::Tree *t)
	{	
		{	/* Process aliases */
			INIT;
			FOR_ALL_CHILDREN(t)
			{	/* Find all the toplevel alias definitions */
				SELECT_ONLY(KEYWORD_ALIAS);
				/* Create an alias record for this alias. */
				module_alias_tbl.insert(std::make_pair(
 					std::string(text),
 					std::vector<std::string>()
 					)
 				);
				/* Process the alias definition body. */
				pass1_visit_alias_declaration(n);
			}
			/* FIXME: check for cycles in the alias graph */
		}
		
		{	/* Process 'exists' */
			INIT;
			FOR_ALL_CHILDREN(t)
			{	/* Find all toplevel exists definition */
				SELECT_ONLY(KEYWORD_EXISTS);
				INIT;
				BIND3(n, objectSpec, IDENT);
				BIND2(n, existsBody);

				/* with children of objectSpec */
				{
					INIT;
					BIND3(objectSpec, module_constructor_name, IDENT);
					BIND3(objectSpec, filename, STRING_LIT);
						/* Absence of filename is allowed by the gramamr,
						 * but it's *not* okay here. */
					BIND2(objectSpec, deriving_or_ident);
					
					switch(deriving_or_ident->getType())
					{
						case cakeJavaParser::KEYWORD_DERIVING: {
							/* Add the raw exists to the database first, then 
							 * deal with the derive. */
							std::string anon = new_anon_ident();
							add_exists(CCP(module_constructor_name->getText()),
								CCP(filename->getText()),
								anon);
								
							/* Now bind the remaining siblings to create the derive. */ 
							BIND3(objectSpec, derived_constructor_ident, IDENT);
							BIND2(objectSpec, lit_or_ident);
							std::string derived_filename_text;
							switch (lit_or_ident->getType())
							{
								case cakeJavaParser::STRING_LIT: {
									BIND3(objectSpec, derived_filename, STRING_LIT);
									derived_filename_text = CCP(derived_filename->getText());
									BIND3(objectSpec, derived_ident, IDENT);
									add_derive_rewrite(
										CCP(derived_ident->getText()), 
										derived_filename_text, 
										existsBody);
								} break;
								case cakeJavaParser::IDENT: {
									BIND3(objectSpec, derived_ident, IDENT);
									derived_filename_text = new_tmp_filename(
										jtocstring_safe(derived_constructor_ident->getText()));
									add_derive_rewrite(CCP(derived_ident->getText()), 
										derived_filename_text, 
										existsBody);
								} break;
								default: SEMANTIC_ERROR(lit_or_ident);								
							}
						} break;
						case cakeJavaParser::IDENT: {
							add_exists(CCP(module_constructor_name->getText()),
								CCP(filename->getText()),
								std::string(CCP(deriving_or_ident->getText())));						
						} break;
						default: SEMANTIC_ERROR(n);
					}
				}				
				/* Create an exists record for this definition.
				 * If it also contains a 'deriving', create a vanilla
				 * 'exists' with a new name, then create the 'derive'
				 * block separately. */
				//exists_tbl.insert(std::make_pair(
					
			
			}
		}
	}
}
