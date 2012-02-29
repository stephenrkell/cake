#include <string>
#include <iostream>
#include <cassert>
#include <cstring>
#include <cstdio>
#include <set>
#include <algorithm>
#include <iterator>
#include "request.hpp"
#include "parser.hpp"
#include "util.hpp"
#include <srk31/indenting_ostream.hpp>
#include <dwarfpp/cxx_dependency_order.hpp>

using std::set;
using std::string;

namespace cake
{
	request::request(const char *cakefile, const char *makefile, const char *outfile /* ignored for now */)
		:	in_filename(cakefile),
			in_fileobj(antlr3FileStreamNew(
				reinterpret_cast<uint8_t*>(
					const_cast<char*>(cakefile)), ANTLR3_ENC_UTF8)),
			out_filename(makefile),
			maybe_out_stream(makefile ? makefile : "/dev/null"),
			p_out(makefile ? &maybe_out_stream : &std::cout),
			module_tbl()
	{
		if (!in_fileobj) throw cake::SemanticError("error opening input file");
		if (!makefile && !maybe_out_stream) throw cake::SemanticError("error opening output file");
		assert(*p_out);
		
		lexer = cakeCLexerNew(in_fileobj);
		tokenStream = antlr3CommonTokenStreamSourceNew(ANTLR3_SIZE_HINT, TOKENSOURCE(lexer));
		parser = cakeCParserNew(tokenStream); 
		cakeCParser_toplevel_return ret = parser->toplevel(parser);
		antlr::tree::Tree *tree = ret.tree;
		
		// process the file
		this->ast = tree;
		// build our tables
		extract_aliases();	
		//extract_inlines();
		//build_inlines();
		extract_exists();
		extract_supplementary();
		extract_derivations();
		
		// no need to topsort derive dependencies any more --
		// we use Make's topsort for this
		// -- hmm, is this true? Since the initial pass of Cake, which writes
		// makerules, still has to write makerules for *all* modules, 
		
		// initialize derivations
		//  -- we do this in a depthfirst post-order walk of the derivation tree
		// (NOTE: we might want to support a derivation DAG in future)
		// starting where? we need the derivation that has no parent
		set<string> depended_upon_module_names;
		set<string> derived_module_names;
		for (derivation_tbl_t::iterator pd = derivation_tbl.begin(); 
			pd != derivation_tbl.end(); ++pd)
		{
			derived_module_names.insert(pd->first);
			auto depended_upon = pd->second->dependencies();
			std::copy(depended_upon.begin(), depended_upon.end(), 
				std::inserter(depended_upon_module_names, depended_upon_module_names.begin()));
		}
		// the name of the starting derivation is the set difference 
		// derived_module_names - depended_upon_module_names
		// and should be unique!
		set<string> singleton_start_derivation;
		std::set_difference(derived_module_names.begin(), derived_module_names.end(),
		                    depended_upon_module_names.begin(), depended_upon_module_names.end(),
		                    std::inserter(singleton_start_derivation, 
		                        singleton_start_derivation.begin()));
		if (singleton_start_derivation.size() != 1) RAISE(ast, "no unique toplevel derivation");

		this->start_derivation = *singleton_start_derivation.begin();

		auto found_start_derivation = derivation_tbl.find(start_derivation);
		assert(found_start_derivation != derivation_tbl.end());
		
		recursively_init_derivations(found_start_derivation);
// 		
// 		for (derivation_tbl_t::iterator pd = derivation_tbl.begin(); 
// 			pd != derivation_tbl.end(); ++pd)
// 		{
// 			
// 			pd->second->init();
// 			//pd->second->fix_input_modules();
// 		}
	}
	
	void 
	request::recursively_init_derivations(
		derivation_tbl_t::iterator i_d
	)
	{
		auto dependencies = i_d->second->dependencies();
		for (auto i_dep = dependencies.begin(); i_dep != dependencies.end(); ++i_dep)
		{	
			derivation_tbl_t::iterator found = derivation_tbl.find(*i_dep);
			if (found != derivation_tbl.end())
			{
				recursively_init_derivations(found);
			}
			// regardless of what kind of module it is, 
			else
			{
				assert(module_tbl.find(*i_dep) != module_tbl.end());
				module_tbl[*i_dep]->updated_dwarf();
				// it's an existing module -- do nothing
			}
		}
		// regardless of whether we have dependencies, we initialise ourselves
		i_d->second->init();
		i_d->second->get_output_module()->updated_dwarf(); // HACK: this doesn't belong here
		
	}
	
