#ifndef CAKE_LINK_HPP_
#define CAKE_LINK_HPP_

#include <fstream>
#include <set>
#include <dwarfpp/cxx_compiler.hpp>
//#include <boost/iterator/filter_iterator.hpp>
#include "request.hpp"
#include "parser.hpp"
#include "valconv.hpp"

namespace dwarf { namespace encap { typedef Die_encap_all_compile_units file_toplevel_die; } }

namespace cake {
	class link_derivation : public derivation
	{
		friend class wrapper_file;
        dwarf::tool::cxx_compiler compiler;
    public:
    	typedef std::pair<module_ptr,module_ptr> iface_pair;
        std::string name_of_module(module_ptr m) { return this->r.module_inverse_tbl[m]; }
        module_ptr module_for_dieset(dwarf::spec::abstract_dieset& ds)
        { 
            for (auto i_mod = r.module_tbl.begin(); i_mod != r.module_tbl.end(); i_mod++)
            {
            	if (&i_mod->second->get_ds() == &ds) return i_mod->second;
            }
            assert(false);
    	}
	private:
        typedef dwarf::encap::file_toplevel_die::subprograms_iterator
         subprograms_in_file_iterator;
        struct is_provided : public std::unary_function<subprograms_in_file_iterator, bool>
        {
            bool operator()(subprograms_in_file_iterator i_subp) const
            { return (!((*i_subp)->get_declaration()) || (!(*((*i_subp)->get_declaration())))); }
        };
        struct is_required : public std::unary_function<subprograms_in_file_iterator, bool>
        {
            bool operator()(subprograms_in_file_iterator i_subp) const
            { is_provided is_p; return !(is_p(i_subp)); }
        };
        typedef selective_iterator<subprograms_in_file_iterator, is_provided>
        	 provided_funcs_iter;
        typedef selective_iterator<subprograms_in_file_iterator, is_required>
        	 required_funcs_iter;

//         struct is_provided : public std::unary_function<dwarf::encap::subprogram_die, bool>
//         {
//             bool operator()(const dwarf::encap::subprogram_die *p_subp) const
//             { return (!(p_subp->get_declaration()) || (!(*(p_subp->get_declaration())))); }
//         };
//         struct is_required : public std::unary_function<dwarf::encap::subprogram_die, bool>
//         {
//             bool operator()(const dwarf::encap::subprogram_die *p_subp) const
//             { is_provided is_p; return !(is_p(p_subp)); }
//         };
//         typedef boost::filter_iterator<is_provided, dwarf::encap::subprograms_iterator>
//         	 provided_funcs_iter;
//         typedef boost::filter_iterator<is_required, dwarf::encap::subprograms_iterator>
//         	 required_funcs_iter;

        
        struct ev_corresp
        {
        	module_ptr source;
        	antlr::tree::Tree *source_pattern;
            antlr::tree::Tree *source_infix_stub;
            module_ptr sink;
            antlr::tree::Tree *sink_expr;
            antlr::tree::Tree *sink_infix_stub;
            
            // if we created a temporary AST, for implicit rules, free these ptrs
            antlr::tree::Tree *source_pattern_to_free; 
            antlr::tree::Tree *sink_pattern_to_free;
            ~ev_corresp() { // non-virtual to keep us POD / initializer-constructible etc.
				if (source_pattern_to_free && source_pattern_to_free->free) source_pattern_to_free->free(source_pattern_to_free);
            	if (sink_pattern_to_free && sink_pattern_to_free->free) sink_pattern_to_free->free(sink_pattern_to_free);
            }
        };
        
// 		struct basic_value_conversion
// 		{
//         	module_ptr source;
//         	boost::shared_ptr<dwarf::spec::type_die> source_data_type;
//         	antlr::tree::Tree *source_infix_stub;
//         	module_ptr sink;
//         	boost::shared_ptr<dwarf::spec::type_die> sink_data_type;
//         	antlr::tree::Tree *sink_infix_stub;
//         	antlr::tree::Tree *refinement;
// 			bool source_is_on_left;
//         	antlr::tree::Tree *corresp; // for generating errors
// 		}; 
		//typedef ::cake::basic_value_conversion basic_value_conversion;
        typedef value_conversion val_corresp;
    
    public:
    	static iface_pair sorted(iface_pair p) 
        { return p.first < p.second ? p : std::make_pair(p.second, p.first); }
        
        std::set<iface_pair> all_iface_pairs;
    
		typedef std::multimap<iface_pair, ev_corresp> ev_corresp_map_t;
        typedef std::multimap<iface_pair, boost::shared_ptr<val_corresp> > val_corresp_map_t;
        
