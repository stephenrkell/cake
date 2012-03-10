#ifndef CAKE_CXX_TARGET_HPP_
#define CAKE_CXX_TARGET_HPP_

#include <dwarfpp/cxx_model.hpp>
#include <dwarfpp/cxx_dependency_order.hpp>

namespace cake {

using std::string;
using std::vector;
using boost::shared_ptr;
using dwarf::spec::type_die;
using namespace dwarf;

class cake_cxx_target : public dwarf::tool::dwarfidl_cxx_target
{
	// forward the constructors
	// this doesn't work yet
	// using dwarf::tool::dwarfidl_cxx_target::dwarfidl_cxx_target;
public:
	template<typename... Args>
	//cake_headers_target(Args&&... args)
	cake_cxx_target(Args&&... args)
	 : dwarf::tool::dwarfidl_cxx_target(std::forward<Args>(args)...) {}

protected:
	shared_ptr<type_die>
	transform_type(
		shared_ptr<type_die> t,
		spec::abstract_dieset::iterator context
	)
	{
		/* If we're a member_die... */
		if ((*context)->get_tag() == DW_TAG_member)
		{
			/* ... strip off "const". To avoid missing consts buried behind volatile,
			 * we just get the unqualified type. NOTE: this doesn't deal with the 
			 * case where it's a typedef, and that typedef is const. But perhaps
			 * we can get away without that for now. */
			return t->get_unqualified_type();
		}
		else return t;
	}
	
//public:
//	//string get_untyped_argument_typename()
//	//{ return " ::cake::unspecified_wordsize_type"; }
//	cake_cxx_target() {} // use the default compiler args
};

} // end namespace cake

#endif
