#ifndef CAKE_LINK_HPP_
#define CAKE_LINK_HPP_

#include <fstream>
#include <set>
#include <unordered_map>
#include <dwarfpp/cxx_model.hpp>
#include <boost/iterator/filter_iterator.hpp>
#include "cake/cxx_target.hpp"
#include "request.hpp"
#include "parser.hpp"

namespace cake {
	using std::string;
	using std::cerr;
	using std::endl;
	using std::pair;
	using std::make_pair;
	using std::set;
	using std::map;
	using std::multimap;
	using std::vector;
	using std::unary_function;
	using std::unordered_map;
	using boost::dynamic_pointer_cast;
	using boost::shared_ptr;
	using boost::optional;
	using namespace dwarf;
	using dwarf::spec::type_die;
	using dwarf::tool::cxx_target;
	
	class wrapper_file;
	class value_conversion;
	
	class link_derivation : public derivation
	{
		friend class wrapper_file;
		cake_cxx_target compiler;
	public:
		typedef pair<module_ptr,module_ptr> iface_pair;
		string name_of_module(module_ptr m) { return this->r.module_inverse_tbl[m]; }
		module_ptr module_for_dieset(spec::abstract_dieset& ds)
		{ 
			for (auto i_mod = r.module_tbl.begin(); i_mod != r.module_tbl.end(); i_mod++)
			{
				if (&i_mod->second->get_ds() == &ds) return i_mod->second;
			}
			assert(false);
		}
	private:
		typedef srk31::conjoining_iterator<
			encap::compile_unit_die::subprogram_iterator>
				subprograms_in_file_iterator;
		// ^-- this will give us all subprograms, even static / non-visible ones...
		// ... so push the visibility test into these predicates too --v
		
		struct is_visible : public unary_function<shared_ptr<spec::subprogram_die>, bool>
		{
			bool operator()(shared_ptr<spec::subprogram_die> p_subp) const
			{
				return spec::file_toplevel_die::is_visible()(
					dynamic_pointer_cast<spec::basic_die>(p_subp)
					);
			}
		};

		struct is_provided : public unary_function<shared_ptr<spec::subprogram_die>, bool>
		{
			bool operator()(shared_ptr<spec::subprogram_die> p_subp) const
			{ 
				is_visible is_v; 
				return is_v(p_subp) && (
					!(p_subp->get_declaration()) 
				|| (!(*(p_subp->get_declaration())))); 
			}
		};
		struct is_required : public unary_function<shared_ptr<spec::subprogram_die>, bool>
		{
			bool operator()(shared_ptr<spec::subprogram_die> p_subp) const
			{
				is_provided is_p; is_visible is_v; 
				return is_v(p_subp) && !(is_p(p_subp)); 
			}
		};
        typedef boost::filter_iterator<is_provided, subprograms_in_file_iterator>
        	 provided_funcs_iter;
        typedef boost::filter_iterator<is_required, subprograms_in_file_iterator>
        	 required_funcs_iter;

       struct ev_corresp
        {
        	module_ptr source;
        	antlr::tree::Tree *source_pattern;
            antlr::tree::Tree *source_infix_stub;
            module_ptr sink;
            antlr::tree::Tree *sink_expr;
            antlr::tree::Tree *sink_infix_stub;

            antlr::tree::Tree *return_leg;
            
            // if we created a temporary AST, for implicit rules, free these ptrs
            antlr::tree::Tree *source_pattern_to_free; 
            antlr::tree::Tree *sink_pattern_to_free;
            ~ev_corresp() { // non-virtual to keep us POD / initializer-constructible etc.
				if (source_pattern_to_free && source_pattern_to_free->free) source_pattern_to_free->free(source_pattern_to_free);
            	if (sink_pattern_to_free && sink_pattern_to_free->free) sink_pattern_to_free->free(sink_pattern_to_free);
            }
        };
        
        typedef value_conversion val_corresp;
    
    public:
    	static iface_pair sorted(iface_pair p) 
        { return p.first < p.second ? p : make_pair(p.second, p.first); }
    	static iface_pair sorted(module_ptr p, module_ptr q) 
        { return sorted(make_pair(p, q)); }
        
        set<iface_pair> all_iface_pairs;
    
		typedef multimap<iface_pair, ev_corresp> ev_corresp_map_t;
		typedef multimap<iface_pair, shared_ptr<val_corresp> > val_corresp_map_t;
        std::map< shared_ptr<val_corresp>, int > val_corresp_numbering;
		
        typedef ev_corresp_map_t::value_type ev_corresp_entry;
        typedef val_corresp_map_t::value_type val_corresp_entry;
		
		// we record the value corresps grouped by iface_pair and by
		// source data type, and later single out a unique init rule
		// for each source data type
		struct init_rules_key_t
		{
			bool from_first_to_second;
			shared_ptr<spec::type_die> source_type;
			bool operator<(const init_rules_key_t& k) const
			{ return this->from_first_to_second < k.from_first_to_second
				|| (this->from_first_to_second == k.from_first_to_second
					&& this->source_type < k.source_type);
			}
		};
		typedef shared_ptr<val_corresp> init_rules_value_t;
		typedef multimap<
			init_rules_key_t,
			init_rules_value_t
		> candidate_init_rules_tbl_t;
		
		map<iface_pair, set<init_rules_key_t> > candidate_init_rules_tbl_keys;
		map<iface_pair, candidate_init_rules_tbl_t> candidate_init_rules;
		
		typedef map<init_rules_key_t, init_rules_value_t> init_rules_tbl_t;
		map<iface_pair, init_rules_tbl_t> init_rules_tbl;
        
        // List of pointer into an ev_corresp map.
        // Note: all pointers should all point into same map, and moreover,
        // should have the same first element (iface_pair).
        typedef vector<ev_corresp_map_t::value_type *> ev_corresp_pair_ptr_list;
        
