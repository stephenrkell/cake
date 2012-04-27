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
	enum how_t { DEREF, SUBOBJECT, DOWNCAST_ZERO_OFFSET, DOWNCAST_INHERITANCE } how;
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
struct transform_reference_to_pointer : std::unary_function<type const&, const type *>
{
	const type *operator()(type const& arg) const { return &arg; }
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
//}

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
		*static_cast<dwarf::spec::abstract_dieset::position *>(this)
		 = (dwarf::spec::abstract_dieset::position){ &p_in->get_ds(), p_in->get_offset() };
		shared_ptr<type_die> p_t = p_in->get_ds().canonicalise_type(p_in, compiler);
		if (!p_t) throw "is void";
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

		switch (p_t->get_tag())
		{
			case DW_TAG_base_type: break; // no edges <-- can't reach anything from these
			case DW_TAG_enumeration_type:
				break; // no edges

			case DW_TAG_array_type: {
				// we can reach the ultimate element type
				auto ultimate_element_type = dynamic_pointer_cast<spec::array_type_die>(p_t)
					->ultimate_element_type();
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
					p_t->get_concrete_type())->get_type();
				if (pointer_target_type->get_concrete_type())
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
					cerr << "Found void* type: " << p_t->summary() << endl;
				}
			} break;

			//case DW_TAG_class_type: <-- fix up DWARF ADT to have a common supertype for struct & class
			case DW_TAG_structure_type:
			{
				/* Use the backrefs info in eds{1,2} to find out who inherits
				 * from us, or zero-offset-contains us. */
				
				// set<Dwarf_Off> canon_backrefs;
				
				// auto backrefs = eds.backrefs().find(
				
				
				// we can be reached from anything that we inherit
				auto p_struct = dynamic_pointer_cast<structure_type_die>(p_t);
				for (auto i_inherit = p_struct->inheritance_children_begin();
					i_inherit != p_struct->inheritance_children_end();
					++i_inherit)
				{
					// NOTE that we are adding back-edges, so 
					// we modify a DIFFERENT type than us!
					
					// FIXME: this is a bit broken.  What if the types
					// that we could be downcast from 
					// are not toplevel name-matched types, so are
					// never passed to this function? How can we be sure
					// this code will see everything that a given type
					// could be downcast to? THis means all inheriting types
					// which is a global property of the component's DWARF info.
					// In other words, we need to search non-toplevel scopes
					// that can see a given toplevel definition (and hence can inherit it).
					// Can probably ignore for now
					
					const type& t = ensure_type_node(*p_graph, (*i_inherit)->get_type());
					t.edges.insert((way_to_reach_type){
						this->p_ds,
						t.off,
						way_to_reach_type::DOWNCAST_INHERITANCE,
						this->off
						});
				}
				
				// we can be reached from a zero-offset-contained type
				if (p_struct->member_children_begin()
					 != p_struct->member_children_end())
				{
					// is this child a structured type at offset zero?
					auto i_first_member = p_struct->member_children_begin();
					auto offset = evaluator(
							(*i_first_member)->get_data_member_location()->at(0), 
							p_ds->get_spec(),
							std::stack<Dwarf_Unsigned>(
									// push zero as the initial stack value
									std::deque<Dwarf_Unsigned>(1, 0UL)
									)
							).tos();
					if (offset == 0 && (*i_first_member)->get_type()
						&& (*i_first_member)->get_type()->get_concrete_type()
						&& (*i_first_member)->get_type()->get_concrete_type()->get_tag()
							== DW_TAG_structure_type)
					{
						const type& t = ensure_type_node(*p_graph, 
							(*i_first_member)->get_type());
						t.edges.insert((way_to_reach_type){
							this->p_ds,
							t.off,
							way_to_reach_type::DOWNCAST_ZERO_OFFSET,
							this->off
							});
					}
					
				}
				
			} // fall through! to handle data members
			case DW_TAG_union_type:
			{
				// we can reach any of our subobjects/arms
				shared_ptr<spec::with_data_members_die> with_data_members
				 = dynamic_pointer_cast<spec::with_data_members_die>(p_t);
				assert(with_data_members);
				for (auto i_memb = with_data_members->member_children_begin();
					i_memb != with_data_members->member_children_end();
					++i_memb)
				{
					edges.insert((way_to_reach_type){
						this->p_ds,
						this->off,
						way_to_reach_type::SUBOBJECT, 
						ensure_type_node(*p_graph, (*i_memb)->get_type()).off
					});
				}
			} break;

			default: assert(false);
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
		cerr << "Saw an edge!" << endl;
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

