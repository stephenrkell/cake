#include <string>
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstdlib>
#include "request.hpp"
#include "util.hpp"
#include "treewalk_helpers.hpp"
#include "parser.hpp"

namespace cake
{
	std::string new_anon_ident()
	{
		static int ctr = 0;
		std::ostringstream s;
		s << "anon_" << ctr++;
		return s.str();
	}
	
	std::string new_tmp_filename(std::string& module_constructor_name)
	{
		static int ctr = 0;
		std::ostringstream s;
		s << "tmp_" << ctr++ << '.' << module::extension_for_constructor(module_constructor_name);
		return s.str();
	}
	
	std::ostringstream exception_msg_stream("");
	
	std::string unescape_ident(std::string& ident)
	{
		std::ostringstream o;
		enum state { BEGIN, ESCAPE } state;

		for (std::string::iterator i = ident.begin(); i < ident.end(); i++)
		{
			switch(state)
			{
				case BEGIN:
					switch (*i)
					{
						case '\\':
							state = ESCAPE;
							break;
						default: 
							o << *i;
							break;
					}
					break;
				case ESCAPE:
					o << *i;
					break;
				default: break; // illegal state
			}
		}
		return o.str();
	}

	std::string unescape_string_lit(std::string& lit)
	{
		if (lit.length() < 2 || *lit.begin() != '"' || *(lit.end() - 1) != '"')
		{
			// string is not quoted, so just return it
			return lit;
		}

		std::ostringstream o;
		enum state { BEGIN, ESCAPE, OCTAL, HEX } state = BEGIN;
		std::string::iterator octal_begin;
		std::string::iterator octal_end;
		std::string::iterator hex_begin;
		std::string::iterator hex_end;

		for (std::string::iterator i = lit.begin() + 1; i < lit.end() - 1; i++)
		{
			// HACK: end-of-string also terminates oct/hex escape sequences
			if (state == HEX && i + 1 == lit.end()) { hex_end = i + 1; goto end_hex; }
			if (state == OCTAL && i + 1 == lit.end()) { octal_end = i + 1; goto end_oct; }
			switch(state)
			{
				case BEGIN:
					switch (*i)
					{
						case '\\':
							state = ESCAPE;
							break;
						case '\"':
							/* We have an embedded '"', which shouldn't happen... so
							 * we're free to do what we like. Let's skip over it silently. */						
							break;
						default: 
							o << *i;
							break;
					}
					break;
				case ESCAPE:
					switch (*i)
					{
						case 'n': o << '\n'; state = BEGIN; break;
						case 't': o << '\t'; state = BEGIN; break;
						case 'v': o << '\v'; state = BEGIN; break;
						case 'b': o << '\b'; state = BEGIN; break;
						case 'r': o << '\r'; state = BEGIN; break;
						case 'f': o << '\f'; state = BEGIN; break;
						case 'a': o << '\a'; state = BEGIN; break;
						case '\\': o << '\\'; state = BEGIN; break;
						case '?': o << '\?'; state = BEGIN; break;
						case '\'':o << '\''; state = BEGIN; break;
						case '\"':o << '\"'; state = BEGIN; break;
						case '0': case '1': case '2': case '3':
						case '4': case '5': case '6': 
						case '7': case '8': case '9':
							state = OCTAL;
							octal_begin = i;
							break;
						case 'x':
							state = HEX;
							hex_begin = i + 1;
							break;
						default:
							// illegal escape sequence -- ignore this
							break;
					}
					break;
				case OCTAL:
					switch (*i)
					{
						case '0': case '1': case '2': case '3':
						case '4': case '5': case '6': 
						case '7': case '8': case '9':
							continue;
						default: 
							// this is the end of octal digits
							octal_end = i;
							end_oct:
							{ 
								std::istringstream s(std::string(octal_begin, octal_end));
								char char_val;
								s >> std::oct >> char_val;
								o << char_val;
							}
							if (octal_end == i) o << *i;
							state = BEGIN;
							break;
					}
					break;
				case HEX:
					switch (*i)
					{
						case '0': case '1': case '2': case '3':
						case '4': case '5': case '6': case '7': 
						case '8': case '9': case 'a': case 'A':
						case 'b': case 'B': case 'c': case 'C':
						case 'd': case 'D': case 'e': case 'E': case 'f': case 'F':
							continue;
						default: 
							// end of hex digits
							hex_end = i;
							end_hex:
							{ 
								std::istringstream s(std::string(hex_begin, hex_end));
								char char_val;
								s >> std::hex >> char_val;
								o << char_val;
							}
							if (hex_end == i) o << *i;
							state = BEGIN;
							break;
					}
					break;					
				default: break; // illegal state
			}
		}
		return o.str();
	}
	
	std::pair<std::string, std::string> read_object_constructor(antlr::tree::Tree *t)
	{
		INIT;
		BIND3(t, id, IDENT);
		std::string fst(CCP(id->getText()));
		std::string snd;
		if (t->getChildCount() > 1) 
		{
			BIND3(t, quoted_lit, STRING_LIT);
			snd = std::string(CCP(quoted_lit->getText()));
		}
		return std::make_pair(fst, snd);
	}
	
	// FIXME: lots of copying here, along with other functions which return complex objects
	// as their return values. Consider a widespread adoption of heap-returning these, using
	// auto_ptr<> in the callers, or smarted shared pointers if copying happens.
	definite_member_name read_definite_member_name(antlr::tree::Tree *memberName)
	{
		definite_member_name name;
		switch(memberName->getType())
		{
			case '_':
				RAISE_INTERNAL(memberName, "expecting a definite memberName, found indefinite `_'");
			// no break
			case cakeJavaParser::DEFINITE_MEMBER_NAME: {
				definite_member_name vec(memberName->getChildCount());
				definite_member_name::iterator iter = vec.begin();
				FOR_ALL_CHILDREN(memberName)
				{
					*iter = std::string(CCP(n->getText()));
					iter++;
				}
				return vec;
			}
			// no break
			default: RAISE_INTERNAL(memberName, "bad syntax tree for memberName");			
			// no break
		}		
	}
	
	std::ostream& operator<<(std::ostream& o, const definite_member_name& n)
	{
		for (definite_member_name::const_iterator iter = n.begin();
			iter != n.end();
			iter++)
		{
			o << *iter;
			if (iter + 1 != n.end()) o << " :: ";	
		}
		return o;
	}
	
	std::string lookup_solib(std::string const&  basename)
	{
		const char *ld_library_path = getenv("LD_LIBRARY_PATH");
		if (ld_library_path == NULL) ld_library_path = guessed_system_library_path;
		
		std::string lib_path_str = std::string(ld_library_path);
		std::string::size_type i = 0;
		while (i < lib_path_str.length()) 
		{
			int colon_index = lib_path_str.find(':', i);
			std::string test_path = lib_path_str.substr(i,
				colon_index - i);
			i = colon_index + 1;
		
			// FIXME: highly platform-dependent logic follows
			std::string lib_filepath = test_path + "/" + guessed_system_library_prefix
				+ basename + "." + module::extension_for_constructor(solib_constructor);
			std::ifstream lib(lib_filepath.c_str(), std::ios::in);
			if (lib)
			{
				std::cerr << "Found library at " << lib_filepath << std::endl;
				return lib_filepath;
			}
		}
		return std::string("");
	}
	
	std::string solib_constructor = std::string("elf_external_sharedlib");
	const char *guessed_system_library_path = "/usr/lib:/lib";
	const char *guessed_system_library_prefix = "lib";
}
