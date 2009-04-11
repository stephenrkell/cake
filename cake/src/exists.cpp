#include <gcj/cni.h>
#include <org/antlr/runtime/tree/Tree.h>
#include <org/antlr/runtime/tree/CommonTree.h>
#include <cakeJavaLexer.h>
#include <cakeJavaParser.h>
#include <cake/SemanticError.h>
#include <fstream>
#include <vector>
#include <map>
#undef EOF
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
			BIND2(n, existsBody);

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
		
// 		//std::ifstream file(unescaped_filename.c_str());
// 		FILE *c_file = fopen(unescaped_filename.c_str(), "r");
// 		if (c_file == NULL) throw new SemanticError(0, JvNewStringUTF("file does not exist! ")->concat(JvNewStringUTF(filename.c_str())));
// 		__gnu_cxx::stdio_filebuf<char> file(c_file, std::ios::in);
// 		int fd = fileno(c_file);
// 		//int fd = fileno(file);
// 		if (fd == -1) throw new SemanticError(0, JvNewStringUTF("file does not exist! ")->concat(JvNewStringUTF(filename.c_str())));
// 		dwarf::file f(fd);
// 		//dwarf::file f(fileno(file));
// 		dwarf::abi_information info(f);		
		
		//print_abi_info(info, module_tbl[module_ident]->get_filename());
		
		/* Now process overrides and so on */
	}	
}