        // Map from symbol name
        // to list of correspondences
        typedef map<string, ev_corresp_pair_ptr_list> wrappers_map_t;

		// we will group value corresps by a four-tuple...
		struct val_corresp_group_key
		{
			module_ptr source_module;
			module_ptr sink_module;
			shared_ptr<type_die> source_data_type;
			shared_ptr<type_die> sink_data_type;
			bool operator==(const val_corresp_group_key& arg) const
			{ return source_module == arg.source_module && sink_module == arg.sink_module
				&& source_data_type == arg.source_data_type 
				&& sink_data_type == arg.sink_data_type; }
			bool operator<(const val_corresp_group_key& arg) const
			{ return source_module < arg.source_module
			     || (source_module == arg.source_module && sink_module < arg.sink_module)
				 || (source_module == arg.source_module && sink_module == arg.sink_module
				    && source_data_type < arg.source_data_type)
				 || (source_module == arg.source_module && sink_module == arg.sink_module
				    && source_data_type == arg.source_data_type
				    && sink_data_type < arg.sink_data_type);
			}
		};
		typedef std::map<val_corresp_group_key, vector<val_corresp *> > 
		val_corresp_group_tbl_t;
		
		std::set< shared_ptr<type_die> > significant_typedefs;
		
		shared_ptr<type_die> 
		first_significant_type(shared_ptr<type_die> t);
		
		typedef map<iface_pair, val_corresp_group_tbl_t> val_corresp_groups_tbl_t;
		val_corresp_groups_tbl_t val_corresp_groups;
		
		optional<link_derivation::val_corresp_map_t::iterator>
		find_value_correspondence(
			module_ptr source, shared_ptr<spec::type_die> source_type,
			module_ptr sink, shared_ptr<spec::type_die> sink_type);
			
 		vector<shared_ptr<spec::type_die> >
		corresponding_dwarf_types(shared_ptr<spec::type_die> type,
			module_ptr corresp_module,
			bool flow_from_type_module_to_corresp_module);
			
		shared_ptr<spec::type_die>
		unique_corresponding_dwarf_type(
			shared_ptr<spec::type_die> type,
			module_ptr corresp_module,
			bool flow_from_type_module_to_corresp_module);
		
		void merge_implicit_dwarf_info();
		
		void 
		find_type_expectations_in_stub(module_ptr module,
			antlr::tree::Tree *stub, 
			shared_ptr<spec::type_die> current_type_expectation,
			multimap< string, shared_ptr<spec::type_die> >& out);

		typedef unsigned long module_tag_t;
		
    private:
    	// correspondences
    	ev_corresp_map_t ev_corresps;
        val_corresp_map_t val_corresps;
		
		// maps remembering which functions have been handled by explicit correspondences,
		// to avoid generating implicit correspondences
		map<module_ptr, set<definite_member_name> > touched_events; // *source* events only
		map<module_ptr, set<definite_member_name> > touched_data_types;
		// HACK: ^^ we currently don't use this, because 
		// name_match_types already checks for pre-existing corresps (for synonymy reasons)
        
        // wrappers are stored in a map from symname to corresp-list
        // a corresp is in the list iff it activates the wrapper
        // i.e. its source event 
        wrappers_map_t wrappers;
        string output_namespace; // namespace in which code is emitted

        string wrap_file_makefile_name;
        string wrap_file_name;
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
			map<vector<string>, shared_ptr<spec::type_die> >& synonymy);
        void name_match_types(iface_pair ifaces);
        // utility for adding corresps
        void add_event_corresp(
        		module_ptr source, 
                antlr::tree::Tree *source_pattern,
                antlr::tree::Tree *source_infix_stub,
    	        module_ptr sink,
                antlr::tree::Tree *sink_expr,
                antlr::tree::Tree *sink_infix_stub,
				antlr::tree::Tree *return_leg,
                bool free_source = false,
                bool free_sink = false,
				bool init_only = false);
        void add_value_corresp(
        	module_ptr source, 
            shared_ptr<spec::type_die> source_data_type,
            antlr::tree::Tree *source_infix_stub,
            module_ptr sink,
            shared_ptr<spec::type_die> sink_data_type,
            antlr::tree::Tree *sink_infix_stub,
            antlr::tree::Tree *refinement,
			bool source_is_on_left,
            antlr::tree::Tree *corresp,
			bool init_only = false
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
        	antlr::tree::Tree *corresp,
			bool init_only = false
		);
		bool ensure_value_corresp(module_ptr source, 
			shared_ptr<spec::type_die> source_data_type,
        	module_ptr sink,
        	shared_ptr<spec::type_die> sink_data_type,
			bool source_is_on_left);
		void 
		find_usage_contexts(const string& ident,
			antlr::tree::Tree *t, vector<antlr::tree::Tree *>& out);
		void assign_value_corresp_numbers();
		void compute_init_rules();
		void compute_wrappers();
        module_tag_t module_tag(module_ptr module) 
		{ return reinterpret_cast<module_tag_t>(module.get()); }
		
//		void output_static_co_objects(); 

	public:
		pair<
			val_corresp_map_t::iterator,
			val_corresp_map_t::iterator
		> val_corresps_for_iface_pair(iface_pair ifaces)
		{ return val_corresps.equal_range(ifaces); }
		
		typedef link_derivation::val_corresp_map_t::value_type ent;
				void write_makerules(std::ostream& out);	
		void extract_definition();
		vector<string> dependencies() { return vector<string>(); }
        link_derivation(cake::request& r, antlr::tree::Tree *t, 
        	const string& id, const string& output_filename);
        virtual ~link_derivation();
        const string& namespace_name() { return output_namespace; }
	};
}

#endif
