#ifndef CAKE_LINK_HPP_
#define CAKE_LINK_HPP_

#include <fstream>

#include "request.hpp"
#include "parser.hpp"

namespace cake {
	class link_derivation : public derivation
	{
    	typedef std::pair<module_ptr,module_ptr> iface_pair;
        struct is_provided : public std::unary_function<dwarf::encap::die::subprograms_iterator, bool>
        {
            bool operator()(dwarf::encap::die::subprograms_iterator i_subp) const
            { return (!((*i_subp)->get_declaration()) || (!(*((*i_subp)->get_declaration())))); }
        };
        struct is_required : public std::unary_function<dwarf::encap::die::subprograms_iterator, bool>
        {
            bool operator()(dwarf::encap::die::subprograms_iterator i_subp) const
            { is_provided is_p; return !(is_p(i_subp)); }
        };
        typedef selective_iterator<dwarf::encap::die::subprograms_iterator, is_provided>
        	 provided_funcs_iter;
        typedef selective_iterator<dwarf::encap::die::subprograms_iterator, is_required>
        	 required_funcs_iter;
        
        struct ev_corresp
        {
        	module_ptr source;
        	antlr::tree::Tree *source_pattern;
            bool is_bidi; // remove in favour of two separate correspondences?
            module_ptr sink;
            antlr::tree::Tree *sink_expr;
            
            // if we created a temporary AST, for implicit rules, free these ptrs
            antlr::tree::Tree *source_pattern_to_free; 
            antlr::tree::Tree *sink_pattern_to_free;
            ~ev_corresp() { // non-virtual to keep us POD
				if (source_pattern_to_free && source_pattern_to_free->free) source_pattern_to_free->free(source_pattern_to_free);
            	if (sink_pattern_to_free && sink_pattern_to_free->free) sink_pattern_to_free->free(sink_pattern_to_free);
            }
        };
        
        struct val_corresp
        {
        	module_ptr source;
            module_ptr sink;
            // FIXME: more here
        };
    
    	iface_pair sorted(iface_pair p) 
        { return p.first < p.second ? p : std::make_pair(p.second, p.first); }
    
    public:
		typedef std::multimap<iface_pair, ev_corresp> ev_corresp_map_t;
        typedef std::multimap<iface_pair, val_corresp> val_corresp_map_t;
        typedef std::vector<ev_corresp_map_t::value_type *> wrapper_corresp_list;
        typedef std::map<std::string, wrapper_corresp_list> wrappers_map_t;
    private:
    	// correspondences
    	ev_corresp_map_t ev_corresps;
        val_corresp_map_t val_corresps;
        
        // wrappers are stored in a map from symname to corresp-list
        // a corresp is in the list iff it activates the wrapper
        // i.e. its source event 
        wrappers_map_t wrappers;
        
        wrapper_file *p_wrap_code; // FIXME: this should be a contained subobject...
        wrapper_file& wrap_code;	// but is a pointer because of...
        	// stupid C++ inability to resolve circular include dependency
        std::ofstream wrap_file;
    
    protected:
    	// process a pairwise block
    	void add_corresps_from_block(module_ptr left,
        	module_ptr right, antlr::tree::Tree *corresps);
		// do name-matching
		void add_implicit_corresps(iface_pair ifaces);
        void name_match_required_and_provided(iface_pair ifaces,
        	module_ptr requiring_iface, module_ptr providing_iface);
        // utility for adding corresps
        void add_event_corresp(module_ptr source, antlr::tree::Tree *source_pattern,
    	        module_ptr sink,
                antlr::tree::Tree *sink_expr,
                bool free_source = false,
                bool free_sink = false);
		void compute_wrappers();

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
        link_derivation(cake::request& r, antlr::tree::Tree *t, std::string& output_filename);
        virtual ~link_derivation();
	};
}

#endif
