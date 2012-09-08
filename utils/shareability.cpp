
#include <fstream>
#include <fileno.hpp>
#include <dwarfpp/lib.hpp>
#include <dwarfpp/attr.hpp>
#include <dwarfpp/adt.hpp>
#include <dwarfpp/encap.hpp>
#include <dwarfpp/cxx_compiler.hpp>
#include <boost/graph/graph_traits.hpp>
#include <boost/graph/depth_first_search.hpp>
#include <algorithm>
#include <srk31/algorithm.hpp>
#include <sstream>

using std::cout; 
using std::cerr; 
using std::endl;
using namespace dwarf;
using namespace dwarf::lib;
using std::pair;
using std::make_pair;
using boost::dynamic_pointer_cast;
using std::set;
using std::map;
using dwarf::spec::type_die;
using dwarf::spec::with_dynamic_location_die;
using std::ostringstream;

dwarf::tool::cxx_compiler compiler;

void collect_named_toplevel_types(
		core::basic_root_die& root,
		map<string, Dwarf_Off>& out
		);
void collect_rep_compatible_pairs(
		dwarf::lib::dieset& ds1, dwarf::lib::dieset& ds2, 
		const vector< pair< Dwarf_Off, Dwarf_Off > >& like_named,
		vector< pair< Dwarf_Off, Dwarf_Off > >& like_named_and_rep_compatible
	);
	
// edge representation
struct way_to_reach_type
{
	dwarf::spec::abstract_dieset *p_ds;
	Dwarf_Off source;
	enum how_t { DEREF, SUBOBJECT, DOWNCAST_ZERO_OFFSET, DOWNCAST_INHERITANCE, PARAM, RETURN_VALUE } how;
	Dwarf_Off target;
	bool operator< (const way_to_reach_type& arg) const
	{ return this->source < arg.source 
		 || (this->source == arg.source && this->target < arg.target)
		 || (this->source == arg.source && this->target == arg.target
		    && (int) this->how < (int) arg.how); 
	}
	bool operator==(const way_to_reach_type& arg) const
	{ return this->source == arg.source && this->target == arg.target 
		&& this->how == arg.how; }
	bool operator!=(const way_to_reach_type& arg) const
	{ return !(*this == arg); }
};

// node representation
struct type; // forward decl

// node collection builder
const type& ensure_type_node(set<type>& g, shared_ptr<type_die> p_t);

// our graph definitions
namespace boost {
	template <>
	struct graph_traits< set<typename ::type> >;
}
	struct transform_reference_to_pointer : std::unary_function< ::type const&, const ::type *>
	{
		const ::type *operator()( ::type const& arg) const { return &arg; }
	};
	inline const ::type * source(way_to_reach_type e, const set< ::type>& g);
	inline const ::type * target(way_to_reach_type e, const set< ::type>& g);

// 	inline std::pair< set< way_to_reach_type >::iterator, set<way_to_reach_type >::iterator >
// 	out_edges(const ::type& t, const set< ::type> & g);
//
// 	inline std::pair< set< ::type>::iterator, set< ::type>::iterator >
// 	vertices(const set< ::type>& g);
//
// 	inline set< ::type>::size_type num_vertices(const set< ::type>& g);
// 	inline set<way_to_reach_type>::size_type out_degree(const ::type *t, const set< ::type>& g);
// }
void
find_instantiating_inheritances_or_members(
	shared_ptr<type_die> p_t,
	shared_ptr<type_die> concrete_t,
	encap::dieset& eds,
	vector<shared_ptr<with_dynamic_location_die> >& out
	);
void
find_instantiating_inheritances_or_members(
	shared_ptr<type_die> p_t,
	shared_ptr<type_die> concrete_t,
	encap::dieset& eds,
	vector<shared_ptr<with_dynamic_location_die> >& out
	)
{
	assert(p_t->get_concrete_type()->get_offset() == concrete_t->get_offset());

	/* For each backref, if it is a typedef / qualifed type pointing at us,
	 * we recurse on its backrefs. */
	auto& backrefs = eds.backrefs()[p_t->get_offset()];
	for (auto i_backref = backrefs.begin(); i_backref != backrefs.end();
		++i_backref)
	{
		/* Is this the DW_AT_type of an inheritance or member? */
		auto p_d = eds[i_backref->first];
		cerr << "Type " << p_t->summary()
			<< " is referenced from " << p_d->summary()
			<< endl;
		/* If we're an analias or synonym of the start type, recurse. */
		auto as_t = dynamic_pointer_cast<type_die>(p_d);
		if (as_t && i_backref->second == DW_AT_type 
			&& as_t->get_concrete_type()->get_offset() == concrete_t->get_offset())
		{
			/* It's a chain we should recurse along. */
			find_instantiating_inheritances_or_members(as_t, concrete_t, eds, out);
		}
		else if (p_d->get_tag() == DW_TAG_member || p_d->get_tag() == DW_TAG_inheritance)
		{
			auto p_including_t = dynamic_pointer_cast<type_die>(p_d->get_parent());
			assert(p_including_t);

			auto tag = p_d->get_tag();
			auto attr = i_backref->second;
			if (attr != DW_AT_type) continue;
			auto to_insert = dynamic_pointer_cast<with_dynamic_location_die>(p_d);
			assert(to_insert);
			if (tag == DW_TAG_inheritance) out.push_back(to_insert);
			else if (tag == DW_TAG_member)
			{
				auto member = dynamic_pointer_cast<spec::member_die>(p_d);
				/* check it's zero-offset */
				if (!member->get_data_member_location()
					|| evaluator(
						member->get_data_member_location()->at(0), 
						eds.get_spec(),
						std::stack<Dwarf_Unsigned>(
								// push zero as the initial stack value
								std::deque<Dwarf_Unsigned>(1, 0UL)
								)
						).tos() == 0)
				{
					out.push_back(to_insert);
				}
			}
		}
		else
		{
			// could be a formal parameter or similar
			continue;
		}
	} // end for backrefs
}

