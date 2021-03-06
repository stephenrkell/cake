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
#include <fstream>
#include <functional>
#include "cake/cxx_target.hpp"

#include <selective_iterator.hpp>

// our headers are too lazy to use the fully-qualified antlr namespace
//namespace antlr = ::org::antlr::runtime;
//#include "dwarfpp_simple.hpp"
#include "parser.hpp"
#include "module.hpp"

//namespace antlr { typedef ANTLRInputStream

namespace cake
{
    //module_ptr make_module_ptr(described_module *arg) { return module_ptr(arg); }
    class derivation;
    class wrapper_file; // in wrapper.hpp
	class link_derivation; 
	class instantiate_derivation;
	class request
	{
		friend class derivation;
		friend class link_derivation;
		friend class rewrite_derivation;
		friend class instantiate_derivation;
		friend class make_exec_derivation;
		friend class wrapper_file;

		/* Source file */
		//jstring in_filename;
		//java::io::File *in_fileobj;
		//java::io::FileInputStream *in_file;
		const char *in_filename;
		pANTLR3_INPUT_STREAM/*std::ifstream*/ /*ANTLR3_FDSC*/ in_fileobj;
		
		const char *out_filename;
		std::ofstream maybe_out_stream;
		std::ostream *p_out;
		
		/* data structure instances */
        typedef std::map<std::string, module_ptr> module_tbl_t;
        typedef std::map<module_ptr, std::string> module_inverse_tbl_t;
   		module_tbl_t module_tbl;
        module_inverse_tbl_t module_inverse_tbl;
        typedef module_tbl_t::value_type module_tbl_entry_t;

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
		
		// the name of the toplevel derivation
		string start_derivation;
		
	public:
		// main Cake modes of operation
		void do_derivation(const string& derived_module_name);
		void write_headers(const string& module_name);
		void write_makerules();
	private:
		/* indenting output stream */
		srk31::indenting_ostream indenting_out;
		/* cxx generation */
		cake_cxx_target compiler; 

	public:
    	typedef module_inverse_tbl_t::value_type module_name_pair;
        		
        	// we use a shared ptr because otherwise, to do module_tbl[i] = blah,
			// (or indeed any insertion into the map)
			// we'd implicitly be constructing our module locally as a temporary
			// and then copying it -- but it's very large, so we don't want that!
		std::map<std::string, std::vector<std::string> > module_alias_tbl;
        
        typedef std::map<string, std::shared_ptr<derivation> > derivation_tbl_t;
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
	    module_ptr create_existing_module(std::string& constructor,
	    	std::string& filename);
        
		void extract_exists();
		void add_exists(antlr::tree::Tree *n);
        
		void extract_supplementary(const string& target_module);
        
		void extract_derivations();
		void add_derivation(antlr::tree::Tree *n);
		shared_ptr<derivation> create_derivation(const std::string&, const std::string&, antlr::tree::Tree *t);	
		shared_ptr<derived_module> create_derived_module(derivation& d, 
			const std::string& id, const std::string& filename);
		
		void recursively_init_derivations(derivation_tbl_t::iterator i_d);
					
	public:
		request(const char *cakefile, const char *makefile, const char *outfile);
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
	
	protected:
		virtual void write_object_dependency_makerules(std::ostream& out);
		virtual string get_emitted_sourcefile_name() = 0;
		vector<string> m_depended_upon_module_names;
		
	public:
		derivation(request& r, antlr::tree::Tree *t)
		: r(r), t(t) {}
		std::vector<module_ptr> get_input_modules() const { return input_modules; }
		virtual void init() = 0;
		virtual const std::string& namespace_name() = 0;
		virtual void write_makerules(std::ostream& out) = 0;
		virtual void write_cxx() = 0;
		vector<string> dependencies() { return m_depended_upon_module_names; }
        module_ptr get_output_module() { return output_module; }
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
