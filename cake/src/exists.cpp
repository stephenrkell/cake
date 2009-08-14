#include <gcj/cni.h>
#include <org/antlr/runtime/tree/Tree.h>
#include <org/antlr/runtime/tree/CommonTree.h>
#include <cakeJavaLexer.h>
#include <cakeJavaParser.h>
#include <cake/SemanticError.h>
#include <fstream>
#include <vector>
#include <map>
#include "cake.hpp"
#include "util.hpp"
#include "dwarfpp.h"
#include "dwarfpp_simple.hpp"
#include "treewalk_helpers.hpp"
//#include <fileno.hpp>
#include <stdio.h>
#include <ext/stdio_filebuf.h>

namespace cake
{
	void request::extract_exists()
	{	
		antlr::tree::Tree *t = (antlr::tree::Tree *) ast;
		/* Process 'exists' */
		INIT;
		FOR_ALL_CHILDREN(t)
		{	/* Find all toplevel exists definition */
			SELECT_ONLY(KEYWORD_EXISTS);
			INIT;
			BIND2(n, objectSpec);
			BIND3(n, existsBody, EXISTS_BODY);

			/* with children of objectSpec */
			{
				INIT;
				switch (objectSpec->getType())
				{
					case cakeJavaParser::OBJECT_SPEC_DIRECT: {
						BIND3(objectSpec, objectConstructor, OBJECT_CONSTRUCTOR);
						BIND3(objectSpec, id, IDENT);
						std::pair<std::string, std::string> ocStrings =
							read_object_constructor(objectConstructor);
						std::string ident(CCP(id->getText()));							
						add_exists(ocStrings.first,
							ocStrings.second,
							ident);
						// now process the claim group
						module_tbl[ident]->process_exists_claims(existsBody);
						} break;
					case cakeJavaParser::OBJECT_SPEC_DERIVING: {
						BIND3(objectSpec, existingObjectConstructor, OBJECT_CONSTRUCTOR);
						BIND3(objectSpec, derivedObjectConstructor, OBJECT_CONSTRUCTOR);
						std::pair<std::string, std::string> eocStrings =
							read_object_constructor(existingObjectConstructor);
						std::pair<std::string, std::string> docStrings =
							read_object_constructor(derivedObjectConstructor);
						BIND3(objectSpec, derivedIdent, IDENT);

						/* Add the raw exists to the database first... */ 
						std::string anon = new_anon_ident();
						add_exists(eocStrings.first,
							eocStrings.second,
							anon);
						// now process any claims about that object
						// FIXME: do claims get processed *before* or *after* rewrites? before.
						module_tbl[anon]->process_exists_claims(existsBody);
						/* ... then deal with the derive. */
						std::string filename = 
							docStrings.second.empty() ? 
								new_tmp_filename(docStrings.first) 
								: docStrings.second;
						add_derive_rewrite(docStrings.first, 
							filename, 
							existsBody);							
						} break;
					default: SEMANTIC_ERROR(n);
				}
			}			
		}
	}


	void request::add_exists(std::string& constructor,
		std::string& filename,
		std::string& module_ident) 
	{
		std::cout << "Told that exists a module, constructor name " << constructor
			<< " with quoted filename " << filename 
			<< " and module identifier " << module_ident << std::endl;
			
		/* Add a module to the module table. */
		#define CASE(s, f) (constructor == #s ) ? static_cast<elf_module*>(new s ## _module((f)))
		module_tbl[module_ident] = boost::shared_ptr<module>(
			CASE(elf_external_sharedlib, lookup_solib(unescape_string_lit(filename)))
		:	CASE(elf_reloc, unescape_string_lit(filename))
		:	0);
		#undef CASE
		// the code above appears to be exception-safe, because the only possibly-throwing thing
		// we do, no matter which arm of the conditional we take, is the new() -- we don't have to
		// worry about interleaving of different (sub)expressions.

		/* Now process overrides and so on */
	}	
}

