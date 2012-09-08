#include <iostream>
#include <vector>
#include <map>
#include <memory>
#include <cassert>
#include "parser.hpp"
#include "request.hpp"
#include "link.hpp"
#include "instantiate.hpp"

namespace cake
{
	/* This is called when adding a derivation defined by an "exists" ... "deriving"
     * construct. */
	void request::add_derive_rewrite(std::string& derived_ident,
		std::string& filename_text,
		antlr::tree::Tree *derive_body) 
	{
		std::cout << "Asked to derive a module name " << derived_ident
			<< " with filename " << filename_text 
            << " defined by tree " << CCP(TO_STRING_TREE(derive_body)) << std::endl;
	}
    
    void request::add_derivation(antlr::tree::Tree *n)
    {
        INIT;
		BIND3(n, objectConstructor, OBJECT_CONSTRUCTOR);
		BIND3(n, id, IDENT);
        BIND2(n, derivedObjectExpression); 

		std::pair<std::string, std::string> ocStrings =
			read_object_constructor(objectConstructor);
		std::string ident(CCP(GET_TEXT(id)));

		std::cerr << "Asked to derive a module, constructor name " << ocStrings.first
		<< " with quoted filename " << ocStrings.second
		<< ", module identifier " << id 
        << ", derivation expression " 
        << CCP(TO_STRING_TREE(derivedObjectExpression)) << std::endl;

		/* Add a derivation to the derivation table. */
		std::string unescaped_filename = unescape_string_lit(ocStrings.second);

		// creating the derivation will create the output module
		string module_name = CCP(GET_TEXT(id));
		shared_ptr<derivation> pd = create_derivation(module_name, 
			unescaped_filename, derivedObjectExpression);
		derivation_tbl.insert(make_pair(
				module_name,
				pd
			)
		); 
	}

	shared_ptr<derived_module> request::create_derived_module(derivation& d, 
		const std::string& id, const std::string& filename)
	{
		auto made = std::make_shared<derived_module>(d, id, filename);
		assert(made->shared_from_this());
		return made;
	}

	shared_ptr<derivation> request::create_derivation(const std::string& module_name, 
		const std::string& output_filename, 
		antlr::tree::Tree *t)
	{
		switch(GET_TYPE(t))
		{
			case CAKE_TOKEN(IDENT): // unary predicates
				if (std::string(CCP(GET_TEXT(t))) == "instantiate")
				{
					return std::make_shared<instantiate_derivation>(
						*this, t, module_name, output_filename);
				} 
				//else if (std::string(GET_TEXT(t)) == "make_exec")
				//{
				//
				//} break;
				//if 
				assert(false); return shared_ptr<derivation>();
			case CAKE_TOKEN(KEYWORD_LINK): 
			{
				return std::make_shared<link_derivation>(
					*this, t, module_name, output_filename);
			}
			default: return shared_ptr<derivation>();
		}
	}
	
// 	void derivation::fix_input_modules()
// 	{
// 		for (auto i_input = input_modules.begin();
// 			i_input != input_modules.end();
// 			++i_input)
// 		{
// 			(*i_input)->updated_dwarf(); // set greatest_preexisting_offset
// 			if (dynamic_pointer_cast<derived_module>(*i_input))
// 			{
// 				auto found_derived_module
// 				 = r.module_inverse_tbl.find(*i_input);
// 				assert(found_derived_module != r.module_inverse_tbl.end());
// 				auto found_input_derivation = r.derivation_tbl.find(
// 					found_derived_module->second);
// 
// 				//std::find_if(
// 				//	derivation_tbl.begin(),
// 				//	derivation_tbl.end()
// 				//	[found_derived_module](const pair<string, shared_ptr<derivation> >& p)
// 				//	{ return p.second == found_derived_module->first find(
// 				//	found_derived_module->first);
// 				if (found_input_derivation != r.derivation_tbl.end())
// 				{
// 					found_input_derivation->second->fix_input_modules(); // recurse
// 					found_input_derivation->second->fix_module();
// 				}
// 			}
// 			//cerr << "In derivation building " 
// 			//	<< this->get_emitted_sourcefile_name()
// 			//	<< " input module " << r.module_inverse_tbl[*i_input]
// 			//	<< " has been fixed with contents as follows." << endl
// 			//	<< (*i_input)->get_ds();
// 		}
// 	}
}