        typedef ev_corresp_map_t::value_type ev_corresp_entry;
        typedef val_corresp_map_t::value_type val_corresp_entry;
        
        // List of pointer into an ev_corresp map.
        // Note: all pointers should all point into same map, and moreover,
        // should have the same first element (iface_pair).
        typedef std::vector<ev_corresp_map_t::value_type *> ev_corresp_pair_ptr_list;
        
        // Map from symbol name
        // to list of correspondences
        typedef std::map<std::string, ev_corresp_pair_ptr_list> wrappers_map_t;
		
		boost::optional<link_derivation::val_corresp_map_t::iterator>
		find_value_correspondence(
			module_ptr source, boost::shared_ptr<dwarf::spec::type_die> source_type,
			module_ptr sink, boost::shared_ptr<dwarf::spec::type_die> sink_type);
			
 		std::vector<boost::shared_ptr<dwarf::spec::type_die> >
		corresponding_dwarf_types(boost::shared_ptr<dwarf::spec::type_die> type,
			module_ptr corresp_module,
			bool flow_from_type_module_to_corresp_module);
		
    private:
    	// correspondences
    	ev_corresp_map_t ev_corresps;
        val_corresp_map_t val_corresps;
        
        // wrappers are stored in a map from symname to corresp-list
        // a corresp is in the list iff it activates the wrapper
        // i.e. its source event 
        wrappers_map_t wrappers;
        std::string output_namespace; // namespace in which code is emitted

        std::string wrap_file_makefile_name;
        std::string wrap_file_name;
        std::ofstream wrap_file;

        wrapper_file *p_wrap_code; // FIXME: this should be a contained subobject...
        wrapper_file& wrap_code;	// but is a pointer because of...
        	// stupid C++ inability to resolve circular include dependency
    
    protected:
    	// process a pairwise block
    	void add_corresps_from_block(module_ptr left,
        	module_ptr right, antlr::tree::Tree *corresps);
		// do name-matching
		void add_implicit_corresps(iface_pair ifaces);
        void name_match_required_and_provided(iface_pair ifaces,
        	module_ptr requiring_iface, module_ptr providing_iface);
		void extract_type_synonymy(module_ptr module,
			std::map<std::vector<std::string>, boost::shared_ptr<dwarf::spec::type_die> >& synonymy);
        void name_match_types(iface_pair ifaces);
        // utility for adding corresps
        void add_event_corresp(
        		module_ptr source, 
                antlr::tree::Tree *source_pattern,
                antlr::tree::Tree *source_infix_stub,
    	        module_ptr sink,
                antlr::tree::Tree *sink_expr,
                antlr::tree::Tree *sink_infix_stub,
                bool free_source = false,
                bool free_sink = false);
        void add_value_corresp(
        	module_ptr source, 
            boost::shared_ptr<dwarf::spec::type_die> source_data_type,
            antlr::tree::Tree *source_infix_stub,
            module_ptr sink,
            boost::shared_ptr<dwarf::spec::type_die> sink_data_type,
            antlr::tree::Tree *sink_infix_stub,
            antlr::tree::Tree *refinement,
			bool source_is_on_left,
            antlr::tree::Tree *corresp
        );
		void add_value_corresp(
        	module_ptr source, 
        	antlr::tree::Tree *source_data_type_mn,
        	antlr::tree::Tree *source_infix_stub,
        	module_ptr sink,
        	antlr::tree::Tree *sink_data_type_mn,
        	antlr::tree::Tree *sink_infix_stub,
        	antlr::tree::Tree *refinement,
			bool source_is_on_left,
        	antlr::tree::Tree *corresp
		);
		bool ensure_value_corresp(module_ptr source, 
			boost::shared_ptr<dwarf::spec::type_die> source_data_type,
        	module_ptr sink,
        	boost::shared_ptr<dwarf::spec::type_die> sink_data_type,
			bool source_is_on_left);
		void compute_wrappers();
        int module_tag(module_ptr module) { return reinterpret_cast<int>(module.get()); }
    /**** these are just notes-to-self ***/
//		void compute_rep_domains();
//		void output_rep_conversions();
		
//		void output_symbol_renaming_rules();
		
//		void output_formgens();		
//		void output_wrappergens();
    /*** end notes-to-self ***/
		
//		void output_static_co_objects(); 

	public:
		void write_makerules(std::ostream& out);	
		void extract_definition();
		std::vector<std::string> dependencies() { return std::vector<std::string>(); }
        link_derivation(cake::request& r, antlr::tree::Tree *t, 
        	const std::string& id, const std::string& output_filename);
        virtual ~link_derivation();
        const std::string& namespace_name() { return output_namespace; }
	};
}

#endif
