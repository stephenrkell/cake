#include <iostream>
#include <vector>
#include <map>
#include <boost/shared_ptr.hpp>
#include <cassert>
#include "parser.hpp"
#include "request.hpp"
#include "link.hpp"

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
        derivation *pd = create_derivation(std::string(CCP(GET_TEXT(id))), 
        	unescaped_filename, derivedObjectExpression);
        derivation_tbl.push_back(
        	boost::shared_ptr<derivation>(pd)
            ); 
    }
    
    derived_module *request::create_derived_module(derivation& d, 
    	const std::string& id, const std::string& filename)
    {
    	return new derived_module(d, id, filename);
    }
    
	derivation *request::create_derivation(const std::string& module_name, 
		const std::string& output_filename, 
		antlr::tree::Tree *t)
	{
		switch(GET_TYPE(t))
		{
			case CAKE_TOKEN(IDENT): // unary predicates
				if (std::string(GET_TEXT(t)) == "instantiate")
				{
					return new instantiate_derivation(*this, t, module_name, output_filename);
				} 
				//else if (std::string(GET_TEXT(t)) == "make_exec")
				//{
				//
				//} break;
				//if 
				assert(false); return 0;
			case CAKE_TOKEN(KEYWORD_LINK): 
			{
				return new link_derivation(*this, t, module_name, output_filename);
			}
			default: return 0;
		}
	}
}
