// #include <gcj/cni.h>
// #include <org/antlr/runtime/tree/Tree.h>
// #include <org/antlr/runtime/tree/CommonTree.h>
// #include <cake/cakeJavaLexer.h>
// #include <cake/cakeJavaParser.h>
// #include <cake/SemanticError.h>
#include <iostream>
#include <vector>
#include <map>
#include <boost/filesystem/path.hpp>

#include "request.hpp"
#include "parser.hpp"
#include "link.hpp"
#include "wrapsrc.hpp"

namespace cake
{
	link_derivation::link_derivation(cake::request& r, antlr::tree::Tree *t,
    	std::string& output_module_filename) 
     : 	derivation(r, t), p_wrap_code(new wrapper_file()), wrap_code(*p_wrap_code),
     	wrap_file((boost::filesystem::path(r.in_filename).branch_path() 
            	/ boost::filesystem::path(output_module_filename + "_wrap.cpp")).string().c_str())
    {
		/* Add a derived module to the module table, and keep the pointer. */
		assert(r.module_tbl.find(output_module_filename) == r.module_tbl.end());
        this->output_module = module_ptr(
        	r.create_derived_module(*this, output_module_filename));
        r.module_tbl[output_module_filename] = output_module;
        
    	assert(GET_TYPE(t) == CAKE_TOKEN(KEYWORD_LINK));
        INIT;
        BIND3(t, identList, IDENT_LIST);
        BIND3(t, linkRefinement, PAIRWISE_BLOCK_LIST);
        
        {
        	INIT;
            std::cerr << "Link expression at " << t << " links modules: ";
            FOR_ALL_CHILDREN(identList)
            {
            	std::cerr << CCP(GET_TEXT(n)) << " ";
                request::module_tbl_t::iterator found = r.module_tbl.find(std::string(CCP(GET_TEXT(n))));
                if (found == r.module_tbl.end()) RAISE(n, "module not defined!");
                input_modules.push_back(found->second);
            }
            std::cerr << std::endl;
        }
        
        {
        	INIT;
            std::cerr << "Link expression at " << t << " has pairwise blocks as follows: ";
            FOR_ALL_CHILDREN(linkRefinement)
            {
            	INIT;
                assert(GET_TYPE(linkRefinement) == CAKE_TOKEN(PAIRWISE_BLOCK_LIST));
            	ALIAS3(n, arrow, BI_DOUBLE_ARROW);
                // now walk each pairwise block and add the correspondences
	            {
                	INIT;
                	BIND3(arrow, leftChild, IDENT);
                    BIND3(arrow, rightChild, IDENT);
                    BIND3(arrow, pairwiseCorrespondenceBody, CORRESP);
	                std::cerr << CCP(GET_TEXT(leftChild))
                		<< " <--> "
                        << CCP(GET_TEXT(rightChild));
	                request::module_tbl_t::iterator found_left = 
                    	r.module_tbl.find(std::string(CCP(GET_TEXT(leftChild))));
	                if (found_left == r.module_tbl.end()) RAISE(n, "module not defined!");
   	                request::module_tbl_t::iterator found_right = 
                    	r.module_tbl.find(std::string(CCP(GET_TEXT(rightChild))));
	                if (found_right == r.module_tbl.end()) RAISE(n, "module not defined!");
                    
                    add_corresps_from_block(
                    	found_left->second, 
                        found_right->second,
                        pairwiseCorrespondenceBody);
                }
            }
			std::cerr << std::endl;
        }
        
        // add implicit corresps for each pair
        for (std::vector<module_ptr>::iterator i_mod = input_modules.begin();
        		i_mod != input_modules.end();
                i_mod++)
        {
        	std::vector<module_ptr>::iterator copy_of_i_mod = i_mod;
        	for (std::vector<module_ptr>::iterator j_mod = ++copy_of_i_mod;
        		j_mod != input_modules.end();
                j_mod++)
			{
		        add_implicit_corresps(std::make_pair(*i_mod, *j_mod));
            }
        }
        
        // generate wrappers
        compute_wrappers();
    }
    
    link_derivation::~link_derivation() 
    { wrap_file.flush(); wrap_file.close(); delete p_wrap_code; }

	void link_derivation::extract_definition()
    {
    
	}

