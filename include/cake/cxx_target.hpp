#ifndef CAKE_CXX_TARGET_HPP_
#define CAKE_CXX_TARGET_HPP_

#include <dwarfpp/cxx_model.hpp>

namespace cake {

using std::string;
using std::vector;

class cake_cxx_target : public dwarf::tool::cxx_target
{
public:
	string get_untyped_argument_typename()
	{ return " ::cake::unspecified_wordsize_type"; }
	cake_cxx_target() : cxx_target(vector<string>(1, string("g++"))) {}
};

} // end namespace cake

#endif
