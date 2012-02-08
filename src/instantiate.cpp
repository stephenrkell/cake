#include <iostream>
#include <vector>
#include <map>
#include <boost/filesystem/path.hpp>

#include "request.hpp"
#include "parser.hpp"
#include "instantiate.hpp"

using boost::make_shared;
using boost::dynamic_pointer_cast;
using boost::shared_ptr;
using std::endl;
using std::cerr;
using std::ostream;
using std::ostringstream;
using std::string;
using std::pair;
using std::make_pair;
using std::vector;
using std::map;

namespace cake
{
	instantiate_derivation::instantiate_derivation(
		request& r, 
		antlr::tree::Tree *t, 
		const string& module_name,
		const string& output_filename
		)
	 : derivation(r, t), 
	   output_namespace("instantiate_" + module_name + "_"), 
	   output_filename(output_filename),
	   output_cxx_filename(module_name + "_instantiate.cpp"),
	   output_cxx_file(new std::ofstream(output_cxx_filename))
	{
		/* Add a derived module to the module table, and keep the pointer. */
		assert(r.module_tbl.find(output_filename) == r.module_tbl.end());
		assert(!this->output_module);
		this->output_module = r.create_derived_module(*this, module_name, output_filename);
		r.module_tbl[module_name] = output_module;
		r.module_inverse_tbl[output_module] = module_name;

		// look up our input module	
		INIT;
		/* instantiate args: (component, structure type, obj name, symbol prefix) */
		BIND3(t, component, IDENT);
		BIND3(t, objtype, IDENT);
		BIND3(t, objname, IDENT);
		BIND3(t, prefix, STRING_LIT);

		auto found = r.module_tbl.find(CCP(GET_TEXT(component)));
		assert(found != r.module_tbl.end());
		input_modules.push_back(found->second);

		this->objtype = unescape_ident(CCP(GET_TEXT(objtype)));
		this->objname = unescape_ident(CCP(GET_TEXT(objname)));
		this->symbol_prefix = unescape_string_lit(CCP(GET_TEXT(prefix)));
		
		cerr << "instantiate " << module_name << ": "
			<< "output filename = " << output_filename << ", "
			<< "output cxx filename = " << output_cxx_filename << "." << endl;
	}
	void instantiate_derivation::extract_definition()
	{
		// don't think we need this
	}
	