struct type : public dwarf::spec::abstract_dieset::position
{
	typedef dwarf::spec::abstract_dieset::position position;
	// note: various stuff here is mutable so that it can be modified
	// in-place while stored in a set. The stuff that operator< depends
	// on is *not* mutable, naturally. We inherit position's operator<.

	/* At construction time, compute our out-edges. */
	mutable set< way_to_reach_type > edges;
	set<type> *p_graph;
	mutable bool initialized;
	//static type void_type;
//private:
	type() : position((position){0, 0}), p_graph(0), initialized(false) {}
private:
	// we use this only to construct dummy type elements to look up in the set<type>
	type(const position& pos) : position(pos), p_graph(0), initialized(false) {}
	friend inline const ::type * /*boost::*/source(way_to_reach_type e, const set<type>& g);
	friend inline const ::type * /*boost::*/target(way_to_reach_type e, const set<type>& g);

public:

	type(set<type>& graph, shared_ptr<type_die> p_in) : p_graph(&graph), initialized(false)
	{
		shared_ptr<type_die> p_t = p_in->get_ds().canonicalise_type(p_in, compiler);
		if (!p_t) throw "is void";
		// HM: try using the concrete position only
		*static_cast<dwarf::spec::abstract_dieset::position *>(this)
		 = (dwarf::spec::abstract_dieset::position){ &p_t->get_ds(), p_t->get_offset() };
	}

private:
	shared_ptr<type_die> get() 
	{
		auto p_d = (*p_ds)[off];
		assert(p_d);
		auto p_t = dynamic_pointer_cast<type_die>(p_d);
		assert(p_t);
		return p_t;
	}

public:
	/* We delay our initialization to avoid infinite looping in the case of
	 * recursive data types. */
	void initialize(dwarf::encap::dieset& eds) const // <-- modifies only "mutable" fields!
	{
		auto nonconst_this = const_cast<type*>(this);
		auto p_t = nonconst_this->get();
		auto concrete_t = p_t->get_concrete_type();
		assert(concrete_t);

		switch (concrete_t->get_tag())
		{
			case DW_TAG_base_type: break; // no edges <-- can't reach anything from these
			case DW_TAG_enumeration_type: break; // no edges
			case DW_TAG_subroutine_type: {
				// we can "reach" the argument and return types
				auto p_subt = dynamic_pointer_cast<spec::subroutine_type_die>(concrete_t);
				assert(p_subt);
				for (auto i_fp = p_subt->formal_parameter_children_begin(); 
					i_fp != p_subt->formal_parameter_children_end();
					++i_fp)
				{
					if ((*i_fp)->get_type() && (*i_fp)->get_type()->get_concrete_type())
					{
						edges.insert((way_to_reach_type){
							this->p_ds,
							this->off,
							way_to_reach_type::PARAM,
							ensure_type_node(*p_graph, (*i_fp)->get_type()).off
						});
					}
				}
				if (p_subt->get_type() && p_subt->get_type()->get_concrete_type())
				{
					edges.insert((way_to_reach_type){
						this->p_ds,
						this->off,
						way_to_reach_type::RETURN_VALUE,
						ensure_type_node(*p_graph, p_subt->get_type()).off
					});
				}

			} break;
			case DW_TAG_array_type: {
				// we can reach the ultimate element type
				auto ultimate_element_type = dynamic_pointer_cast<spec::array_type_die>(concrete_t)
					->ultimate_element_type();
				assert(ultimate_element_type && ultimate_element_type->get_concrete_type());
				edges.insert((way_to_reach_type){
					this->p_ds,
					this->off,
					way_to_reach_type::DEREF,
					ensure_type_node(*p_graph, ultimate_element_type).off
					});
			} break;
			case DW_TAG_pointer_type:
			{
				auto pointer_target_type = dynamic_pointer_cast<spec::pointer_type_die>(
					concrete_t)->get_type();
				if (pointer_target_type && pointer_target_type->get_concrete_type())
				{
					// we can reach the target type from these
					edges.insert((way_to_reach_type){
						this->p_ds,
						this->off,
						way_to_reach_type::DEREF,
						ensure_type_node(*p_graph, pointer_target_type).off
						});
				}
				else
				{
					// this is the void pointer case -- might want to advise user?
					cerr << "Found void* type: " << concrete_t->summary() << endl;
				}
			} break;

			//case DW_TAG_class_type: <-- fix up DWARF ADT to have a common supertype for struct & class
			case DW_TAG_structure_type:
			{
				/* Use the backrefs info in eds{1,2} to find out who inherits
				 * from us, or zero-offset-contains us. */
				set< pair< Dwarf_Off, Dwarf_Half > > canon_backrefs;
				eds.for_all_identical_types(concrete_t,
					[&canon_backrefs, &eds](shared_ptr<type_die> some_t)
					{
						// FIXME: BUG: this doesn't pick up on member/inheritance DIEs 
						// that are defined as instances typedefs 
						// HMM... how to fix this?

						vector<shared_ptr<with_dynamic_location_die> > instantiatings;
						find_instantiating_inheritances_or_members(
							some_t, 
							some_t->get_concrete_type(),
							eds,
							instantiatings);
						for (auto i_inst = instantiatings.begin(); 
							i_inst != instantiatings.end(); ++i_inst)
						{
							auto enclosing_type = dynamic_pointer_cast<type_die>(
								(*i_inst)->get_parent());
							assert(enclosing_type);
							auto canonicalised = eds.canonicalise_type(enclosing_type,
								compiler);
							assert(canonicalised);
							canon_backrefs.insert(make_pair(canonicalised->get_offset(),
								(*i_inst)->get_tag()));
						}
					});

				for (auto i_downcastable_to = canon_backrefs.begin();	
					i_downcastable_to != canon_backrefs.end();
					++i_downcastable_to)
				{
					auto p_target = (*this->p_ds)[i_downcastable_to->first];
					auto p_target_t = dynamic_pointer_cast<type_die>(p_target);
					assert(p_target_t);
					auto p_target_canon_t = p_ds->canonicalise_type(p_target_t,
						compiler);
					assert(p_target_canon_t);
					this->edges.insert((way_to_reach_type){
						this->p_ds,
						this->off,
						(i_downcastable_to->second == DW_TAG_inheritance)
							 ? way_to_reach_type::DOWNCAST_INHERITANCE 
							 : way_to_reach_type::DOWNCAST_ZERO_OFFSET,
						ensure_type_node(*p_graph, p_target_canon_t).off
						});
				}

// 				// we can be reached from anything that we inherit
// 				auto p_struct = dynamic_pointer_cast<structure_type_die>(p_t);
// 				for (auto i_inherit = p_struct->inheritance_children_begin();
// 					i_inherit != p_struct->inheritance_children_end();
// 					++i_inherit)
// 				{
// 					// NOTE that we are adding back-edges, so 
// 					// we modify a DIFFERENT type than us!
//
// 					// FIXME: this is a bit broken.  What if the types
// 					// that we could be downcast from 
// 					// are not toplevel name-matched types, so are
// 					// never passed to this function? How can we be sure
// 					// this code will see everything that a given type
// 					// could be downcast to? THis means all inheriting types
// 					// which is a global property of the component's DWARF info.
// 					// In other words, we need to search non-toplevel scopes
// 					// that can see a given toplevel definition (and hence can inherit it).
// 					// Can probably ignore for now
//
// 					const type& t = ensure_type_node(*p_graph, (*i_inherit)->get_type());
// 					t.edges.insert((way_to_reach_type){
// 						this->p_ds,
// 						t.off,
// 						way_to_reach_type::DOWNCAST_INHERITANCE,
// 						this->off
// 						});
// 				}
//
// 				// we can be reached from a zero-offset-contained type
// 				if (p_struct->member_children_begin()
// 					 != p_struct->member_children_end())
// 				{
// 					// is this child a structured type at offset zero?
// 					auto i_first_member = p_struct->member_children_begin();
// 					auto offset = evaluator(
// 							(*i_first_member)->get_data_member_location()->at(0), 
// 							p_ds->get_spec(),
// 							std::stack<Dwarf_Unsigned>(
// 									// push zero as the initial stack value
// 									std::deque<Dwarf_Unsigned>(1, 0UL)
// 									)
// 							).tos();
// 					if (offset == 0 && (*i_first_member)->get_type()
// 						&& (*i_first_member)->get_type()->get_concrete_type()
// 						&& (*i_first_member)->get_type()->get_concrete_type()->get_tag()
// 							== DW_TAG_structure_type)
// 					{
// 						const type& t = ensure_type_node(*p_graph, 
// 							(*i_first_member)->get_type());
// 						t.edges.insert((way_to_reach_type){
// 							this->p_ds,
// 							t.off,
// 							way_to_reach_type::DOWNCAST_ZERO_OFFSET,
// 							this->off
// 							});
// 					}
//
// 				}

			} // fall through! to handle data members
			case DW_TAG_union_type:
			{
				// we can reach any of our subobjects/arms
				shared_ptr<spec::with_data_members_die> with_data_members
				 = dynamic_pointer_cast<spec::with_data_members_die>(concrete_t);
				assert(with_data_members);
				for (auto i_memb = with_data_members->member_children_begin();
					i_memb != with_data_members->member_children_end();
					++i_memb)
				{
					auto member_type = (*i_memb)->get_type();
					assert(member_type && member_type->get_concrete_type());
					edges.insert((way_to_reach_type){
						this->p_ds,
						this->off,
						way_to_reach_type::SUBOBJECT, 
						ensure_type_node(*p_graph, member_type).off
					});
				}
			} break;

			default: {
				Dwarf_Half tag = concrete_t->get_tag();
				cerr << "Saw unexpected tag " << spec::DEFAULT_DWARF_SPEC.tag_lookup(tag)
					<< endl;
				assert(false);
			}
		} // end switch

		initialized = true;
	}
};
//type type::void_type;
void
construct_graph(
	set<type>& graph,
	vector< pair< Dwarf_Off, Dwarf_Off > > & offsets,
	dwarf::lib::dieset& ds,
	dwarf::encap::dieset& eds,
	bool is_second);

