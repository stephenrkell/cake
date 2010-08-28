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
    	const std::string& id,
        const std::string& output_module_filename) 
     : 	derivation(r, t), output_namespace("link_" + id + "_"), 
     	wrap_file_makefile_name(
        	boost::filesystem::path(id + "_wrap.cpp").string()),
     	wrap_file_name((boost::filesystem::path(r.in_filename).branch_path() 
            	/ wrap_file_makefile_name).string()),
     	wrap_file(wrap_file_name.c_str()),
     	p_wrap_code(new wrapper_file(*this, wrap_file)), wrap_code(*p_wrap_code)
    {
		/* Add a derived module to the module table, and keep the pointer. */
		assert(r.module_tbl.find(output_module_filename) == r.module_tbl.end());
        this->output_module = module_ptr(
        	r.create_derived_module(*this, id, output_module_filename));
        r.module_tbl[id] = output_module;
        r.module_inverse_tbl[output_module] = id;
        
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
                request::module_tbl_t::iterator found = r.module_tbl.find(
                	std::string(CCP(GET_TEXT(n))));
                if (found == r.module_tbl.end()) RAISE(n, "module not defined!");
                input_modules.push_back(found->second);
            }
            std::cerr << std::endl;
        }

		// enumerate all interface pairs
        for (std::vector<module_ptr>::iterator i_mod = input_modules.begin();
        		i_mod != input_modules.end();
                i_mod++)
        {
        	std::vector<module_ptr>::iterator copy_of_i_mod = i_mod;
        	for (std::vector<module_ptr>::iterator j_mod = ++copy_of_i_mod;
        		j_mod != input_modules.end();
                j_mod++)
			{
            	all_iface_pairs.insert(sorted(std::make_pair(*i_mod, *j_mod)));
            }
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
			
			// add implicit correpsondences *last*, s.t. explicit ones can take priority
			for (auto i_pair = all_iface_pairs.begin(); 
				i_pair != all_iface_pairs.end();
				i_pair++)
			{
				add_implicit_corresps(*i_pair);
			}
        }
        
        // remember each interface pair and add implicit corresps
        for (std::vector<module_ptr>::iterator i_mod = input_modules.begin();
        		i_mod != input_modules.end();
                i_mod++)
        {
        	std::vector<module_ptr>::iterator copy_of_i_mod = i_mod;
        	for (std::vector<module_ptr>::iterator j_mod = ++copy_of_i_mod;
        		j_mod != input_modules.end();
                j_mod++)
			{
            	all_iface_pairs.insert(sorted(std::make_pair(*i_mod, *j_mod)));
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
    
//     std::string link_derivation::namespace_name()
//     {
//     	std::ostringstream s;
//         s << "link_" << r.module_inverse_tbl[output_module] << '_';
//         return s.str();
// 	}

	void link_derivation::write_makerules(std::ostream& out)
	{
    	// implicit rule for making hpp files
        out << "%.o.hpp: %.o" << std::endl
        	<< '\t' << "dwarfhpp \"$<\" > \"$@\"" << std::endl;
        // dependencies for generated cpp file
        out << wrap_file_makefile_name << ".d: " << wrap_file_makefile_name << std::endl
        	<< '\t' << "g++ -MM -MG -I. -c \"$<\" > \"$@\"" << std::endl;
        out << "include " << wrap_file_makefile_name << ".d" << std::endl;

		out << output_module->get_filename() << ": ";
        for (std::vector<module_ptr>::iterator i = input_modules.begin();
        	i != input_modules.end(); i++)
        {
			out << (*i)->get_filename() << ' ';
        }
        
		// output the wrapper file header
        wrap_file << "// generated by Cake version " << CAKE_VERSION << std::endl;
        wrap_file << "#include <cake/prelude.hpp>" << std::endl;

        // for each component, include its dwarfpp header in its own namespace
        for (std::vector<module_ptr>::iterator i = input_modules.begin();
        		i != input_modules.end();
                i++)
        {
        	wrap_file << "namespace cake { namespace " << namespace_name()
            		<< " { namespace " << r.module_inverse_tbl[*i] << " {" << std::endl;
                    
            wrap_file << "\t#include \"" << (*i)->get_filename() << ".hpp\"" << std::endl;
            wrap_file << "\tclass marker {}; // used for per-component template specializations" 
                      << std::endl;
            wrap_file << "} } }" << std::endl; 

        }
        
        // for each pair of components, output the value conversions
        for (auto i_pair = all_iface_pairs.begin(); i_pair != all_iface_pairs.end();
        	i_pair++)
        {
			wrap_file << "namespace cake {" << std::endl;
			// emit each as a value_convert template
            auto all_value_corresps = val_corresps.equal_range(*i_pair);
            for (auto i_corresp = all_value_corresps.first;
            	i_corresp != all_value_corresps.second;
                i_corresp++)
            {
                auto opt_from_type = i_corresp->second.source->get_ds().toplevel()->visible_resolve(
                    i_corresp->second.source_data_type.begin(), i_corresp->second.source_data_type.end());
                auto opt_to_type = i_corresp->second.sink->get_ds().toplevel()->visible_resolve(
                    i_corresp->second.sink_data_type.begin(), i_corresp->second.sink_data_type.end());
                if (!opt_from_type) RAISE(i_corresp->second.corresp, 
                    "named source type does not exist");
                if (!opt_to_type) RAISE(i_corresp->second.corresp, 
                    "named sink type does not exist");
				auto p_from_type = boost::dynamic_pointer_cast<dwarf::spec::type_die>(opt_from_type);
				auto p_to_type = boost::dynamic_pointer_cast<dwarf::spec::type_die>(opt_to_type);
                if (!p_from_type) RAISE(i_corresp->second.corresp, 
                    "named source of value correspondence is not a DWARF type");
                if (!p_to_type) RAISE(i_corresp->second.corresp, 
                    "named target of value correspondence is not a DWARF type");
            	wrap_code.emit_value_conversion(
                	i_corresp->second.source,
            		p_from_type,
            		i_corresp->second.source_infix_stub,
            		i_corresp->second.sink,
            		p_to_type,
            		i_corresp->second.sink_infix_stub,
            		i_corresp->second.refinement,
					i_corresp->second.source_is_on_left,
					i_corresp->second.corresp);
        	}
			wrap_file << "} // end namespace cake" << std::endl;
            
            // now emit the component_pair specialisation which describes the rules
            // applying for this pair of components
        	wrap_file << "namespace cake {" << std::endl;
            wrap_file << "\ttemplate<> struct component_pair<" 
            	<< namespace_name() << "::" << r.module_inverse_tbl[i_pair->first]
                << "::marker, "
                << namespace_name() << "::" << r.module_inverse_tbl[i_pair->second]
                << "::marker> {" << std::endl;
            
            // FIXME: emit mapping
            wrap_file 
<< "        template <"
<< "            typename To,"
<< "            typename From = ::cake::unspecified_wordsize_type, "
<< "            int RuleTag = 0"
<< "        >"
<< "        static"
<< "        To"
<< "        value_convert_from_first_to_second(From arg)"
<< "        {"
<< "            return value_convert<From, "
<< "                To,"
<< "                RuleTag"
<< "                >().operator()(arg);"
<< "        }"
<< "        template <"
<< "            typename To,"
<< "            typename From = ::cake::unspecified_wordsize_type, "
<< "            int RuleTag = 0"
<< "        >"
<< "        static "
<< "        To"
<< "        value_convert_from_second_to_first(From arg)"
<< "        {"
<< "            return value_convert<From, "
<< "                To,"
<< "                RuleTag"
<< "                >().operator()(arg);"
<< "        }	"
<< std::endl;

			
			
            wrap_file << "\t};" << std::endl;
			wrap_file << "} // end namespace cake" << std::endl;
        }

		bool wrapped_some = false;
        std::ostringstream linker_args;
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
            for (ev_corresp_pair_ptr_list::iterator i_corresp_ptr = i_wrap->second.begin();
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
                
                /* If a given module both requires and provides the same symbol, i.e.
                 * if we are trying to interpose on an internal reference, we have to
                 * emit some extra make rules. FIXME: support this! */
                
            } // end for each corresp

            // (**WILL THIS WORK in ld?**)
            // It's an error to have some but not all sources specifyin an arg pattern.
            // FIXME: check for this

            if (!can_simply_rebind)
            {
            	wrapped_some = true;

        	    // tell the linker that we're wrapping this symbol
        	    linker_args << "--wrap " << i_wrap->first << ' ';
                
                // also tell the linker that unprefixed references to the symbol
                // should go to __real_<sym>
                // FIXME: can't do this as at the time of processing, there is no
                // symbol __real_<sym>. Instead must do as a separate stage,
                // e.g. when building executable.
                //linker_args << "--defsym " << i_wrap->first 
                //	<< '=' << "__real_" << i_wrap->first << ' ';

                /* Generate the wrapper */
                wrap_code.emit_wrapper(i_wrap->first, i_wrap->second, r.module_inverse_tbl);
            }
            else
            {
            	// don't emit wrapper, just use --defsym 
                if (!symname_bound_to || i_wrap->first != *symname_bound_to)
                {
                    linker_args << "--defsym " << i_wrap->first 
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
		} // end for each wrapper
        
        // Now output the linker args.
        // If wrapped some, first add the wrapper as a dependency (and an argument)
        if (wrapped_some)
        {
        	out << "$(patsubst %.cpp,%.o," << wrap_file_name << ") " /*<< std::endl*/;
	        out << std::endl << '\t' << "ld -r -o " << output_module->get_filename() << ' ';
            out << linker_args.str() << ' ';
        	out << "$(patsubst %.cpp,%.o," << wrap_file_name << ") " /*<< std::endl*/;
        }  // Else just output the args
        else out << std::endl << '\t' << "ld -r -o " << output_module->get_filename() << linker_args.str() << ' ';
        
        // add the other object files to the input file list
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
                                    BIND3(correspHead, sourceInfixStub, INFIX_STUB_EXPR);
                                    BIND3(correspHead, sinkInfixStub, INFIX_STUB_EXPR);
                                    BIND2(correspHead, sinkExpr);
                                	add_event_corresp(left, sourcePattern, sourceInfixStub,
                                    	right, sinkExpr, sinkInfixStub);
                                }
                            	break;
                            case CAKE_TOKEN(RL_DOUBLE_ARROW):
                            	// right is the source, left is the sink
                                {
                                	INIT;
                                	BIND2(correspHead, sinkExpr);
                                    BIND3(correspHead, sinkInfixStub, INFIX_STUB_EXPR);
                                    BIND3(correspHead, sourceInfixStub, INFIX_STUB_EXPR);
                                    BIND3(correspHead, sourcePattern, EVENT_PATTERN);
                                    add_event_corresp(right, sourcePattern, sourceInfixStub,
                                    	left, sinkExpr, sinkInfixStub);
                                }
                            	break;
                            case CAKE_TOKEN(BI_DOUBLE_ARROW):
                            	// add *two* correspondences
                                {
                                	INIT;
                                    BIND3(correspHead, leftPattern, EVENT_PATTERN);
                                    BIND3(correspHead, leftInfixStub, INFIX_STUB_EXPR);
                                    BIND3(correspHead, rightInfixStub, INFIX_STUB_EXPR);
                                    BIND3(correspHead, rightPattern, EVENT_PATTERN);
                                    add_event_corresp(left, leftPattern, leftInfixStub, 
                                        right, rightPattern, rightInfixStub);
                                    add_event_corresp(right, rightPattern, rightInfixStub,
                                        left, leftPattern, leftInfixStub);
	                            }
                            	break;
                            default: RAISE(correspHead, "expected a double-stemmed arrow");
                        }
					}                    
                	break;
                case CAKE_TOKEN(KEYWORD_VALUES):
                    {
                    	INIT;
                        BIND2(n, correspHead); // some kind of correspondence operator
                        std::string ruleName;
                        if (GET_CHILD_COUNT(n) > 1)
                        {
                        	BIND3(n, ruleNameIdent, IDENT);
                            ruleName = CCP(GET_TEXT(ruleNameIdent));
                        }
                        switch(GET_TYPE(correspHead))
                        {
                        	case CAKE_TOKEN(BI_DOUBLE_ARROW):
                            {
                                INIT;
                                /* The BI_DOUBLE_ARROW is special because 
                                  * - it might have multivalue children (for many-to-many)
                                  * - it should not have nonempty infix stubs
                                      (because these would be ambiguous) */
                                BIND2(correspHead, leftValDecl);
                                BIND3(correspHead, leftInfixStub, INFIX_STUB_EXPR);
                                BIND3(correspHead, rightInfixStub, INFIX_STUB_EXPR);
                                if (GET_CHILD_COUNT(leftInfixStub)
                                || GET_CHILD_COUNT(rightInfixStub) > 0)
                                {
                                    RAISE(correspHead, 
                                    "infix stubs are ambiguous for bidirectional value correspondences");
                                }
                                BIND2(correspHead, rightValDecl);
                                BIND3(correspHead, valueCorrespondenceRefinement, 
                                    VALUE_CORRESPONDENCE_REFINEMENT);
                                // we don't support many-to-many yet
                                assert(GET_TYPE(leftValDecl) != CAKE_TOKEN(MULTIVALUE)
                                && GET_TYPE(rightValDecl) != CAKE_TOKEN(MULTIVALUE));
                                ALIAS3(leftValDecl, leftMember, DEFINITE_MEMBER_NAME);
                                ALIAS3(rightValDecl, rightMember, DEFINITE_MEMBER_NAME);
                                // each add_value_corresp call denotes a 
                                // value conversion function that needs to be generated
                                add_value_corresp(left, leftMember, leftInfixStub,
                                    right, rightMember, rightInfixStub,
                                    valueCorrespondenceRefinement, true, correspHead);
                                add_value_corresp(right, rightMember, rightInfixStub,
                                    left, leftMember, leftInfixStub, 
                                    valueCorrespondenceRefinement, false, correspHead);

                            }
                            break;
                            case CAKE_TOKEN(LR_DOUBLE_ARROW):
                            case CAKE_TOKEN(RL_DOUBLE_ARROW):
                            case CAKE_TOKEN(LR_DOUBLE_ARROW_Q):
                            case CAKE_TOKEN(RL_DOUBLE_ARROW_Q):
                            {
                                INIT;
                                BIND2(correspHead, leftValDecl);
                                BIND3(correspHead, leftInfixStub, INFIX_STUB_EXPR);
                                BIND3(correspHead, rightInfixStub, INFIX_STUB_EXPR);
                                BIND2(correspHead, rightValDecl);
                                BIND3(correspHead, valueCorrespondenceRefinement, 
                                    VALUE_CORRESPONDENCE_REFINEMENT);
                                // many-to-many not allowed
                                if(!(GET_TYPE(leftValDecl) != CAKE_TOKEN(MULTIVALUE)
                                && GET_TYPE(rightValDecl) != CAKE_TOKEN(MULTIVALUE)))
                                { RAISE(correspHead, "many-to-many value correspondences must be bidirectional"); }
                                ALIAS3(leftValDecl, leftMember, DEFINITE_MEMBER_NAME);
                                ALIAS3(rightValDecl, rightMember, DEFINITE_MEMBER_NAME);
                                
                                // FIXME: pay attention to question marks
                                switch(GET_TYPE(correspHead))
                                {
                                	case CAKE_TOKEN(LR_DOUBLE_ARROW):
                                    case CAKE_TOKEN(LR_DOUBLE_ARROW_Q):
                                        add_value_corresp(left, leftMember, leftInfixStub,
                                            right, rightMember, rightInfixStub,
                                            valueCorrespondenceRefinement, true, correspHead);
                                        break;
                                    case CAKE_TOKEN(RL_DOUBLE_ARROW):
                                    case CAKE_TOKEN(RL_DOUBLE_ARROW_Q):
                                        add_value_corresp(right, rightMember, rightInfixStub,
                                            left, leftMember, leftInfixStub, 
                                            valueCorrespondenceRefinement, false, correspHead);
                                        break;
                                    default: assert(false);
                            	}
                            }
                            break;
                            default: assert(false);
                        }
                    }
                	break;
                default: RAISE(n, "expected an event correspondence or a value correspondence block");
            }
        }
    }
    
    void link_derivation::add_event_corresp(
    	module_ptr source, 
        antlr::tree::Tree *source_pattern,
        antlr::tree::Tree *source_infix_stub,
    	module_ptr sink,
        antlr::tree::Tree *sink_expr,
        antlr::tree::Tree *sink_infix_stub,
        bool free_source,
        bool free_sink)
    {
    	auto key = sorted(std::make_pair(source, sink));
        assert(all_iface_pairs.find(key) != all_iface_pairs.end());
    	ev_corresps.insert(std::make_pair(key, 
                    	(struct ev_corresp){ /*.source = */ source, // source is the *requirer*
                        	/*.source_pattern = */ source_pattern,
                            /*.source_infix_stub = */ source_infix_stub,
                            /*.sink = */ sink, // sink is the *provider*
                            /*.sink_expr = */ sink_expr,
                            /*.sink_infix_stub = */ sink_infix_stub,
                            /*.source_pattern_to_free = */ (free_source) ? source_pattern : 0,
                            /*.sink_pattern_to_free = */ (free_sink) ? sink_expr : 0 }));
    }

    void link_derivation::add_value_corresp(
        module_ptr source, 
        definite_member_name source_data_type,
        antlr::tree::Tree *source_infix_stub,
        module_ptr sink,
        definite_member_name sink_data_type,
        antlr::tree::Tree *sink_infix_stub,
        antlr::tree::Tree *refinement,
		bool source_is_on_left,
        antlr::tree::Tree *corresp
    )
    {
    	auto key = sorted(std::make_pair(source, sink));
        assert(all_iface_pairs.find(key) != all_iface_pairs.end());
    	val_corresps.insert(std::make_pair(key,
        				(struct val_corresp){ /* .source = */ source,
                        	/* .source_data_type = */ source_data_type,
                            /* .source_infix_stub = */ source_infix_stub,
            				/* .sink = */ sink,
                            /* .sink_data_type = */ sink_data_type,
                            /* .source_infix_stub = */ sink_infix_stub,
                            /* .refinement = */ refinement,
							/* .source_is_on_left = */ source_is_on_left,
                            /* .corresp = */ corresp }));
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
        
        // find DWARF types of matching name
        name_match_types(ifaces);
    }

    void link_derivation::name_match_required_and_provided(
    	iface_pair ifaces,
        module_ptr requiring_iface, 
        module_ptr providing_iface)
    {
    	assert((requiring_iface == ifaces.first && providing_iface == ifaces.second)
        	|| (requiring_iface == ifaces.second && providing_iface == ifaces.first));
            
    	/* Search dwarf info*/
        dwarf::encap::Die_encap_all_compile_units& requiring_info
        	= requiring_iface->all_compile_units();

        dwarf::encap::Die_encap_all_compile_units& providing_info
        	= providing_iface->all_compile_units();
            
        //auto r_test = requiring_info.subprograms_begin();
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
                        0, // no infix stub
    	                providing_iface, // sink is the provider
                        tmp_sink_pattern, 
                        0, // no infix stub
                        true, true);
                }
            }
        }
    }

    struct found_type 
    { 
        module_ptr module;
        boost::shared_ptr<dwarf::spec::type_die> t;
    };
    void link_derivation::name_match_types(
    	iface_pair ifaces)
    {
        std::multimap<std::vector<std::string>, found_type> found_types;
        std::set<std::vector<std::string> > keys;
        
        // traverse whole dieset depth-first, remembering DIEs that 
        // -- 1. are type DIEs, and
        // -- 2. have an ident path from root (i.e. have *names*)
        for (auto i_mod = ifaces.first;
        			i_mod;
                    i_mod = (i_mod == ifaces.first) ? ifaces.second : module_ptr())
        {
            for (auto i_die = i_mod->get_ds().begin();
        	    i_die != i_mod->get_ds().end();
                i_die++)
            {
        	    auto p_type = boost::dynamic_pointer_cast<dwarf::spec::type_die>(*i_die);
        	    if (!p_type) continue;

                auto opt_path = p_type->ident_path_from_cu();
                if (!opt_path) continue;

                found_types.insert(std::make_pair(*opt_path, (found_type){ i_mod, p_type }));
                keys.insert(*opt_path);
            }
    	}
        // now look for names that have exactly two entries in the multimap,
        // and where the module of each is different
        for (auto i_k = keys.begin(); i_k != keys.end(); i_k++)
        {
        	auto iter_pair = found_types.equal_range(*i_k);
            if (srk31::count(iter_pair.first, iter_pair.second) == 2
            && (iter_pair.second--, 
             iter_pair.first->second.module != iter_pair.second->second.module))
            {
            	std::cerr << "data type " << definite_member_name(*i_k)
                	<< " exists in both modules" << std::endl;
            	// iter_pair points to a pair of like-named types in differing modules
                // add value correspondences in *both* directions
                // *** FIXME: ONLY IF not already present?
                add_value_corresp(
                	iter_pair.first->second.module,
                    *i_k, //iter_pair.first->second.module->get_ds().toplevel()->visible_resolve(i_k->begin(), i_k->end()),
                    0,
                    iter_pair.second->second.module,
                    *i_k, // iter_pair.second->second.module->get_ds().toplevel()->visible_resolve(i_k->begin(), i_k->end()),
                    0,
                    0, // refinement
					true, // source_is_on_left -- irrelevant as we have no refinement
                    0 // corresp
                );
                add_value_corresp(
                	iter_pair.second->second.module,
                    *i_k, //iter_pair.second->second.module->get_ds().toplevel()->visible_resolve(i_k->begin(), i_k->end()),
                    0,
                    iter_pair.first->second.module,
                    *i_k, //iter_pair.first->second.module->get_ds().toplevel()->visible_resolve(i_k->begin(), i_k->end()),
                    0,
                    0, // refinement
					false, // source_is_on_left -- irrelevant as we have no refinement
					0 // corresp
                );
            }
            else std::cerr << "data type " << definite_member_name(*i_k)
                	<< " exists only in one module" << std::endl;
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