int main(int argc, char **argv)
{
	assert(argc > 2);
	cerr << "Opening " << argv[1] << "..." << endl;
	std::ifstream in1(argv[1]);
	assert(in1);
	core::basic_root_die root1(fileno(in1));
	dwarf::lib::file df1(fileno(in1));
	dwarf::lib::dieset ds1(df1);
	
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

	vector< pair< Dwarf_Off, Dwarf_Off > > like_named_and_rep_compatible;
	collect_rep_compatible_pairs(
		ds1, ds2, 
		like_named,
		like_named_and_rep_compatible
	);
	
	
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
	
	set<type> graph2;
	construct_graph(graph2, like_named_and_rep_compatible, ds2, eds2, true);
	
	/* Now for each data type that we care about, invoke dfs on the graph
	 * to enumerate the set of type reachability paths starting from that type. */
	for (auto i_lr = like_named_and_rep_compatible.begin();
		i_lr != like_named_and_rep_compatible.end();
		++i_lr)
	{
		auto found_t1 = dynamic_pointer_cast<type_die>(ds1[i_lr->first]);
		assert(found_t1);
		auto found_t1_vertex = graph1.find(type(graph1, found_t1));
		assert(found_t1_vertex != graph1.end());
		auto t1_vertex = &*found_t1_vertex;
		
		std::map<
			boost::graph_traits<set<type> >::vertex_descriptor, 
			boost::default_color_type
		> underlying_dfs_node_color_map;
		auto dfs_color_map = boost::make_assoc_property_map(underlying_dfs_node_color_map);
		vector<path_explorer_t::path> paths;
		path_explorer_t path_explorer(&paths);
		path_explorer.root_vertex = t1_vertex;
		auto visitor = boost::visitor(path_explorer)
			.color_map(dfs_color_map)
			.root_vertex(t1_vertex);
		// run the DFS
		cerr << "Running DFS with root " << found_t1->summary() << endl;
		boost::depth_first_search(graph1, visitor);
		auto edges = out_edges(t1_vertex, graph1);
		for (auto i_out_edge = edges.first; i_out_edge != edges.second; ++i_out_edge)
		{
			cerr << "Root has out edge from 0x" << std::hex <<  i_out_edge->source
				<< " to " << i_out_edge->target << std::dec << endl;
		}

		//path_explorer.print_paths

		/* Now we have the two graphs, we depthfirst explore each one
		 * starting from each of the corresp'ing types, and
		 * for each path that we reach, we check that there is an analogous
		 * path in the other graph. 
		 * What then?
		 * For each path that does not have an analogous path,
		 * for each data type along that common path prefix that was in the possibly-shareable set,
		 * we can delete its pair from the set.
		 *
		 * FIXME: are we ensuring that all elements along the path are rep-compatible?
		 * Otherwise, we might have to track what offsets (say) were accessed on DEREF or SUBOBJECT
		 * etc..
		 * FIXME: are we conflating contexts
		 * with the treatment of pointers?
		 * i.e. all objects containing an int* can reach int
		 * HMm, so this seems okay.

		 * Is this enough for shareability? Have I faithfully implemented the intended
		 * algorithm ?*/
		/* DUMP for each data type x...
		 * "From x, I can reach:
		 *    Y, by path ...
		 *    Y, by path ...
		 *    Z, by path ...
		 *    Z, by path ...
		 */
		cerr << "Found " << paths.size() << " paths." << endl;
		for (auto i_path = paths.begin();
			i_path != paths.end();
			++i_path)
		{
			cerr << "From data type " << ds1[i_path->front().source]->summary()
				<< " we can reach " << ds1[i_path->back().target]->summary()
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
					default: assert(false);
				}

				cerr << " to " << ds1[i_hop->target]->summary() << "; ";

				if (last_hop)
				{
					// check that he last hop's target is our source
					assert(last_hop->target == i_hop->source);
				}

			}
			cerr << "END." << endl;
		}
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