namespace boost
{
	template <>
	struct graph_traits< set< ::type> > {
		typedef ::type const *vertex_descriptor;
		typedef way_to_reach_type edge_descriptor;

		typedef set<way_to_reach_type>::iterator out_edge_iterator;
		typedef transform_iterator<
			transform_reference_to_pointer,
			set< ::type>::iterator
		> vertex_iterator;

		typedef directed_tag directed_category;
		typedef allow_parallel_edge_tag edge_parallel_category;
		typedef incidence_graph_tag traversal_category;

		typedef set< ::type>::size_type vertices_size_type;
		typedef set<way_to_reach_type>::size_type edges_size_type;
		typedef set<way_to_reach_type>::size_type degree_size_type;
	};

	}// end namespace boost
	inline
	const ::type *
	source(
		boost::graph_traits< set< ::type> >::edge_descriptor e,
		const set< ::type>& g)
	{
		auto found = g.find( ::type(( ::type::position){ e.p_ds, e.source }));
		assert(found != g.end());
		return &*found;
	}

	inline
	const ::type *
	target(
		boost::graph_traits< set< ::type> >::edge_descriptor e,
		const set< ::type>& g)
	{
		auto found = g.find( ::type(( ::type::position){ e.p_ds, e.target }));
		assert(found != g.end());
		return &*found;
	}

	inline std::pair<
		boost::graph_traits< set< ::type> >::out_edge_iterator,
		boost::graph_traits< set< ::type> >::out_edge_iterator 
	>
	out_edges(
		boost::graph_traits< set< ::type> >::vertex_descriptor u, 
		const set< ::type> & g)
	{
		return make_pair(u->edges.begin(), u->edges.end());
	}

	inline std::pair<
		boost::graph_traits<set< ::type> >::vertex_iterator,
		boost::graph_traits<set< ::type> >::vertex_iterator 
	>
	vertices(const set< ::type>& g)
	{
		return make_pair(g.begin(), g.end());
	}

	inline 
	boost::graph_traits<set< ::type> >::vertices_size_type 
	num_vertices(const set< ::type>& g)
	{
		return g.size();
	}

	inline boost::graph_traits<set< ::type> >::degree_size_type
	out_degree(
		boost::graph_traits<set< ::type> >::vertex_descriptor u,
		const set< ::type>& ds)
	{
		return u->edges.size();
	}