	void instantiate_derivation::write_makerules(std::ostream& out)
	{
		/* Instantiate works by
		 * - generating a C++ file that 
		 *     - includes the .o.hpp file
		 *     - instantiates the named structure...
		 *     - ... having defined function prototypes for each fp element
		 *     - ... referencing those prototypes in the initializer
		 *     - ... and being compiled *without* debug info.
		 * We manually propagate debug info to the output module, 
		 * by setting its encap::dieset to equal that of our input module,
		 * plus some additions. */
		write_object_dependency_makerules(out);
		// this function --^ doesn't write a newline...
		// ... so add extra dependencies
		out << " $(patsubst %.cpp,%.o," << output_cxx_filename << ")";
		out << "\n\tld -r -o \"$@\" $+" << endl;

		out << "$(patsubst %.cpp,%.o," << output_cxx_filename << "): " 
			<< output_cxx_filename << " " << output_filename << ".hpp";
		out << "\n\t$(CXX) $(CXXFLAGS) -c -o \"$@\" \"$<\"" << endl;
		
		/* To be consistent with the linkage names,
		 * we should use the same namespacing conventions we use in
		 * link.cpp. */ 
		*output_cxx_file << "namespace cake { namespace " 
			<< r.module_inverse_tbl[*input_modules.begin()] << "{" << endl;
		*output_cxx_file << "#include \"" << (*input_modules.begin())->get_filename() << ".hpp"
			<< "\"" << endl << "} }" << endl;
		// now open our own namespace
		*output_cxx_file << "namespace cake { namespace " 
			<< r.module_inverse_tbl[output_module->shared_from_this()] << " "
			<< "{" << endl;
		// instantiate each function pointer in the named data type
		definite_member_name mn(1, objtype);
		auto named = (*input_modules.begin())->get_ds().toplevel()->visible_resolve(
			mn.begin(), mn.end());
		auto named_type = dynamic_pointer_cast<spec::type_die>(named);
		auto named_with_data_members = dynamic_pointer_cast<spec::with_data_members_die>(named_type);
		assert(named && named_type && named_with_data_members);
		
		// first define a couple of helper functions we'll use repeatedly
		auto memb_is_funcptr = [](spec::with_data_members_die::member_iterator i_memb)
		{
			return (*i_memb)->get_type() 
				&& (*i_memb)->get_type()->get_concrete_type()->get_tag() == DW_TAG_pointer_type
				&& dynamic_pointer_cast<spec::pointer_type_die>((*i_memb)->get_type()->get_concrete_type())
					->get_type()
				&& dynamic_pointer_cast<spec::pointer_type_die>((*i_memb)->get_type()->get_concrete_type())
					->get_type()->get_concrete_type()->get_tag() == DW_TAG_subroutine_type;
		};
		auto get_subroutine_type = [](
			spec::with_data_members_die::member_iterator i_memb
		)
		{
			return dynamic_pointer_cast<spec::subroutine_type_die>(
						dynamic_pointer_cast<spec::pointer_type_die>(
							(*i_memb)->get_type()->get_concrete_type())
							->get_type()->get_concrete_type()
							);
		};
		
		for (auto i_memb = named_with_data_members->member_children_begin(); 
			i_memb != named_with_data_members->member_children_end(); ++i_memb)
		{
			if (memb_is_funcptr(i_memb))
			{
				auto subt = get_subroutine_type(i_memb);
				
				assert(subt);
				assert((*i_memb)->get_name());
				*output_cxx_file << r.compiler.make_function_declaration_of_type(
					subt, 
					symbol_prefix + *(*i_memb)->get_name());
			}

		}
		// instantiate the named data type
		*output_cxx_file << "extern \"C\" {\n" // HACK
		<< " ::cake::" << r.module_inverse_tbl[*input_modules.begin()] << "::" << objtype
			<< " " << objname << " = {" << endl;
		for (auto i_memb = named_with_data_members->member_children_begin(); 
			i_memb != named_with_data_members->member_children_end(); ++i_memb)
		{
			assert((*i_memb)->get_name());
			if (i_memb != named_with_data_members->member_children_begin()) out << ", " << endl;
			*output_cxx_file << "\t/* ." << *(*i_memb)->get_name() << " = */ ";
			if (memb_is_funcptr(i_memb))
			{
				*output_cxx_file << "&" << symbol_prefix + *(*i_memb)->get_name();
			}
			else
			{
				// HACK: since C++ does not have designated initializers, 
				// we have to make a guess about what the default value of this member
				// should be.
				auto conc = (*i_memb)->get_type()->get_concrete_type();
				if (conc->get_tag() == DW_TAG_base_type
				||  conc->get_tag() == DW_TAG_pointer_type) *output_cxx_file << "0";
				else *output_cxx_file << "{}";
				
			}
		}
		*output_cxx_file << endl << "};" << endl;
		*output_cxx_file << "} // end extern \"C\"" << endl;
		
		// close the namespace and flush the file
		*output_cxx_file << "} }" << endl;
		output_cxx_file->flush();
		
		// write a rule that copies the input .o.hpp file to the output
		assert(srk31::count(input_modules.begin(), input_modules.end()) == 1);
		out << output_filename << ".hpp: " 
			<< (*input_modules.begin())->get_filename() << ".hpp"
			<< "\n\tcp \"$<\" \"$@\"" << endl;
		
		// create the state in the output module
		assert(dynamic_cast<encap::dieset&>((*input_modules.begin())->get_ds()).map_size() > 0);
		// 1. copy old DIEs
		dynamic_cast<encap::dieset&>(output_module->get_ds())
		 = dynamic_cast<encap::dieset&>((*input_modules.begin())->get_ds());
		dynamic_pointer_cast<derived_module>(output_module)->updated_dwarf();
		// 1a. sanity check
		unsigned new_size_pre = dynamic_cast<encap::dieset&>(output_module->get_ds()).map_size();
		unsigned old_size_pre = dynamic_cast<encap::dieset&>((*input_modules.begin())->get_ds()).map_size();
		assert(new_size_pre >= old_size_pre && "pre-copy size");
		// 2. create subprogram dies for each created subprogram
		// -- in a new compile_unit? YES, for now -- will need fixing up later
		auto created_cu = dynamic_pointer_cast<encap::compile_unit_die>(
			dwarf::encap::factory::for_spec(
				dwarf::spec::DEFAULT_DWARF_SPEC
			).create_die(DW_TAG_compile_unit,
				dynamic_pointer_cast<encap::basic_die>(output_module->get_ds().toplevel()),
				output_cxx_filename));
		created_cu->set_language(DW_LANG_C99); // lie!
		created_cu->set_producer(r.compiler.get_producer_string());
		
		for (auto i_memb = named_with_data_members->member_children_begin(); 
			i_memb != named_with_data_members->member_children_end(); ++i_memb)
		{
			if (memb_is_funcptr(i_memb))
			{
				auto subt = get_subroutine_type(i_memb);
				
				assert(subt);
				auto created_subprogram = dynamic_pointer_cast<encap::subprogram_die>(
					dwarf::encap::factory::for_spec(
						dwarf::spec::DEFAULT_DWARF_SPEC
					).create_die(DW_TAG_subprogram,
						dynamic_pointer_cast<encap::basic_die>(created_cu),
						symbol_prefix + *(*i_memb)->get_name()));
				for (auto i_fp = subt->formal_parameter_children_begin();
					i_fp != subt->formal_parameter_children_end(); ++i_fp)
				{
					auto created_fp = dynamic_pointer_cast<encap::formal_parameter_die>(
						dwarf::encap::factory::for_spec(
						dwarf::spec::DEFAULT_DWARF_SPEC
					).create_die(DW_TAG_formal_parameter,
						dynamic_pointer_cast<encap::basic_die>(created_subprogram),
						(*i_fp)->get_name()));
					created_fp->set_type((*i_fp)->get_type());
				}
				// FIXME: also do unspecified_parameters
			}
		}
		// 3. sanity check
		unsigned new_size_post = dynamic_cast<encap::dieset&>(output_module->get_ds()).map_size();
		unsigned old_size_post = dynamic_cast<encap::dieset&>((*input_modules.begin())->get_ds()).map_size();
		assert(new_size_post >= old_size_post
			&& "post-creation size");
		
		// sanity check
		cerr << "Copied the following data to the dieset of module " 
			<< r.module_inverse_tbl[this->output_module] << ":" << endl;
		cerr << output_module->get_ds();
	}
}