	void link_derivation::write_makerules(std::ostream& out)
	{
    	out << output_module->get_filename() << ": ";

        for (std::vector<module_ptr>::iterator i = input_modules.begin();
        	i != input_modules.end(); i++)
        {
			out << (*i)->get_filename();
            out << ' ';
        }
        out << std::endl << '\t' << "ld -r -o " << output_module->get_filename() << ' ';

        // output wrapped symbol names (and the wrappers, to a separate file)
        for (wrappers_map_t::iterator i_wrap = wrappers.begin(); i_wrap != wrappers.end();
        		i_wrap++)
        {
        	// i_wrap->first is the wrapped symbol name
            // i_wrap->second is a list of pointer to the event correspondences
            //   whose *source* pattern invokes that symbol
            
            // ... however, if we have no source specifying an argument pattern,
            // don't emit a wrapper (because we can't provide args to invoke the __real_ function),
            // just --defsym __wrap_ = __real_ (i.e. undo the wrapping)
            bool can_simply_rebind = true;
            boost::optional<std::string> symname_bound_to;
            for (wrapper_corresp_list::iterator i_corresp_ptr = i_wrap->second.begin();
            			i_corresp_ptr != i_wrap->second.end();
                        i_corresp_ptr++)
            {
            	// we can only simply rebind if our symbol is only invoked
                // with simple name-only patterns mapped to an argument-free
                // simple expression.
                boost::optional<std::string> source_symname =
                	source_pattern_is_simple_function_name((*i_corresp_ptr)->second.source_pattern);
                boost::optional<std::string> sink_symname =
                	sink_expr_is_simple_function_name((*i_corresp_ptr)->second.sink_expr);
                if (source_symname) assert(source_symname == i_wrap->first);
                else 
                {
                	std::cerr << "Detected that required symbol " << i_wrap->first
                    	<< " is not a simple rebinding of a required symbol "
                        << " because source pattern "
                        << CCP(TO_STRING_TREE((*i_corresp_ptr)->second.source_pattern))
                        << " is not a simple function name." << std::endl;
                	can_simply_rebind = false;
                }
                if (sink_symname)
                {
                	if (symname_bound_to) RAISE((*i_corresp_ptr)->second.sink_expr,
                    	"previous event correspondence has bound caller to different symname");
                    else symname_bound_to = sink_symname;
				}
                else 
                {
                	std::cerr << "Detected that required symbol " << i_wrap->first
                    	<< " is not a simple rebinding of a required symbol "
                        << " because sink pattern "
                        << CCP(TO_STRING_TREE((*i_corresp_ptr)->second.sink_expr))
                        << " is not a simple function name." << std::endl;                
    	            can_simply_rebind = false;
	            }
            }

            // (**WILL THIS WORK in ld?**)
            // It's an error to have some but not all sources specifyin an arg pattern.
            // FIXME: check for this

            if (!can_simply_rebind)
            {
        	    // tell the linker that we're wrapping this symbol
        	    out << "--wrap " << i_wrap->first << ' ';

                /* Generate the wrapper */
                std::string wrapper_code = wrap_code.emit_wrapper(i_wrap->first, i_wrap->second);
                assert(wrapper_code.size() > 0);
	            wrap_file << wrapper_code;
                std::cerr << "Generated wrapper code " << wrapper_code << std::endl;
            }
            else
            {
            	// don't emit wrapper, just use --defsym 
                if (!symname_bound_to || i_wrap->first != *symname_bound_to)
                {
                    out << "--defsym " << i_wrap->first 
                	    << '='
                        // FIXME: if the callee symname is the same as the wrapped symbol,
                        // we should use __real_; otherwise we should use
                        // the plain old callee symbol name. 
                        /*<< "__real_"*/ << *symname_bound_to //i_wrap->first
                        << ' ';
                }
                else { /* do nothing! */ }
                
            }
            wrap_file.flush();
		}

        for (std::vector<module_ptr>::iterator i = input_modules.begin();
        	i != input_modules.end(); i++)
        {
			out << (*i)->get_filename() << ' ';
        }
        out << std::endl;
        

// 		// if it's a link:
// 		compute_function_bindings(); // event correspondences: which calls should be bound (possibly indirectly) to which definitions?
// 		
// 		compute_form_value_correspondences(); 
// 			// use structural + lossless rules, plus provided correspondences
// 			// not just the renaming-only correspondences!
// 			// since other provided correspondences (e.g. use of "as") might bring compatibility "back"
// 			// in other words we need to traverse *all* correspondences, forming implicit ones by name-equiv
// 			// How do we enumerate all correspondences? need a starting set + transitive closure
// 			// starting set is set of types passed in bound functions?
// 			// *** no, just walk through the DWARF info -- it's finite. add explicit ones first?
// 			
// 			// in the future, XSLT-style notion of correspondences, a form correspondence can be
// 			// predicated on a context... is this sufficient expressivity for
// 			// 1:N, N:1 and N:M object identity conversions? we'll need to tweak the notion of
// 			// co-object relation also, clearly
// 
// 		compute_static_value_correspondences();
// 			// -- note that when we support stubs and sequences, they will imply some nontrivial
// 			// value correspondences between function values. For now it's fairly simple 
// 			// -- wrapped if not in same rep domain, else not wrapped
// 		
// 		compute_dwarf_type_compatibility(); 
// 			// of all the pairs of corresponding forms, which are binary-compatible?
// 		
// 		compute_rep_domains(); // graph colouring ahoy
// 		
// 		output_rep_conversions(); // for the non-compatible types that need to be passed
// 		
// 		compute_interposition_points(); // which functions need wrapping between which domains
// 		
// 		output_symbol_renaming_rules(); // how to implement the above for static references, using objcopy and ld options
// 		
// 		output_formgens(); // one per rep domain
// 		
// 		output_wrappergens(); // one per inter-rep call- or return-direction, i.e. some multiple of 2!
// 		
// 		output_static_co_objects(); // function and global references may be passed dynamically, 
// 			// as may stubs and wrappers, so the co-object relation needs to contain them.
// 			// note that I really mean "references to statically-created objects", **but** actually
// 			// globals need not be treated any differently, unless there are two pre-existing static
// 			// obejcts we want to correspond, or unless they should be treated differently than their
// 			// DWARF-implied form would entail. 
// 			// We treat functions specially so that we don't have to generate rep-conversion functions
// 			// for functions -- we expand all the rep-conversion ahead-of-time by generating wrappers.
// 			// This works because functions are only ever passed by reference (identity). If we could
// 			// construct functions at run-time, things would be different!
// 			
// 		// output_stubs(); -- compile stubs from stub language expressions
		
	}
    
