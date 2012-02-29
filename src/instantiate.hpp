#ifndef CAKE_INSTANTIATE_HPP_
#define CAKE_INSTANTIATE_HPP_

#include <fstream>
#include <dwarfpp/cxx_model.hpp>
#include "request.hpp"
#include "parser.hpp"

namespace cake {
	using std::string;
	using std::cerr;
	using std::endl;
	using std::pair;
	using std::make_pair;
	using std::vector;
	using boost::dynamic_pointer_cast;
	using boost::shared_ptr;
	using boost::optional;
	using namespace dwarf;
	using dwarf::spec::type_die;
	
	class instantiate_derivation : public derivation
	{
		string output_namespace;
		string output_filename;
		string objtype;
		string objname;
		string symbol_prefix;
		string output_cxx_filename;
		//std::ofstream *output_cxx_file; // this is now local
		
		// helpers
		shared_ptr<spec::subroutine_type_die>
		get_subroutine_type(
			spec::with_data_members_die::member_iterator i_memb
		);
		bool memb_is_funcptr(spec::with_data_members_die::member_iterator i_memb);
	protected:
		string get_emitted_sourcefile_name();
	public:
		instantiate_derivation(
			request& r, 
			antlr::tree::Tree *t, 
			const string& module_name,
			const string& output_filename
			);
		void init();
		void write_makerules(std::ostream& out);
		void write_cxx();
		const string& namespace_name() { return output_namespace; }
		vector<string> dependencies() { return vector<string>(); }

	};
	
} // end namespace cake

#endif
