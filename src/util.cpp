#include <string>
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstdlib>
#include <cctype>
#include "request.hpp"
#include "util.hpp"
//#include "treewalk_helpers.hpp"
#include "parser.hpp"
#include "module.hpp"

using boost::shared_ptr;
using dwarf::spec::type_die;
using dwarf::spec::typedef_die;
using dwarf::spec::type_chain_die;
using boost::dynamic_pointer_cast;
using std::vector;

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
	
	std::string unescape_ident(const std::string& ident)
	{
		std::ostringstream o;
		enum { BEGIN, ESCAPE } state = BEGIN;

		for (auto i = ident.begin(); i != ident.end(); i++)
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
					state = BEGIN;
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
		std::string fst(CCP(GET_TEXT(id)));
		std::string snd;
		if (GET_CHILD_COUNT(t) > 1) 
		{
			BIND3(t, quoted_lit, STRING_LIT);
			snd = std::string(CCP(GET_TEXT(quoted_lit)));
		}
		return std::make_pair(fst, snd);
	}
	
//	// FIXME: lots of copying here, along with other functions which return complex objects
//	// as their return values. Consider a widespread adoption of heap-returning these, using
//	// auto_ptr<> in the callers, or smarted shared pointers if copying happens.
//	definite_member_name read_definite_member_name(antlr::tree::Tree *memberName)
//	{
//	}
	definite_member_name::definite_member_name(antlr::tree::Tree *t)
	{
		switch(GET_TYPE(t))
		{
			case '_':
				RAISE_INTERNAL(t, "expecting a definite memberName, found indefinite `_'");
			// no break
			case '.': {
				INIT;
				BIND3(t, left, IDENT);
				BIND2(t, right);
				*this = read_definite_member_name(right);
				push_back(CCP(GET_TEXT(left)));
			}
			break;
			case CAKE_TOKEN(IDENT):
				*this = definite_member_name(1, std::string(CCP(GET_TEXT(t))));
			break;
			case CAKE_TOKEN(DEFINITE_MEMBER_NAME): {
				INIT;
				//FOR_ALL_CHILDREN(t)
				//{
				//	push_back(std::string(CCP(GET_TEXT(n))));
				//}
				BIND2(t, top);
				*this = read_definite_member_name(top);
			}
			break;
			default: RAISE_INTERNAL(t, "bad syntax tree for memberName");			
		}
	}			
	definite_member_name read_definite_member_name(antlr::tree::Tree *memberName)
	{ return definite_member_name(memberName); }
	
	antlr::tree::Tree *make_definite_member_name_expr(const definite_member_name& arg)
	{
		// We do this by building a string and feeding it to the parser.
	 	std::cerr << "creating definite member name tree for " << arg << std::endl;

		std::string fragment;
		for (auto i = arg.begin(); i != arg.end(); i++)
		{
			if (i != arg.begin()) fragment += ".";
			fragment += cake_token_text_from_ident(*i);
		}
		char *dup = strdup(fragment.c_str());
		pANTLR3_INPUT_STREAM ss = antlr3StringStreamNew(
			reinterpret_cast<uint8_t*>(dup), 
			ANTLR3_ENC_UTF8, fragment.size(), (uint8_t *)"(no file)");
		cakeCLexer *lexer = cakeCLexerNew(ss);
		antlr::CommonTokenStream *tokenStream = antlr3CommonTokenStreamSourceNew(
			ANTLR3_SIZE_HINT, TOKENSOURCE(lexer));
		cakeCParser *parser = cakeCParserNew(tokenStream); 
		cakeCParser_definiteMemberName_return ret = parser->definiteMemberName(parser);
		
		// We should now have the tree in ret.tree. 
		// Free all the other temporary stuff we created.
		// FIXME: work out which bits I can free now and which to cleanup later!
		//ss->free(ss);
		//lexer->free(lexer);
		//tokenStream->free(tokenStream);
		//parser->free(parser);
		//free(dup);
		
		return ret.tree;
	}
	antlr::tree::Tree *make_ident_expr(const std::string& ident)
	{
		// We do this by building a string and feeding it to the parser.
	 	std::cerr << "creating ident AST for " << ident << std::endl;
// 
// 		char *dup = strdup(ident.c_str());
// 		pANTLR3_INPUT_STREAM ss = antlr3StringStreamNew(
// 			reinterpret_cast<uint8_t*>(dup), 
// 			ANTLR3_ENC_UTF8, ident.size(), (uint8_t *)"(no file)");
// 		cakeCLexer *lexer = cakeCLexerNew(ss);
// 		antlr::CommonTokenStream *tokenStream = antlr3CommonTokenStreamSourceNew(
// 			ANTLR3_SIZE_HINT, TOKENSOURCE(lexer));
// 		cakeCParser *parser = cakeCParserNew(tokenStream); 
// 		// HACK: use stubPrimitiveExpression to build idents
// 		cakeCParser_stubPrimitiveExpression_return ret = parser->stubPrimitiveExpression(parser);
// 	
// 		return ret.tree;

		return make_ast(ident, &cakeCParser::stubPrimitiveExpression);
	}
	
	template<typename AntlrReturnedObject>
	antlr::tree::Tree *
	make_ast(
		const std::string& fragment, 
		AntlrReturnedObject (* cakeCParser::* parserFunction)(cakeCParser_Ctx_struct *)
	)
	{
		// We do this by building a string and feeding it to the parser.
	 	std::cerr << "creating arbitrary AST for fragment " << fragment << std::endl;

		char *dup = strdup(fragment.c_str());
		pANTLR3_INPUT_STREAM ss = antlr3StringStreamNew(
			reinterpret_cast<uint8_t*>(dup), 
			ANTLR3_ENC_UTF8, fragment.size(), (uint8_t *)"(no file)");
		cakeCLexer *lexer = cakeCLexerNew(ss);
		antlr::CommonTokenStream *tokenStream = antlr3CommonTokenStreamSourceNew(
			ANTLR3_SIZE_HINT, TOKENSOURCE(lexer));
		cakeCParser *parser = cakeCParserNew(tokenStream); 
		// HACK: use stubPrimitiveExpression to build idents
		AntlrReturnedObject ret = (parser->*parserFunction)(parser);
	
		return ret.tree;
	}
	
	std::string cake_token_text_from_ident(const std::string& arg)
	{
		/* Turn arg into some text that will be parsed as a Cake token.
		 * This means punctuation must be escaped, and
		 * if the result is a reserved word, 
		 * we should insert an arbitrary escape. */
		
		const unsigned bufsize = 4096;
		char buf[bufsize];
		auto i_c = arg.begin();
		char *p_c = &buf[0];
		assert(*i_c && (isalpha(*i_c) || (*i_c == '_')));
		do 
		{
			if (!isalnum(*i_c) && (*i_c != '_')) *p_c++ = '\\';
			*p_c++ = *i_c;
			assert(p_c - buf < bufsize); // must have room for null terminator too
		} while (i_c != arg.end() && *++i_c);
		// insert null terminator
		*p_c++ = '\0';
		if (is_cake_keyword(std::string(buf)))
		{
			// move back two places
			p_c -= 2;
			// insert an escape character
			*p_c++ = '\\';
			// insert the final input character again
			*p_c++ = *(i_c - 1);
			// insert the null terminator again
			*p_c++ = '\0';
			// definitely not a keyword now
			assert(!is_cake_keyword(buf));
			// did we spill over? oh dear
			assert(p_c - buf <= bufsize);
		}
		return std::string(buf);
	}
	
	bool is_cake_keyword(const std::string& arg)
	{
		char *dup = strdup(arg.c_str());
		pANTLR3_INPUT_STREAM ss = antlr3StringStreamNew(
			reinterpret_cast<uint8_t*>(dup), 
			ANTLR3_ENC_UTF8, arg.size(), (uint8_t *)"(no file)");
		cakeCLexer *lexer = cakeCLexerNew(ss);
		antlr::CommonTokenStream *tokenStream = antlr3CommonTokenStreamSourceNew(
			ANTLR3_SIZE_HINT, TOKENSOURCE(lexer));

		auto vec = tokenStream->getTokens(tokenStream);
		assert(tokenStream->p != -1);
		pANTLR3_COMMON_TOKEN tok = static_cast<pANTLR3_COMMON_TOKEN>(vec->get(vec, tokenStream->p));
		
		bool retval;
		//std::cerr << "Pulled out a token of type " << GET_TYPE(tok) << std::endl;
		if (tok->getType(tok) != CAKE_TOKEN(IDENT)) retval = true;
		else retval = false;
		
		ss->free(ss);
		lexer->free(lexer);
		tokenStream->free(tokenStream);
		free(dup);
		
		return retval;
	}

	std::string get_event_pattern_call_site_name(antlr::tree::Tree *t)
	{
		INIT;
		switch (GET_TYPE(t))
		{
			case CAKE_TOKEN(EVENT_PATTERN): {
				BIND3(t, eventContext, EVENT_CONTEXT);
				BIND2(t, memberNameExpr);
				definite_member_name mn;
				switch (GET_TYPE(memberNameExpr))
				{
					case CAKE_TOKEN(INDEFINITE_MEMBER_NAME):
						RAISE_INTERNAL(memberNameExpr, "invoked events may not be indefinite");
					case CAKE_TOKEN(DEFINITE_MEMBER_NAME):
						// the good case
						mn = read_definite_member_name(memberNameExpr);
						if (mn.size() != 1) RAISE(memberNameExpr, 
							"invoked events may not contain `.'");
						else return mn.at(0);
					default: RAISE_INTERNAL(memberNameExpr, "not a member name expr");
				}
				// FOR_REMAINING_CHILDREN( ) // annotatedValuePattern 
			} break;
			default:
				RAISE_INTERNAL(t, "not an event pattern");
		}
	}   
	
	antlr::tree::Tree *make_simple_event_pattern_for_call_site(
		const std::string& name)
	{
		// We do this by building a string and feeding it to the parser.
		// This avoids strong dependency on the parse tree data structure,
		// at the cost of depending strongly on the Cake grammar. But we're
		// not likely to make major changes to such a simple part of it.

		std::cerr << "creating event pattern for call-site name: " << name  << std::endl;

		std::string fragment(name); fragment += "(...)";
		char *dup = strdup(fragment.c_str());
		pANTLR3_INPUT_STREAM ss = antlr3StringStreamNew(
			reinterpret_cast<uint8_t*>(dup), 
			ANTLR3_ENC_UTF8, fragment.size(), (uint8_t*)"(no file)");
		cakeCLexer *lexer = cakeCLexerNew(ss);
		antlr::CommonTokenStream *tokenStream = antlr3CommonTokenStreamSourceNew(
			ANTLR3_SIZE_HINT, TOKENSOURCE(lexer));
		cakeCParser *parser = cakeCParserNew(tokenStream); 
		cakeCParser_eventPattern_return ret = parser->eventPattern(parser);
		
		// We should now have the tree in ret.tree. 
		// Free all the other temporary stuff we created.
		// FIXME: work out which bits I can free now and which to cleanup later!
		//ss->free(ss);
		//lexer->free(lexer);
		//tokenStream->free(tokenStream);
		//parser->free(parser);
		//free(dup);
		
		std::cerr << "Created simple event pattern for call site " 
			<< name << ": " << CCP(TO_STRING_TREE(ret.tree)) << std::endl;
		return ret.tree;
	}

	antlr::tree::Tree *make_simple_sink_expression_for_event_name(
		const std::string& event_name)
	{
		// We do this by building a string and feeding it to the parser.
		// This avoids strong dependency on the parse tree data structure,
		// at the cost of depending strongly on the Cake grammar. But we're
		// not likely to make major changes to such a simple part of it.

		std::cerr << "creating sink expression from event name: " << event_name  << std::endl;

		std::string fragment(event_name); 
		fragment += "(in_args...)";
		char *dup = strdup(fragment.c_str());
		pANTLR3_INPUT_STREAM ss = antlr3StringStreamNew(
			reinterpret_cast<uint8_t*>(dup), 
			ANTLR3_ENC_UTF8, fragment.size(), (uint8_t*)"(no file)");
		cakeCLexer *lexer = cakeCLexerNew(ss);
		antlr::CommonTokenStream *tokenStream = antlr3CommonTokenStreamSourceNew(
			ANTLR3_SIZE_HINT, TOKENSOURCE(lexer));
		cakeCParser *parser = cakeCParserNew(tokenStream); 
		cakeCParser_eventPatternRewriteExpr_return ret = parser->eventPatternRewriteExpr(parser);
		
		// We should now have the tree in ret.tree. 
		// Free all the other temporary stuff we created.
		// FIXME: work out which bits I can free now and which to cleanup later!
		//ss->free(ss);
		//lexer->free(lexer);
		//tokenStream->free(tokenStream);
		//parser->free(parser);
		//free(dup);
		
		std::cerr << "Created simple sink expression for event name " 
			<< event_name << ": " << CCP(TO_STRING_TREE(ret.tree)) << std::endl;
		return ret.tree;
	}
	
	antlr::tree::Tree *make_simple_corresp_expression(
		const std::vector<std::string>& ident, boost::optional<std::vector<std::string>& > rhs_ident)
	{
		std::cerr << "creating corresp expression for: " << ident  << std::endl;

		std::string ident_in_cake;
		std::string rhs_ident_in_cake;
		std::string fragment;
		for (auto i_ident = ident.begin(); i_ident != ident.end(); i_ident++)
		{
			if (i_ident != ident.begin()) ident_in_cake += ".";
			ident_in_cake += cake_token_text_from_ident(*i_ident);
		}
		fragment = ident_in_cake + " <--> " + ident_in_cake;
		if (!rhs_ident)
		{
			fragment += ident_in_cake;
		}
		else
		{
			for (auto i_ident = rhs_ident->begin(); i_ident != rhs_ident->end(); i_ident++)
			{
				if (i_ident != rhs_ident->begin()) rhs_ident_in_cake += ".";
				rhs_ident_in_cake += cake_token_text_from_ident(*i_ident);
			}
			fragment += rhs_ident_in_cake;
		}
		char *dup = strdup(fragment.c_str());
		pANTLR3_INPUT_STREAM ss = antlr3StringStreamNew(
			reinterpret_cast<uint8_t*>(dup), 
			ANTLR3_ENC_UTF8, fragment.size(), (uint8_t *)"(no file)");
		cakeCLexer *lexer = cakeCLexerNew(ss);
		antlr::CommonTokenStream *tokenStream = antlr3CommonTokenStreamSourceNew(
			ANTLR3_SIZE_HINT, TOKENSOURCE(lexer));
		cakeCParser *parser = cakeCParserNew(tokenStream); 
		cakeCParser_valueCorrespondenceBase_return ret = parser->valueCorrespondenceBase(parser);
		
		// We should now have the tree in ret.tree. 
		// Free all the other temporary stuff we created.
		// FIXME: work out which bits I can free now and which to cleanup later!
		//ss->free(ss);
		//lexer->free(lexer);
		//tokenStream->free(tokenStream);
		//parser->free(parser);
		//free(dup);
		
		return ret.tree;
	}

	boost::optional<std::string> source_pattern_is_simple_function_name(antlr::tree::Tree *t)
	{
		return pattern_is_simple_function_name(t);
	}

	boost::optional<std::string> pattern_is_simple_function_name(antlr::tree::Tree *t)
	{
		assert(GET_TYPE(t) == CAKE_TOKEN(EVENT_PATTERN));
		if (GET_CHILD_COUNT(t) > 4) return false;
		INIT;
		BIND2(t, eventContext);
		BIND2(t, memberNameExpr);
		BIND2(t, eventCountPredicate);
		BIND2(t, eventParameterNamesAnnotation);
		if (GET_TYPE(memberNameExpr) != CAKE_TOKEN(DEFINITE_MEMBER_NAME)) return false;
		else 
		{
			if (GET_CHILD_COUNT(memberNameExpr) != 1) return false;
			else return std::string(CCP(GET_TEXT(GET_CHILD(memberNameExpr, 0))));
		}
	}
	
	boost::optional<std::string> sink_expr_is_simple_function_name(antlr::tree::Tree *t)
	{
		switch(GET_TYPE(t))
		{
			case CAKE_TOKEN(EVENT_SINK_AS_PATTERN):
				{
					INIT;
					BIND3(t, eventPattern, EVENT_PATTERN);
					return pattern_is_simple_function_name(eventPattern);
				} 
			case CAKE_TOKEN(EVENT_SINK_AS_STUB):
				{
					INIT;
					BIND2(t, stubTopLevel);
					std::cerr << "Sink stub expression has toplevel token type " 
						<< GET_TYPE(stubTopLevel) << std::endl;
					if (GET_TYPE(stubTopLevel) == CAKE_TOKEN(IDENT))
					{
						return std::string(CCP(GET_TEXT(stubTopLevel)));
					} else return false;
				}
				return false;
			default: assert(false); return false;
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
	
	vector<shared_ptr<type_die> > 
	type_synonymy_chain(shared_ptr<type_die> d)
	{
		vector<shared_ptr<type_die> > v;
		auto concrete = d->get_concrete_type();
		while (d != concrete)
		{
			auto tc = dynamic_pointer_cast<type_chain_die>(d);
			assert(tc);
			if (dynamic_pointer_cast<typedef_die>(tc)) v.push_back(d);
			assert(tc->get_type());
			d = *tc->get_type();
			assert(d);
		}
		return v;
	}
		
	std::string solib_constructor = std::string("elf_external_sharedlib");
	const char *guessed_system_library_path = "/usr/lib:/lib";
	const char *guessed_system_library_prefix = "lib";
}