    void link_derivation::add_corresps_from_block(
    	module_ptr left,
        module_ptr right,
        antlr::tree::Tree *corresps)
    {
    	assert(GET_TYPE(corresps) == CAKE_TOKEN(CORRESP));
        INIT;
        FOR_ALL_CHILDREN(corresps)
        {
        	switch(GET_TYPE(n))
            {
            	case CAKE_TOKEN(EVENT_CORRESP):
                    {
                    	INIT;
                        BIND2(n, correspHead);
                        switch (GET_TYPE(correspHead))
                        {
                        	case CAKE_TOKEN(LR_DOUBLE_ARROW):
                            	// left is the source, right is the sink
                                {
                                	INIT;
                                	BIND3(correspHead, sourcePattern, EVENT_PATTERN);
                                    BIND2(correspHead, sinkExpr);
                                	add_event_corresp(left, sourcePattern,
                                    	right, sinkExpr);
                                }
                            	break;
                            case CAKE_TOKEN(RL_DOUBLE_ARROW):
                            	// right is the source, left is the sink
                                {
                                	INIT;
                                	BIND2(correspHead, sinkExpr);
                                    BIND3(correspHead, sourcePattern, EVENT_PATTERN);
                                    add_event_corresp(right, sourcePattern,
                                    	left, sinkExpr);
                                }
                            	break;
                            case CAKE_TOKEN(BI_DOUBLE_ARROW):
                            	// add *two* correspondences
                                {
                                	INIT;
                                    BIND3(correspHead, leftPattern, EVENT_PATTERN);
                                    BIND3(correspHead, rightPattern, EVENT_PATTERN);
                                    add_event_corresp(left, leftPattern, right, rightPattern);
                                    add_event_corresp(right, rightPattern, left, leftPattern);
	                            }
                            	break;
                            default: RAISE(correspHead, "expected a double-stemmed arrow");
                        }
					}                    
                	break;
                case CAKE_TOKEN(KEYWORD_VALUES):
                	assert(false);
                	break;
                default: RAISE(n, "expected an event correspondence or a value correspondence block");
            }
        }
    }
    
    void link_derivation::add_event_corresp(module_ptr source, antlr::tree::Tree *source_pattern,
    	module_ptr sink,
        antlr::tree::Tree *sink_expr,
        bool free_source,
        bool free_sink)
    {
    	ev_corresps.insert(std::make_pair(sorted(std::make_pair(source, sink)), 
                    	(struct ev_corresp){ /*.source = */ source, // source is the *requirer*
                        	/*.source_pattern = */ source_pattern,
                            /*.is_bidi = */ false,
                            /*.sink = */ sink, // sink is the *provider*
                            /*.sink_expr = */ sink_expr,
                            /*.source_pattern_to_free = */ (free_source) ? source_pattern : 0,
                            /*.sink_pattern_to_free = */ (free_sink) ? sink_expr : 0 }));
    }
    