	void request::depthFirst(antlr::tree::Tree *t)
	{
		unsigned childCount = GET_CHILD_COUNT(t);
		std::cerr 	<< "found a node with " << childCount << " children" << ", "
				 	<< "type: " << GET_TYPE(t) << std::endl;
		for (unsigned i = 0; i < GET_CHILD_COUNT(t); i++)
		{
			depthFirst(reinterpret_cast<antlr::tree::Tree*>(GET_CHILD(t, i)));
		}
		std::cerr 	<< "text: " << CCP(GET_TEXT(t)) << std::endl;
		
	}
	
	void request::write_makerules()
	{
		// output makerules for each derive
		for (derivation_tbl_t::iterator pd = derivation_tbl.begin(); 
			pd != derivation_tbl.end(); ++pd)
		{
			assert(*p_out);
			pd->second->write_makerules(*p_out);
		}
		
		// we also write *all* the makerules for generating header files here
		for (module_tbl_t::iterator i_mod = module_tbl.begin();
			i_mod != module_tbl.end();
			++i_mod)
		{
			*p_out << i_mod->second->get_filename() << ".hpp: "
				<< i_mod->second->get_filename() << " $(patsubst %,," << in_filename << ") # HACK: manually rebuild hpp for now, until it's faster" << endl
				<< "\t" << "$(CAKE) --make-module-headers " << i_mod->first
				<< " " << in_filename << " > $@" << endl;
		}
	}
	
	void request::write_headers(const string& module_name)
	{
		using dwarf::tool::dwarfidl_cxx_target;
		
		auto found = module_tbl.find(module_name);
		if (found == module_tbl.end()) RAISE(ast, "no such module");
		else 
		{
			// HACK: if it's a derivation, fix the module
			if (dynamic_pointer_cast<derived_module>(found->second))
			{
				auto found_deriv = derivation_tbl.find(module_name);
				assert(found_deriv != derivation_tbl.end());
				//found_deriv->second->fix_module();
			}
			
			string headers_out_filename = //outfile ? outfile : 
				(found->second->get_filename() + ".hpp");
			std::ofstream out(headers_out_filename.c_str());
			srk31::indenting_ostream indenting_out(out);
			dwarfidl_cxx_target headers_target(" ::cake::unspecified_wordsize_type",
				indenting_out, 
				dwarf::tool::cxx_compiler::default_compiler_argv(true)); // use CXXFLAGS
			
			headers_target.emit_all_decls(found->second->get_ds().toplevel());
		}
	}
	
	void request::do_derivation(const string& derived_module_name)
	{
		// output source code for a given derive
		auto found = derivation_tbl.find(derived_module_name); 
		if (found == derivation_tbl.end()) RAISE(ast, "no such derivation");
		else
		{
			//found->second->fix_module();
			found->second->write_cxx();
		}
	}

	void request::extract_exists()
	{	
		/* Process 'exists' */
		INIT;
		FOR_ALL_CHILDREN(ast)
		{	/* Find all toplevel exists definition */
			SELECT_ONLY(KEYWORD_EXISTS);
			add_exists(n);
		}
	}

	void request::extract_derivations() 
	{
		INIT;
		FOR_ALL_CHILDREN(this->ast)
		{
			SELECT_ONLY(CAKE_TOKEN(KEYWORD_DERIVE));
			add_derivation(n);
		}
	}
		
	void derivation::write_object_dependency_makerules(std::ostream& out)
	{
		/* First up, we write the makerule that says that we should re-run Cake
		 * with --derive-module. BUT what will that make? */
		out << get_emitted_sourcefile_name() << ": " << r.in_filename;
	
		//out << output_module->get_filename() << ":: ";
		for (vector<module_ptr>::iterator i = input_modules.begin();
			i != input_modules.end(); i++)
		{
			out << " " << (*i)->get_filename() << " " << (*i)->get_filename() << ".hpp";
		}
		out << endl << "\t" << "$(CAKE) --derive-module " 
			<< r.module_inverse_tbl[get_output_module()] << " "
			<< r.in_filename << endl;
		
		// Now we write the rule for the toplevel .o file that we're supposed to build
		out << output_module->get_filename() << ": ";
		for (vector<module_ptr>::iterator i = input_modules.begin();
			i != input_modules.end(); i++)
		{
			out << " " << (*i)->get_filename();
		}
		out << " "; // we leave the line unterminated, for additions in the subclass
	}
}
