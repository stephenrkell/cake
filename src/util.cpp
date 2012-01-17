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
using namespace dwarf;
using dwarf::spec::basic_die;
using dwarf::spec::type_die;
using dwarf::spec::typedef_die;
using dwarf::spec::type_chain_die;
using boost::dynamic_pointer_cast;
using std::vector;
using std::cerr;
using std::endl;

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
				FOR_ALL_CHILDREN(t)
				{
					push_back(std::string(CCP(GET_TEXT(n))));
				}
				//BIND2(t, top);
				//*this = read_definite_member_name(top);
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
		for (auto i_ident = ident.begin(); i_ident != ident.end(); ++i_ident)
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
			for (auto i_ident = rhs_ident->begin(); i_ident != rhs_ident->end(); ++i_ident)
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
		// sanity check
		bool is_typedef = dynamic_pointer_cast<typedef_die>(d);
		
		// begin proper
		auto concrete = d->get_concrete_type();
		while (d != concrete)
		{
			auto tc = dynamic_pointer_cast<type_chain_die>(d);
			assert(tc);
			if (dynamic_pointer_cast<typedef_die>(tc)) v.push_back(d);
			assert(tc->get_type());
			d = tc->get_type();
			assert(d);
		}
		
		// sanity check
		assert(!is_typedef || v.size() > 0);
		return v;
	}

	int 
	path_to_node(antlr::tree::Tree *ancestor,
		antlr::tree::Tree *target, std::deque<int>& out)
	{
		std::deque<int> retpath;
		if (ancestor == target) { out = retpath; return 0; }
		else for (unsigned i_child = 0U; i_child < GET_CHILD_COUNT(ancestor); ++i_child)
		{
			if (0 == path_to_node(GET_CHILD(ancestor, i_child), target, retpath))
			{
				retpath.push_front(i_child);
				out = retpath; 
				return 0;
			}
		}
		return -1;
	}

	boost::shared_ptr<dwarf::spec::basic_die>
	map_ast_context_to_dwarf_element(
		antlr::tree::Tree *node,
		module_ptr dwarf_context,
		bool must_be_immediate
	)
	{
		if (!node) return boost::shared_ptr<dwarf::spec::basic_die>();
//		antlr::tree::Tree *prev_node = 0;
		assert(GET_TYPE(node) == CAKE_TOKEN(IDENT));
		cerr << "Considering use contexts of ident " << CCP(TO_STRING_TREE(node))
			<< endl;

		// first we do a depthfirst walk of the tree from the ancestor,
		// looking for the node
// 		std::deque<int> path;
// 		int retval = path_to_node(ancestor, node, path);
// 		if (retval == -1) RAISE_INTERNAL(node, "not an ancestor");
// 		// parent chain goes from node to ancestor, inclusive
// 		std::deque<antlr::tree::Tree *> parent_chain;
// 		antlr::tree::Tree *cur = ancestor;
// 		parent_chain.push_back(node);
// 		for (auto i_ind = path.begin(); i_ind != path.end(); ++i_ind) 
// 		{
// 			parent_chain.push_front(cur);
// 			cur = GET_CHILD(cur, *i_ind);
// 		}
// 		assert(cur == node);

		//auto i_path = path.rbegin();
		antlr::tree::Tree *begin_node = node;
		antlr::tree::Tree *prev_node = 0;
		antlr::tree::Tree *prev_prev_node = 0;
		while (node && (prev_prev_node = prev_node, prev_node = node, ((node = GET_PARENT(node)) != NULL)))
		//for (auto i_node = parent_chain.begin(); i_node != parent_chain.end(); ++i_node, ++i_path)
		{
			if (!node) 
			{
				// we've reached the top
				break;
			}

			// if we're not at the end of the chain, we can reach the current node like so...
			//assert(i_node == parent_chain.end() -1 ||
			//	GET_CHILD(*(i_node + 1), *i_path) == *i_node);
			
			cerr << "Considering subtree " << CCP(TO_STRING_TREE(node))
				<< endl;
			switch (GET_TYPE(node))
			{
				case CAKE_TOKEN(INVOKE_WITH_ARGS): {
					antlr::tree::Tree *invoked_function = GET_CHILD(node, GET_CHILD_COUNT(node) - 1);
					assert(invoked_function && 
						(GET_TYPE(invoked_function) == CAKE_TOKEN(DEFINITE_MEMBER_NAME)
						|| (GET_TYPE(invoked_function) == CAKE_TOKEN(IDENT))));
					cerr << "Found a function call, to " << CCP(TO_STRING_TREE(invoked_function)) << endl;
					// we only match if we're directly underneath an argument position
					if (must_be_immediate 
						&& !(GET_PARENT(begin_node) && GET_PARENT(GET_PARENT(begin_node)) == node))
					{
						cerr << "Ident grandparent is not this function call." << endl;
						goto failed;
					}
					else
					{
					
						definite_member_name dmn;
						if (GET_TYPE(invoked_function) == CAKE_TOKEN(IDENT))
						{ dmn.push_back(CCP(GET_TEXT(invoked_function))); }
						else dmn = read_definite_member_name(invoked_function);
						auto found_dwarf = dwarf_context->get_ds().toplevel()->visible_resolve(
							dmn.begin(), dmn.end());
						auto found_subp = dynamic_pointer_cast<dwarf::spec::subprogram_die>(found_dwarf);
						assert(found_dwarf); assert(found_subp);
						cerr << "We think that this subtree is a call to " << *found_subp << endl;
						int num = 0;
						
						for (auto i_fp = found_subp->formal_parameter_children_begin(); i_fp != 
							found_subp->formal_parameter_children_end(); ++i_fp, ++num)
						{
							cerr << "Is our context argument " << num 
								<< " of " << CCP(TO_STRING_TREE(node))
								<< "? ";
							auto multivalue = GET_CHILD(node, 0); assert(multivalue);
							if (GET_CHILD(multivalue, num) == /**i_path */ prev_prev_node) 
							{
								cerr << "yes." << endl;
								return *i_fp;
							}
							else cerr << "no." << endl;
						}
						assert(false);
					}
				}
				case CAKE_TOKEN(EVENT_PATTERN): {
					unsigned prev_node_pos = 0U;
					while (prev_node_pos < GET_CHILD_COUNT(node) && GET_CHILD(node, prev_node_pos) != prev_node)
					{ ++prev_node_pos; }
					assert(prev_node_pos != GET_CHILD_COUNT(node)); // we should always find prev_node
					
					// get the subprogram
					shared_ptr<spec::subprogram_die> subprogram;
					{
						INIT;
						BIND3(node, eventContext, EVENT_CONTEXT);
						BIND3(node, memberNameExpr, DEFINITE_MEMBER_NAME); // name of call being matched -- can ignore this here
						
						auto dmn = read_definite_member_name(memberNameExpr);
						assert(dmn.size() == 1);
						
						auto found = dwarf_context->get_ds().toplevel()->visible_resolve(
							dmn.begin(), dmn.end());
						assert(found);
						subprogram = dynamic_pointer_cast<spec::subprogram_die>(found);
						assert(subprogram);
					}
					// now look for the pos'th fp
					unsigned pos = 0;
					spec::subprogram_die::formal_parameter_iterator i_fp;
					for (i_fp = subprogram->formal_parameter_children_begin();	
						i_fp != subprogram->formal_parameter_children_end();	
						++i_fp, ++pos)
					{
						if (pos == prev_node_pos) break;
					}
					assert(i_fp != subprogram->formal_parameter_children_end());
					
					return *i_fp;
				} break;
				// most tokens we silently ascend through
				
				case CAKE_TOKEN(RL_DOUBLE_ARROW):
				case CAKE_TOKEN(LR_DOUBLE_ARROW):
				case CAKE_TOKEN(BI_DOUBLE_ARROW):
				case CAKE_TOKEN(RL_DOUBLE_ARROW_Q):
				case CAKE_TOKEN(LR_DOUBLE_ARROW_Q):
					/* Here we've reached the end of the per-module AST. We should really
					 * check that the context we came from pertains to the argument module (FIXME). */
					break;

				// if we get to the top of a link block, we're definitely finishing
				case CAKE_TOKEN(PAIRWISE_BLOCK_LIST):
				case CAKE_TOKEN(TOPLEVEL):
					break;
				
				case CAKE_TOKEN(IDENT):
				case CAKE_TOKEN(DEFINITE_MEMBER_NAME):
				case CAKE_TOKEN(MULTIVALUE):
				default:
					continue;
			}
		} // end while walking up tree
	failed:
		return boost::shared_ptr<dwarf::spec::basic_die>();
	}
	
	bool treat_subprogram_as_untyped(
		boost::shared_ptr<dwarf::spec::subprogram_die> subprogram)
	{
		auto args_begin 
			= subprogram->formal_parameter_children_begin();
		auto args_end
			= subprogram->formal_parameter_children_end();
		return (args_begin == args_end
						 && subprogram->unspecified_parameters_children_begin() !=
						subprogram->unspecified_parameters_children_end());
	}	
	bool subprogram_returns_void(
		shared_ptr<spec::subprogram_die> subprogram)
	{
		if (!subprogram->get_type())
		{
			if (treat_subprogram_as_untyped(subprogram))
			{
				std::cerr << "Warning: assuming function " << *subprogram << " is void-returning."
					<< std::endl;
			}
			return true;
		}
		return false;
	}			
		
	std::string solib_constructor = std::string("elf_external_sharedlib");
	const char *guessed_system_library_path = "/usr/lib:/lib";
	const char *guessed_system_library_prefix = "lib";
}
