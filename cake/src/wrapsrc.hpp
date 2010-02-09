#ifndef CAKE_WRAPSRC_HPP_
#define CAKE_WRAPSRC_HPP_

#include <sstream>
#include <map>
#include <vector>
#include <dwarfpp/encap.hpp>
#include <dwarfpp/cxx_compiler.hpp>

#include "link.hpp"

namespace cake
{
    class wrapper_file
    {
        dwarf::tool::cxx_compiler compiler;
        
		std::string function_header(
    		boost::optional<dwarf::abstract::Die_abstract_base<dwarf::encap::die>&> ret_type,
        	const std::string& function_name,
            dwarf::encap::die::formal_parameters_iterator args_begin,
            dwarf::encap::die::formal_parameters_iterator args_end,
            antlr::tree::Tree *source_pattern,
            dwarf::encap::Die_encap_subprogram& subprogram);
        
    public:
        wrapper_file() : compiler(std::vector<std::string>(1, std::string("g++"))) {}
        std::string emit_wrapper(const std::string& wrapped_symname, 
                link_derivation::wrapper_corresp_list& corresps);
    };


}

#endif