//}

struct path_explorer_t : public boost::dfs_visitor<>
{
	/* We remember all the paths without a back edge. How to
	 * enumerate them? 
	 * When we see a new edge, we walk the paths list,
	 * and add  */
	
	const type *root_vertex;
	 
	typedef std::deque< way_to_reach_type > path;
	vector<path> *p_paths;

	path_explorer_t(vector<path> *p_paths) : p_paths(p_paths) {}

	template <class Edge, class Graph>
	void add_paths(Edge e, Graph g)
	{
		//cerr << "Saw an edge!" << endl;
		vector<path> paths_to_add;
		/* 1. extend existing paths. */
		for (auto i_path = p_paths->begin(); i_path != p_paths->end(); ++i_path)
		{
			if (i_path->back().target == e.source)
			{
				auto copied_path = *i_path;
				copied_path.push_back(e);
				paths_to_add.push_back(copied_path);
			}
		}
		/* 2. start new paths. */
		if (e.source == root_vertex->off)
		{
			paths_to_add.push_back(path(1, e));
		}
		
		std::copy(paths_to_add.begin(), paths_to_add.end(),
			std::back_inserter(*p_paths));
	}

	template <class Edge, class Graph>
	void tree_edge(Edge e, Graph& g) { add_paths(e, g); }

	template <class Edge, class Graph>
	void forward_or_cross_edge(Edge e, Graph& g) { add_paths(e, g); }
	
	template <class Edge, class Graph>
	void back_edge(Edge e, Graph& g) { /* do nothing */}
};

string 
string_rep_for_type_die(shared_ptr<type_die> p_t) 
{

	assert(p_t->get_offset() == p_t->get_concrete_type()->get_offset()); // only call or recurse on concretised

	if (p_t->get_name()) return *p_t->get_name();
	else switch(p_t->get_tag())
	{
		case DW_TAG_base_type: assert(false); // base types should have names
		case DW_TAG_enumeration_type: { // HACK: implement this properly
			shared_ptr<type_die> underlying_type = dynamic_pointer_cast<spec::enumeration_type_die>(p_t)
					->get_type();
			if (!underlying_type)
			{
				underlying_type = dynamic_pointer_cast<spec::type_die>(
					p_t->enclosing_compile_unit()->named_child("int"));
				if (!underlying_type) underlying_type = dynamic_pointer_cast<spec::type_die>(
				 p_t->enclosing_compile_unit()->named_child("signed int"));
				
				assert(underlying_type);
			}
			assert(underlying_type->get_concrete_type());
		
			return "{" + string_rep_for_type_die(
				underlying_type->get_concrete_type()) + "}";
		}
		case DW_TAG_pointer_type: {
			shared_ptr<type_die> target_type = dynamic_pointer_cast<spec::pointer_type_die>(p_t)
					->get_type();
			if (target_type) target_type = target_type->get_concrete_type();
			return "{" 
				+ (target_type ? string_rep_for_type_die(target_type) : string("void")) 
				+ "*}";
		}
		case DW_TAG_array_type: {
			auto element_type = dynamic_pointer_cast<spec::array_type_die>(p_t)
					->ultimate_element_type()->get_concrete_type();
			return "{" + string_rep_for_type_die(element_type) + "[]}";
		}
		case DW_TAG_structure_type: {
			ostringstream out; 
			out << "{<";
			auto p_struct = dynamic_pointer_cast<spec::structure_type_die>(p_t);
			assert(p_struct);
			for (auto i_member = p_struct->member_children_begin();
				i_member != p_struct->member_children_end(); ++ i_member)
			{
				if (i_member != p_struct->member_children_begin()) out << ",";
				auto offset = evaluator(
						(*i_member)->get_data_member_location()->at(0), 
						p_struct->get_ds().get_spec(),
						std::stack<Dwarf_Unsigned>(
								// push zero as the initial stack value
								std::deque<Dwarf_Unsigned>(1, 0UL)
								)
						).tos();
				out << string_rep_for_type_die((*i_member)->get_type()->get_concrete_type());
				out << "@" << offset;
			}
			out << ">}";
			return out.str();
		}
		case DW_TAG_union_type: {
			ostringstream out;
			out << "{<";
			auto p_union = dynamic_pointer_cast<spec::union_type_die>(p_t);
			assert(p_union);
			for (auto i_member = p_union->member_children_begin();
				i_member != p_union->member_children_end(); ++ i_member)
			{
				if (i_member != p_union->member_children_begin()) out << "|";
				out << string_rep_for_type_die((*i_member)->get_type()->get_concrete_type());
			}
			out << ">}";
			return out.str();
		}
		case DW_TAG_subroutine_type: {
			ostringstream out;
			auto p_subt = dynamic_pointer_cast<spec::subroutine_type_die>(p_t);
			out << "{(";
			for (auto i_fp = p_subt->formal_parameter_children_begin(); 
				i_fp != p_subt->formal_parameter_children_end();  ++i_fp)
			{
				if (i_fp != p_subt->formal_parameter_children_begin())
				{
					out << ",";
				}
				out << string_rep_for_type_die((*i_fp)->get_type()->get_concrete_type());
			}
			out << ")=>";
			if (p_subt->get_type() && p_subt->get_type()->get_concrete_type())
			{
				out << string_rep_for_type_die(p_subt->get_type()->get_concrete_type());
			}
			out << "}";
			return out.str();
		}
		default: {
			Dwarf_Half tag = p_t->get_tag();
			cerr << "Saw unexpected tag " << spec::DEFAULT_DWARF_SPEC.tag_lookup(tag) << endl;
			assert(false);
		}
	} // end switch
	
	assert(false);
}; 

