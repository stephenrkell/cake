// #include <gcj/cni.h>
// #include <org/antlr/runtime/tree/Tree.h>
// #include <org/antlr/runtime/tree/CommonTree.h>
// #include <cake/cakeJavaLexer.h>
// #include <cake/cakeJavaParser.h>
// #include <cake/SemanticError.h>
#include <fstream>
#include <vector>
#include <map>
#include "request.hpp"
#include "util.hpp"
#include "module.hpp"
#include <dwarfpp/encap.hpp>
// #include <boost/filesystem.hpp> // removed use of boost::filesystem while boost is C++11-broken (depends on deleted copy constructors)
#include <memory>
#include <stdio.h>
#include <ext/stdio_filebuf.h>
#include <libgen.h> // for dirname

namespace cake
{
	void request::add_exists(antlr::tree::Tree *n)
	{
		INIT;
		BIND2(n, objectSpec);
		BIND3(n, existsBody, EXISTS_BODY);

		/* with children of objectSpec */
		{
			INIT;
			switch (GET_TYPE(objectSpec))
			{
				case OBJECT_SPEC_DIRECT: {
					BIND3(objectSpec, objectConstructor, OBJECT_CONSTRUCTOR);
					BIND3(objectSpec, id, IDENT);
					std::pair<std::string, std::string> ocStrings =
						read_object_constructor(objectConstructor);
					std::string ident(CCP(GET_TEXT(id)));							
					add_existing_module(ocStrings.first,
						ocStrings.second,
						ident);
					// now process the claim group
					module_tbl[ident]->process_exists_claims(existsBody);
					} break;
				case OBJECT_SPEC_DERIVING: {
					BIND3(objectSpec, existingObjectConstructor, OBJECT_CONSTRUCTOR);
					BIND3(objectSpec, derivedObjectConstructor, OBJECT_CONSTRUCTOR);
					std::pair<std::string, std::string> eocStrings =
						read_object_constructor(existingObjectConstructor);
					std::pair<std::string, std::string> docStrings =
						read_object_constructor(derivedObjectConstructor);
					BIND3(objectSpec, derivedIdent, IDENT);

					/* Add the raw exists to the database first... */ 
					std::string anon = new_anon_ident();
					add_existing_module(eocStrings.first,
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

	module_ptr request::create_existing_module(std::string& constructor,
		std::string& filename)
	{
		std::string unescaped_filename = unescape_string_lit(filename);
		// #define CASE(s, f, ...) (constructor == #s ) ? (std::make_shared<s ## _module>((f) , ##__VA_ARGS__))
		if       (constructor == "elf_external_sharedlib") return std::make_shared<elf_external_sharedlib_module>(lookup_solib(unescaped_filename), unescaped_filename);
		else if  (constructor == "elf_reloc") return              std::make_shared<elf_reloc_module>(make_absolute_pathname(unescaped_filename), unescaped_filename);
		return module_ptr();
		// #undef CASE
	}

	void request::add_existing_module(std::string& constructor,
		std::string& filename,
		std::string& module_ident) 
	{
		std::cerr << "Told that exists a module, constructor name " << constructor
			<< " with quoted filename " << filename 
			<< " and module identifier " << module_ident << std::endl;
			
		/* Add a module to the module table. */
		module_tbl[module_ident] = create_existing_module(constructor, filename);
		module_inverse_tbl[module_tbl[module_ident]] = module_ident;
	}	
	
	std::string request::make_absolute_pathname(std::string ref)
	{
		//boost::filesystem::path p(ref);
		//if (!p.root_directory().empty())
		if (ref.length() > 0 && ref.at(0) == '/')
		{
			// this means p is absolute, so return it
			return ref;
		}
		else
		{
			// this means p is relative, so prepend the Cake file's dirname
			
			//return (boost::filesystem::path(in_filename).branch_path() 
			//	/ boost::filesystem::path(ref)).string();
			char *tmp = strdup(in_filename);
			string ret = string(dirname(tmp)) + "/" + ref;
			free(tmp);
			return ret;
		}
	}
}

