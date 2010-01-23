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
#include <vector>
#include <map>
#include <memory>
#include <boost/shared_ptr.hpp>
#include <fstream>

// our headers are too lazy to use the fully-qualified antlr namespace
//namespace antlr = ::org::antlr::runtime;
//#include "dwarfpp_simple.hpp"
#include "parser.hpp"
//#include "module.hpp"

//namespace antlr { typedef ANTLRInputStream

namespace cake
{
	class module;
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
        char *in_filename;
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
		std::map<std::string, boost::shared_ptr<module> > module_tbl;	
			// we use a shared ptr because otherwise, to do module_tbl[i] = blah,
			// (or indeed any insertion into the map)
			// we'd implicitly be constructing our module locally as a temporary
			// and then copying it -- but it's very large, so we don't want that!
		std::map<std::string, std::vector<std::string> > module_alias_tbl;
		
		/* processing alias declarations */
		void pass1_visit_alias_declaration(antlr::tree::Tree *t);
		
		/* processing exists declarations */
		void add_exists(std::string& module_constructor_name,
			std::string& quoted_filename,
			std::string& module_ident);
			
		/* processing derive declarations */
		void add_derive_rewrite(std::string& derived_ident,
			std::string& filename_text,
			antlr::tree::Tree *derive_body);
			
		void extract_aliases();	
		void extract_inlines();
		void build_inlines();
		void extract_exists();
		void extract_supplementary();
		void extract_derivations();
		
		// derivations may have to happen in some order -- that doesn't mean
		// we have to process them in that order, although it might if we
		// end up supporting a derivation algebra (see below) since we might
		// have to compute an "exists" block for intermediate results.
		void compute_derivation_dependencies();
		
					
	public:
		request(char *filename);
		int process();
		
		//static void print_abi_info(dwarf::abi_information& info, std::string& unescaped_filename);
	};
	
	class derivation
	{	
	protected:
		request *r;
		antlr::tree::Tree *t;		
		
	public:
		derivation(request *r, antlr::tree::Tree *t) : r(r), t(t) {}
		virtual void extract_definition() = 0;
		virtual void write_makerules(std::ostream& out) = 0;
		virtual std::vector<std::string> dependencies() = 0;
	};
	
	class link_derivation : public derivation
	{
		void extract_event_correspondences();		
		void extract_value_correspondences();
	
		void compute_function_bindings();
		void compute_form_value_correspondences(); 

		void compute_static_value_correspondences();		
		void compute_dwarf_type_compatibility(); 

		void compute_rep_domains();
		void output_rep_conversions();
		
		void compute_interposition_points();
		void output_symbol_renaming_rules();
		
		void output_formgens();		
		void output_wrappergens();
		
		void output_static_co_objects(); 
		
	protected:
		void extract_definition();

	public:
		void write_makerules(std::ostream& out);	
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