int main(int argc, char **argv)
{
	assert(argc > 2);
	cerr << "Opening " << argv[1] << "..." << endl;
	std::ifstream in1(argv[1]);
	assert(in1);
	core::basic_root_die root1(fileno(in1));
	dwarf::lib::file df1(fileno(in1));
	dwarf::lib::dieset ds1(df1);
	
	// read the list of data types we care about
	set<string> typenames;
	int i = 3; 
	while (i < argc)
	{
		typenames.insert(argv[i++]);
	}
	
	// also open an encap dieset, for backrefs
	std::ifstream ein1(argv[1]);
	assert(ein1);
	dwarf::encap::file edf1(fileno(ein1));
	dwarf::encap::dieset& eds1 = edf1.ds();

	cerr << "Opening " << argv[2] << "..." << endl;
	std::ifstream in2(argv[2]);
	assert(in2);
	core::basic_root_die root2(fileno(in2));
	dwarf::lib::file df2(fileno(in2));
	dwarf::lib::dieset ds2(df2);

	// also open an encap dieset, for backrefs
	std::ifstream ein2(argv[2]);
	assert(ein2);
	dwarf::encap::file edf2(fileno(ein2));
	dwarf::encap::dieset& eds2 = edf2.ds();
	

	/* Our analysis proceeds as follows.
	 * 1. Collect distinct named toplevel data types defined in the two files.
	 * 2. Enumerate the pairs of like-named data types.
	 * 3. Among these, enumerate the pairs that are rep-compatible.
	 * 4. Build the reachability graph in each case.
	 * 5. Run the algorithm removing non-analogously-reachable pairs to a fixed point.
	 * 6. The pairs of types that are left are definitely reachable. */
	
	map<string, Dwarf_Off> named_toplevel_types1;
	collect_named_toplevel_types(root1, named_toplevel_types1);
	
	map<string, Dwarf_Off> named_toplevel_types2;
	collect_named_toplevel_types(root2, named_toplevel_types2);
	
	// the offsets here are irrelevant -- we will discard them later
	vector< pair<string, Dwarf_Off> > shared_names;
	std::set_intersection(
		named_toplevel_types1.begin(), named_toplevel_types1.end(),
		named_toplevel_types2.begin(), named_toplevel_types2.end(),
		std::back_inserter(shared_names),
		named_toplevel_types1.value_comp()
		);
	cerr << "Shared names (" << shared_names.size() << "): ";
	for (auto i_name = shared_names.begin();
		 i_name != shared_names.end(); ++i_name)
	{
		if (i_name != shared_names.begin()) cerr << ", ";
		cerr << i_name->first;
	}
	cerr << endl;
	
	// actually get the offsets we need
	vector< pair< Dwarf_Off, Dwarf_Off > > like_named;
	for (auto i_name = shared_names.begin();
		i_name != shared_names.end();
		++i_name)
	{
		like_named.push_back(make_pair(
			named_toplevel_types1[i_name->first],
			named_toplevel_types2[i_name->first]
		));
	}
	
	// which of these is the user interested in?
	vector< pair<Dwarf_Off, Dwarf_Off > > like_named_and_interested;
	srk31::copy_if(like_named.begin(), like_named.end(),
		std::back_inserter(like_named_and_interested),
		[&eds1, &typenames](const pair<Dwarf_Off, Dwarf_Off>& p) {
			auto opt_name = eds1[p.first]->get_name();
			assert(opt_name);
			return typenames.find(*opt_name) != typenames.end();
		}
	);
	cout << "Of the " << typenames.size() << " types that the user is interested in, "
		<< like_named_and_interested.size() << " were found as a like-named pair." << endl;

	vector< pair< Dwarf_Off, Dwarf_Off > > like_named_and_rep_compatible;
	collect_rep_compatible_pairs(
		ds1, ds2, 
		like_named,
		like_named_and_rep_compatible
	);
	
	vector< pair< Dwarf_Off, Dwarf_Off > > like_named_and_rep_compatible_and_interested;
	srk31::copy_if(like_named_and_rep_compatible.begin(), like_named_and_rep_compatible.end(),
		std::back_inserter(like_named_and_rep_compatible_and_interested),
		[&eds1, &typenames](const pair<Dwarf_Off, Dwarf_Off>& p) {
			auto opt_name = eds1[p.first]->get_name();
			assert(opt_name);
			return typenames.find(*opt_name) != typenames.end();
		}
	);
	cout << "Of the " << typenames.size() << " types that the user is interested in, "
		<< like_named_and_rep_compatible_and_interested.size() 
		<< " were found as a like-named and rep-compatible pair." << endl;
	
	
	/* Now we need to build the type reachability graph.
	 * Recall that edges in the graph are from type to type
	 * where t1 --> t2 if
	 * t1 is a pointer to a t2
	 * t1 has a field (in any subobject) that is a pointer to a t2
	 * ... model this by "t1 contains a t2"... transitive reachability takes care of the rest
	 * t1 can be admissibly reinterpreted to a t2.
	 * 
	 * Note that the reachability graph includes types that are not in the shareable set! 
	 * Recall that if we run shareability analysis on two identical ABIs, everything
	 * that name-matches should turn out to be shareable.
	 * But not everything has a name, so not everything name-matches.
	 * That is okay. 
	 * 
	 * If two components share an object, they share everything that can be reached
	 * from that object. What we want to do is
	 * enumerate all acyclic paths in the reachability graph of each component
	 * and check that they stay within the set of rep-compabible types,
	 * i.e. if a given path from starting point t1 reaches t2 in component C1,
	 * then in component C2, starting from t1's correspondent t1', 
	 * will reach some type t2' which should be rep-compatible with t1'.
	 * 
	 * Where do we start the depth-first search?
	 * Start it from each name-matched type in turn?
	 */	
	set<type> graph1;
	construct_graph(graph1, like_named_and_rep_compatible, ds1, eds1, false);
	auto size1_at_construction = graph1.size();
	
	set<type> graph2;
	construct_graph(graph2, like_named_and_rep_compatible, ds2, eds2, true);
	auto size2_at_construction = graph2.size();
	
	/* Now for each data type that we care about, invoke dfs on the graph
	 * to enumerate the set of type reachability paths starting from that type. */
	map< pair<Dwarf_Off, Dwarf_Off>, bool > rep_compatibility_cache;
	for (auto i_lr = like_named_and_rep_compatible.begin();
		i_lr != like_named_and_rep_compatible.end();
		++i_lr)
	{
		auto start1 = dynamic_pointer_cast<type_die>(ds1[i_lr->first]);
		auto start2 = dynamic_pointer_cast<type_die>(ds2[i_lr->second]);
		string name1 = *start1->get_name();
		if (typenames.find(name1) == typenames.end())
		{
			cerr << "Skipping pair of " << start1->summary()
				<< " and " << start2->summary()
				<< " which the user wasn't interested in." << endl;
			continue;
		}
		
		cerr << "Deciding whether " << start1->summary()
			<< " and " << start2->summary()
			<< " are shareable." << endl;
		/* Now we have the two graphs, we depthfirst explore each one
		 * starting from each of the corresp'ing types, and
		 * for each path that we reach, we check that there is an analogous
		 * path in the other graph. */
		auto compute_paths = [i_lr](Dwarf_Off off, set<type>& graph, lib::dieset& ds, encap::dieset& eds){
			auto found_t = dynamic_pointer_cast<type_die>(ds[off]);
			assert(found_t);
			auto found_t_vertex = graph.find(type(graph, found_t));
			assert(found_t_vertex != graph.end());
			auto t_vertex = &*found_t_vertex;
			std::map<
				boost::graph_traits<set<type> >::vertex_descriptor, 
				boost::default_color_type
			> underlying_dfs_node_color_map;
			auto dfs_color_map = boost::make_assoc_property_map(underlying_dfs_node_color_map);
			vector<path_explorer_t::path> paths;
			path_explorer_t path_explorer(&paths);
			path_explorer.root_vertex = t_vertex;
			auto visitor = boost::visitor(path_explorer)
				.color_map(dfs_color_map)
				.root_vertex(t_vertex);
			// run the DFS
			boost::depth_first_search(graph, visitor);
			return std::move(paths);
		};
		
		auto paths1 = compute_paths(i_lr->first, graph1, ds1, eds1);
		auto paths2 = compute_paths(i_lr->second, graph2, ds2, eds2);
		cerr << "Generated " << paths1.size() << " and " << paths2.size()
			<< " paths respectively." << endl;
		
		/* Paths are not shareable iff 
		 * - any corresponding element along them is not rep-compatible with its correspondent;
		 * - any correpsonding element along them is not rep-compatible but has been
		 *    found to be non-shareable;
		 */
		
		/* Instead of computing paths, we really want to build a DAG
		 * so that we can eliminate paths that are subsequences of other paths.
		 * OR, hmm, does this let us get the partial style of result we want, i.e.
		 * "this pair is shareable, this one not"? */
		
		/* For each path that does not have an analogous path,
		 * for each data type along that common path prefix that was 
		 * in the possibly-shareable set, we can delete its pair from the set.
		 */
		/* 
		 * How to define analogousness?
		 * for named types: the name 
		 * for unnamed types: use a string rep? 
		 */
		
		typedef vector<path_explorer_t::path>::const_iterator val_t;
		
		struct comp_t
		{
			bool 
			operator()(const pair<string, val_t >& arg1, const pair<string, val_t >& arg2) const
			{
				return arg1.first < arg2.first; // i.e. like std::map
			}
		} comp;
		//comp_t comp(inner_comp); //(/*std::less<string>()*/);
		set<pair<string, val_t >, comp_t> stringified1(comp);
		set<pair<string, val_t >, comp_t> stringified2(comp);
		map< string, vector<path_explorer_t::path>::const_iterator > dummy_map;
		auto print_paths = [](const vector<path_explorer_t::path>& paths, lib::dieset& ds,
			decltype(stringified1)& out){

			cerr << "Found " << paths.size() << " paths." << endl;
			for (auto i_path = paths.begin();
				i_path != paths.end();
				++i_path)
			{
				auto print_friendly = [=]()
				{
					cerr << "From data type " << ds[i_path->front().source]->summary()
						<< " we can reach " << ds[i_path->back().target]->summary()
						<< " by path: ";
					boost::optional< way_to_reach_type > last_hop;
					for (auto i_hop = i_path->begin(); i_hop != i_path->end(); last_hop = *i_hop, ++i_hop)
					{
						switch(i_hop->how)
						{
							case way_to_reach_type::DEREF: cerr << "dereference"; break;
							case way_to_reach_type::SUBOBJECT: cerr << "subobject access"; break;
							case way_to_reach_type::DOWNCAST_ZERO_OFFSET: 
								cerr << "downcast using zero-offset"; break;
							case way_to_reach_type::DOWNCAST_INHERITANCE: 
								cerr << "downcast using inheritance"; break;
							case way_to_reach_type::PARAM:
								cerr << "parameter access"; break;
							case way_to_reach_type::RETURN_VALUE:
								cerr << "return value"; break;
							default: assert(false);
						}

						cerr << " to " << ds[i_hop->target]->summary() << "; ";

						if (last_hop)
						{
							// check that he last hop's target is our source
							assert(last_hop->target == i_hop->source);
						}

					}
					cerr << "END." << endl;
				};
				
				// don't print friendly; print stringified
				auto start = dynamic_pointer_cast<type_die>(ds[i_path->front().source]);
				ostringstream s;
				s << string_rep_for_type_die(start->get_concrete_type());

				boost::optional< way_to_reach_type > last_hop;
				for (auto i_hop = i_path->begin(); i_hop != i_path->end(); last_hop = *i_hop, ++i_hop)
				{
					switch(i_hop->how)
					{
						case way_to_reach_type::DEREF: s << "*"; break;
						case way_to_reach_type::SUBOBJECT: s << "."; break;
						case way_to_reach_type::DOWNCAST_ZERO_OFFSET: 
							s << "!"; break;
						case way_to_reach_type::DOWNCAST_INHERITANCE: 
							s << "^"; break;
						case way_to_reach_type::PARAM:
							s << ">"; break;
						case way_to_reach_type::RETURN_VALUE:
							s << "<"; break;
						default: assert(false);
					}

					auto p_next = dynamic_pointer_cast<type_die>(ds[i_hop->target]);
					assert(p_next);
					s << string_rep_for_type_die(p_next->get_concrete_type());

					if (last_hop)
					{
						// check that the last hop's target is our source
						assert(last_hop->target == i_hop->source);
					}

				}
				
				out.insert(make_pair(s.str(), i_path));
			}
		};
		
		print_paths(paths1, ds1, stringified1);
		print_paths(paths2, ds2, stringified2);
		
		/* Now we have two sets of strings. Find the set difference. */
// 		typedef path_explorer_t::path path;
// 		typedef vector<path> vec_t;
// 		typedef vec_t::iterator it_t;
// 		vector< pair<string, vector<path_explorer_t::path>::const_iterator > > difference1;
// 		std::set_difference(
// 			stringified1.begin(), stringified1.end(),
// 			stringified2.begin(), stringified2.end(),
// 			std::back_inserter(difference1),
// 			dummy_map.value_comp());
// 		cerr << "Set difference 1 has " << difference1.size() << " elements " << endl;
// 		for (auto i_diff = difference1.begin(); i_diff != difference1.end(); ++i_diff)
// 		{
// 			cerr << "Difference 1: " << i_diff->first << endl;
// 		}
// 		
// 		vector< pair<string, vector<path_explorer_t::path>::const_iterator > > difference2;
// 		std::set_difference(
// 			stringified2.begin(), stringified2.end(),
// 			stringified1.begin(), stringified1.end(),
// 			std::back_inserter(difference2),
// 			dummy_map.value_comp());
// 		for (auto i_diff = difference2.begin(); i_diff != difference2.end(); ++i_diff)
// 		{
// 			cerr << "Difference 2: " << i_diff->first << endl;
// 		}
// 		cerr << "Set difference 2 has " << difference2.size() << " elements " << endl;

		/* We simply iterate through the two sets in parallel, expecting to find the same
		 * strings. We also do the rep-compatibility check. */
		bool shareable = true;
		boost::optional<string> reason;
		auto it1 = stringified1.begin();
		auto it2 = stringified2.begin();
		for (;
			it1 != stringified1.end() && it2 != stringified2.end();
			++it1, ++it2)
		{
			auto i_path1 = it1->second;
			auto i_path2 = it2->second;

			/* Actually test_start1 won't be the same as test_start1 in general, 
 			 * because extra canonicalisation has been done to test_start1/2 i.e.
			 * the ones actually in the graph. */
			auto test_start1 = dynamic_pointer_cast<type_die>(ds1[i_path1->front().source]);
			assert(test_start1);
			//assert(test_start1->get_offset() == start1->get_offset());
			auto test_start2 = dynamic_pointer_cast<type_die>(ds2[i_path2->front().source]);
			assert(test_start2);
			//assert(test_start2->get_offset() == start2->get_offset());

			if (it1->first != it2->first) 
			{ 
				shareable = false; 
				reason = string("Starting from file ") +
					((it1->first < it2->first) ? string(argv[1]) : string(argv[2]))
					+ ", " +
					((it1->first < it2->first) ? start1->summary() : start2->summary())
					+ " path " +
					((it1->first < it2->first) ? it1->first : it2->first)
					+ " is not valid from file " +
					((it1->first < it2->first) ? string(argv[2]) : string(argv[1]))
					+ ", " +
					((it1->first < it2->first) ? start2->summary() : start1->summary());
				break; 
			}
			
			
			/* Now check for rep-compatibility of each type along the way. */
			
			if (!start1->is_rep_compatible(start2) || !start2->is_rep_compatible(start1))
			{ assert(false); } // we only start the analysis from like-named and rep-compatible types
			rep_compatibility_cache.insert(
				make_pair(
					make_pair(start1->get_offset(), start2->get_offset()), 
					true
				)
			);
			
			boost::optional< way_to_reach_type > last_hop1;
			boost::optional< way_to_reach_type > last_hop2;
			unsigned hopcount = 0;
			auto i_hop1 = i_path1->begin(), i_hop2 = i_path2->begin();
			for (;
				i_hop1 != i_path1->end() && i_hop2 != i_path2->end();
				last_hop1 = *i_hop1, last_hop2 = *i_hop2, ++i_hop1, ++i_hop2, ++hopcount)
			{
				auto next1 = dynamic_pointer_cast<type_die>(ds1[i_hop1->target]);
				auto next2 = dynamic_pointer_cast<type_die>(ds2[i_hop2->target]);
				
				auto found_in_cache = rep_compatibility_cache.find(make_pair(next1->get_offset(),	
					next2->get_offset()));
				bool hit_cache = (found_in_cache != rep_compatibility_cache.end());
				
				if ((hit_cache && found_in_cache->second)
					|| (!hit_cache && (
						!next1->is_rep_compatible(next2) || 
						!next2->is_rep_compatible(next1))))
				{
					if (!hit_cache) rep_compatibility_cache.insert(make_pair(make_pair(next1->get_offset(), next2->get_offset()), true));
					std::ostringstream s; s << hopcount; string hopcount_str = s.str();
					reason = string("Starting from file ") + argv[1] + ", " + start1->summary()
					 + " and file " + argv[2] + ", " + start2->summary()
					 + " found rep-incompatible types, resp. "
					 + next1->summary() + ", " + next2->summary()
					 + " at hop " + hopcount_str + " along path " + it1->first;
					break;
				} else if (!hit_cache) rep_compatibility_cache.insert(make_pair(make_pair(next1->get_offset(), next2->get_offset()), false));
			}
			if (i_hop1 != i_path1->end() || i_hop2 != i_path2->end())
			{
				assert(reason); // hopcount should be the same
				shareable = false; break;
			}
		}
		// if we have anything left over, we're not shareable
		if (it1 != stringified1.end() || it2 != stringified2.end())
		{
			if (!reason) reason = string("File ") +
				(it1 == stringified1.end() ? argv[2] : argv[1])
				+ ", " + 
				(it1 == stringified1.end() ? start2->summary() : start1->summary() )
				+ " supports more paths than file " + 
				(it1 == stringified1.end() ? argv[1] : argv[2])
				+ ", " + 
				(it1 == stringified1.end() ? start1->summary() : start2->summary() )
				+ ", first additional path: " + 
				(it1 == stringified1.end() ? it2->first : it1->first );
				
			shareable = false;
		}

		if (shareable)
		{
			std::cout << "Found (from file " << argv[1] << ") " << start1->summary() 
				<< " to be shareable with (from file " << argv[2] << ") " << start2->summary()
				<< endl;
			 assert(!reason);
		}
		if (!shareable)
		{
			assert(reason);
			std::cout << "Found (from file " << argv[1] << ") " << start1->summary() 
				<< " NOT SHAREABLE with (from file " << argv[2] << ") " << start2->summary()
				<< ", reason: " << *reason << endl;
		}
		
		/* FIXME: ensuring that all elements along the path are rep-compatible
		 * Otherwise, we might have to track what offsets (say) were accessed on DEREF or SUBOBJECT
		 * etc.. */

	} // end for i_lr
	
}


	
void
construct_graph(
	set<type>& graph,
	vector< pair< Dwarf_Off, Dwarf_Off > > & offsets,
	dwarf::lib::dieset& ds,
	dwarf::encap::dieset& eds,
	bool is_second)
{
	 
	// 1. seed the set of nodes by adding all our corresp'd types
	for (auto i_off = offsets.begin(); i_off != offsets.end(); ++i_off)
	{
		auto p_d = ds[is_second ? i_off->second : i_off->first];
		assert(p_d);
		auto p_t = dynamic_pointer_cast<spec::type_die>(p_d);
		assert(p_t);
		auto concrete_t = p_t->get_concrete_type();
		if (!concrete_t)
		{
			cerr << "Skipping void/opaque type " << concrete_t->summary() << endl;
		}
		auto tag = p_t->get_tag();
		ensure_type_node(graph, p_t);
	}

	// 2. build nodes and edges by initializing nodes until a fixed point
	bool saw_something_uninitialized = false;
	do
	{
		for (set<type>::iterator i_node = graph.begin(); i_node != graph.end(); ++i_node)
		{
			if (!i_node->initialized)
			{
				saw_something_uninitialized = true;
				i_node->initialize(eds); // may add more uninit'd nodes to the set
			}
		}
		
	} while (saw_something_uninitialized && (saw_something_uninitialized = false, true));
	
	// 3. sanity check: for every edge in the graph, are all its vertices initialized?
	auto verts = vertices(graph);
	for (auto i_vert = verts.first; i_vert != verts.second; ++i_vert)
	{
		auto edges = out_edges(*i_vert, graph); 
		for (auto i_edge = edges.first; i_edge != edges.second; ++i_edge)
		{
			assert(std::find(verts.first, verts.second, source(*i_edge, graph)) != verts.second);
			assert(std::find(verts.first, verts.second, target(*i_edge, graph)) != verts.second);
			auto found = graph.find(**i_vert);
			assert(found != graph.end());
		}
	}
	
}

