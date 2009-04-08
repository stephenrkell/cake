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
//#include <fileno.hpp>
#include <stdio.h>
#include <ext/stdio_filebuf.h>

namespace cake
{
	void request::add_exists(std::string& module_constructor_name,
		std::string& filename,
		std::string& module_ident) 
	{
		std::cout << "Told that exists a module, constructor name " << module_constructor_name
			<< " with quoted filename " << filename 
			<< " and module identifier " << module_ident << std::endl;
			
		/* Open the file and examine its DWARF information. */
		std::string s;
		std::string unescaped_filename = unescape_string_lit(filename);
		//std::ifstream file(unescapeed_filename.c_str());
		FILE *c_file = fopen(unescaped_filename.c_str(), "r");
		if (c_file == NULL) throw new ::cake::SemanticError(0, JvNewStringUTF("file does not exist! ")->concat(JvNewStringUTF(filename.c_str())));
		__gnu_cxx::stdio_filebuf<char> file(c_file, std::ios::in);
		int fd = fileno(c_file);
		//int fd = fileno(file);
		if (fd == -1) throw new ::cake::SemanticError(0, JvNewStringUTF("file does not exist! ")->concat(JvNewStringUTF(filename.c_str())));
		dwarf::file f(fd);
		//dwarf::file f(fileno(file));
		dwarf::abi_information info(f);
	}
}