    // Get the names of all functions provided by iface1
    void link_derivation::add_implicit_corresps(iface_pair ifaces)
    {
		std::cerr << "Adding implicit correspondences between module " 
        	<< ifaces.first->get_filename() << " and " << ifaces.second->get_filename()
            << std::endl;
          
        // find functions required by iface1 and provided by iface2
		name_match_required_and_provided(ifaces, ifaces.first, ifaces.second);
        // find functions required by iface2 and provided by iface1
		name_match_required_and_provided(ifaces, ifaces.second, ifaces.first);
    }

    void link_derivation::name_match_required_and_provided(
    	iface_pair ifaces,
        module_ptr requiring_iface, 
        module_ptr providing_iface)
    {
    	assert((requiring_iface == ifaces.first && providing_iface == ifaces.second)
        	|| (requiring_iface == ifaces.second && providing_iface == ifaces.first));
            
    	/* Search dwarf info*/
        dwarf::encap::Die_encap_all_compile_units requiring_info
        	= requiring_iface->all_compile_units();

        dwarf::encap::Die_encap_all_compile_units providing_info
        	= providing_iface->all_compile_units();
            
    	required_funcs_iter r_iter(
        	requiring_info.subprograms_begin(), requiring_info.subprograms_end());
    	required_funcs_iter r_end(
        	requiring_info.subprograms_begin(), requiring_info.subprograms_end(),
            requiring_info.subprograms_end());
        provided_funcs_iter p_iter(
        	providing_info.subprograms_begin(), providing_info.subprograms_end());
        provided_funcs_iter p_end(
        	providing_info.subprograms_begin(), providing_info.subprograms_end(),
            providing_info.subprograms_end());
    
		for ( ; r_iter != r_end; r_iter++)
        {        
        	std::cerr << "Found a required subprogram!" << std::endl;
            std::cerr << **r_iter;
            
            for ( ; p_iter != p_end; p_iter++) 
	        {
            	if ((*r_iter)->get_name() == (*p_iter)->get_name())
                {
                	// add a correspondence
                    std::cerr << "Matched name " << *((*r_iter)->get_name())
                    	<< " in modules " << *((*r_iter)->parent().get_name())
                        << " and " << *((*p_iter)->parent().get_name())
                        << std::endl;

					antlr::tree::Tree *tmp_source_pattern = 
                    	make_simple_event_pattern_for_call_site(
                        	*((*r_iter)->get_name()));
					antlr::tree::Tree *tmp_sink_pattern = 
                    	make_simple_sink_expression_for_event_pattern(
                        	std::string(*((*p_iter)->get_name())) + std::string("(...)"));

					add_event_corresp(requiring_iface, // source is the requirer 
                    	tmp_source_pattern,
    	                providing_iface, // sink is the provider
                        tmp_sink_pattern, 
                        true, true);
                }
            }
        }
    }
	
	void link_derivation::compute_wrappers() 
    {
    	/* We need to synthesise a set of wrapper from our event correspondences. 
         * For now, we generate a wrapper for every call-site calling a corresponding
         * function. We may need to generate additional wrappers for address-taken
         * functions.
         *
         * Wrappers need to be nonempty if direct oblivious binding is no good.
         * Direct oblivious binding is good iff 
         *    the corresponding symbol exists in the sink component
         * for all possible calling components,
         *    the called symbol name matches (or the call is indirect => has no symbol name); and
         *    after argument renaming/reordering/rebinding,
         *        all arguments are rep-compatible and definitely shareable
         * (note this is also the rep-compatibility test for functions-as-objects).
         */
         
        // For each call-site (required function) in one of our correspondences,
        // generate a wrapper. The same wrapper may have many relevant rules.
        
        for (ev_corresp_map_t::iterator i_ev = ev_corresps.begin();
        		i_ev != ev_corresps.end(); i_ev++)
        {
        	std::string called_function_name = get_event_pattern_call_site_name(
            	i_ev->second.source_pattern);
            
            std::cerr << "Function " << called_function_name << " may be wrapped!" << std::endl;
            wrappers[called_function_name].push_back(
            		&(*i_ev) // push a pointer to the ev_corresp map entry
            	);
        }
    }

//	void link_derivation::output_rep_conversions() {}		
		
//	void link_derivation::output_symbol_renaming_rules() {}		
//	void link_derivation::output_formgens() {}
		
//	void link_derivation::output_wrappergens() {}		
//	void link_derivation::output_static_co_objects() {}	
}
