// #include <java/lang/System.h>
// #include <java/io/File.h>
// #include <java/io/FileInputStream.h>
// #include <org/antlr/runtime/ANTLRInputStream.h>
// #include <org/antlr/runtime/ANTLRStringStream.h>
// #include <org/antlr/runtime/CommonTokenStream.h>
// #include <org/antlr/runtime/TokenStream.h>
// #include <org/antlr/runtime/tree/Tree.h>
// #include <org/antlr/runtime/tree/CommonTree.h>
// #include <cake/CloneableTree.h>
// #include <org/antlr/runtime/CommonToken.h>
/*
#undef EOF
#include "cakeJavaLexer.h"
#include "cakeJavaParser.h"
*/
//namespace cake { class cakeJavaLexer; }
//namespace cake { class cakeJavaParser; }
// #include "cake/SemanticError.h"
// #include "cake/InternalError.h"

#ifndef __CAKE__REQUEST_HPP
#define __CAKE__REQUEST_HPP

#include <vector>
#include <map>
#include <memory>
#include <boost/shared_ptr.hpp>
#include <fstream>
#include <functional>

#include <selective_iterator.hpp>

// our headers are too lazy to use the fully-qualified antlr namespace
//namespace antlr = ::org::antlr::runtime;
//#include "dwarfpp_simple.hpp"
#include "parser.hpp"
#include "module.hpp"

//namespace antlr { typedef ANTLRInputStream

namespace cake
{
   	typedef boost::shared_ptr<module_described_by_dwarf> module_ptr;
    //module_ptr make_module_ptr(described_module *arg) { return module_ptr(arg); }
    class derivation;
    class wrapper_file; // in wrapper.hpp
	class request
	{
		friend class derivation;
		friend class link_derivation;
		friend class rewrite_derivation;
		friend class make_exec_derivation;
		

		/* Source file */
		//jstring in_filename;
		//java::io::File *in_fileobj;
		//java::io::FileInputStream *in_file;
        const char *in_filename;
        pANTLR3_INPUT_STREAM/*std::ifstream*/ /*ANTLR3_FDSC*/ in_fileobj;

		/* Parsing apparatus */		
		//antlr::ANTLRInputStream *stream;
        //antlr::ANTLRInputStream *stream;
		//cakeJavaLexer *lexer;
        cakeCLexer *lexer;
		//antlr::CommonTokenStream *tokenStream;
        antlr::CommonTokenStream *tokenStream;
		//cakeJavaParser *parser;
        cakeCParser *parser;
		
		/* AST */
		antlr::tree::Tree *ast;
		
		/* AST traversal */		
		void depthFirst(antlr::tree::Tree *t);
		void toplevel();
				
		/* data structure instances */
        typedef std::map<std::string, module_ptr> module_tbl_t;
   		module_tbl_t module_tbl;	
        typedef module_tbl_t::value_type module_tbl_entry_t;
		
        	// we use a shared ptr because otherwise, to do module_tbl[i] = blah,
			// (or indeed any insertion into the map)
			// we'd implicitly be constructing our module locally as a temporary
			// and then copying it -- but it's very large, so we don't want that!
		std::map<std::string, std::vector<std::string> > module_alias_tbl;
        
        typedef std::vector<boost::shared_ptr<derivation> > derivation_tbl_t;
        derivation_tbl_t derivation_tbl;
        
        /* makes an absolute pathname out of a filename mentioned in Cake source */
        std::string make_absolute_pathname(std::string ref);
		
		/* processing alias declarations */
		void pass1_visit_alias_declaration(antlr::tree::Tree *t);
		
		/* processing exists declarations */
		void add_existing_module(std::string& module_constructor_name,
			std::string& quoted_filename,
			std::string& module_ident);
			
		/* processing derive declarations */
		void add_derive_rewrite(std::string& derived_ident,
			std::string& filename_text,
			antlr::tree::Tree *derive_body);
			
		void extract_aliases();	
        
		void extract_inlines();
		void build_inlines();
	    module_ptr::pointer create_existing_module(std::string& constructor,
	    	std::string& filename);
        
		void extract_exists();
		void add_exists(antlr::tree::Tree *n);
        
		void extract_supplementary();
        
		void extract_derivations();
        void add_derivation(antlr::tree::Tree *n);
	    derivation *create_derivation(std::string&, antlr::tree::Tree *t);	
	    derived_module *create_derived_module(derivation& d, std::string& filename);      	
		// derivations may have to happen in some order -- that doesn't mean
		// we have to process them in that order, although it might if we
		// end up supporting a derivation algebra (see below) since we might
		// have to compute an "exists" block for intermediate results.
		void compute_derivation_dependencies();
		void sort_derivations();
					
	public:
		request(const char *filename);
		int process();
		
		//static void print_abi_info(dwarf::abi_information& info, std::string& unescaped_filename);
	};
	
	class derivation
	{	
	protected:
		request& r;
		antlr::tree::Tree *t;
        module_ptr output_module; // defaults to null
        std::vector<module_ptr> input_modules; // defaults to empty
		
	public:
		derivation(request& r, antlr::tree::Tree *t)
         : r(r), t(t) {}
		virtual void extract_definition() = 0;
		virtual void write_makerules(std::ostream& out) = 0;
		virtual std::vector<std::string> dependencies() = 0;
	};
	
	
	class rewrite_derivation : public derivation
	{
	
	public:
		void write_makerules(std::ostream& out);	
	};
	
	class make_exec_derivation : public derivation
	{
	
	public:
		void write_makerules(std::ostream& out);	
	};
	
	//const char *token_name(jint t);
}

#endif