const type& ensure_type_node(set<type>& graph, shared_ptr<type_die> p_t)
{
	// we construct, but don't initialize(), the type
	type t(graph, p_t);
	
	// insert it -- this may or may not do anything
	graph.insert(t);
	
	// return what is in the set
	// NOTE that we don't initialize it!
	set<type>::iterator found = graph.find(t);
	assert(found != graph.end());
	return *found;
}
	
void collect_named_toplevel_types(
		core::basic_root_die& root,
		map<string, Dwarf_Off>& out
		)
{
	cerr << "Searching for data types in ..." << endl;
	for (auto i = root.begin(); i != root.end(); ++i)
	{
		cerr << "At depth " << i.depth() << endl;
		if (i.depth() != 2) continue; // must be toplevel
		
		if (i.tag_here() == DW_TAG_base_type
		 || i.tag_here() == DW_TAG_typedef
		 || i.tag_here() == DW_TAG_structure_type
		 || i.tag_here() == DW_TAG_union_type
		 || i.tag_here() == DW_TAG_enumeration_type)
		 
		{
			if (!i.name_here()) continue;
			string name(i.name_here().get());
			out.insert(make_pair(name, i.offset_here()));
		}
	}
}
void collect_rep_compatible_pairs(
	dwarf::lib::dieset& ds1, 
	dwarf::lib::dieset& ds2, 
	const vector< pair< Dwarf_Off, Dwarf_Off > >& like_named,
	vector< pair< Dwarf_Off, Dwarf_Off > >& out
)
{
	cerr << "Out of " << like_named.size() << " candidates, found "
		<< "like-named and rep-compatible pairs of types: ";
	bool output = false;
	for (auto i_ln = like_named.begin(); i_ln != like_named.end(); ++i_ln)
	{
		auto type1 = dynamic_pointer_cast<spec::type_die>(ds1[i_ln->first]);
		auto type2 = dynamic_pointer_cast<spec::type_die>(ds2[i_ln->second]);
		
		assert(type1);
		assert(type2);
		
		if (type1->is_rep_compatible(type2)
		 && type2->is_rep_compatible(type1))
		{
			out.push_back(*i_ln);
			if (output) cerr << ", ";
			cerr << *ds1[i_ln->first]->get_name(); output = true;
		}
		else cerr << type1->summary() << " is not rep-compatible with " << type2->summary()
			<< " or vice-versa." << endl;
	}
	cerr << "; total " << like_named.size() << endl;
}
