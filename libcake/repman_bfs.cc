#include <cstdlib>

#include <libreflect.hpp>
#include "repman.h"
//#include "repman_image.hh"

#include <map>
#include <deque>
#include <cstdio>
#include <sstream>

/* debugging */
FILE* debug_out = NULL;
#ifndef DEBUGGING_OUTPUT_FILENAME
#define DEBUGGING_OUTPUT_FILENAME NULL
#endif
const char *debugging_output_filename = DEBUGGING_OUTPUT_FILENAME;
#define DEBUG_GUARD(stmt) if (debug_out != NULL) { stmt; }

using boost::shared_ptr;
using boost::dynamic_pointer_cast;
using namespace dwarf::spec;
using pmirror::process_image;

/* C++ prototypes */

// our representation of nodes in the graph
typedef struct 
{ 
	void* obj; 
	int rep; 
	shared_ptr<type_die> dwarf_type; 
} node_rec; 

void build_adjacency_list_recursive(std::deque<node_rec>& adj_u,
	void *obj_u, int rep, int subobject_offset, shared_ptr<type_die> t);

enum node_colour { WHITE, GREY, BLACK };

void walk_bfs(int object_rep, void *object, /*int object_form,*/ int co_object_rep,
	void (*on_blacken)(void*, int, int), int arg_n_minus_1, int arg_n)
{
	/* We are doing breadth-first search through the object graph rooted at object,
	 * using object_ref_derefed_offsets[object_rep][object_form] to make an adjacency list
	 * out of the actual object graph.*/
	 
	/* init debug output */
	if (debug_out == NULL && debugging_output_filename != NULL)
	{
		debug_out = fopen(debugging_output_filename, "r+");
	}
	DEBUG_GUARD(fprintf(debug_out, "digraph view_from_%p {\n", object))
	
	// map of colourings
	std::map<void*, node_colour> colours;
	std::map<void*, int> distances;
	std::map<void*, void*> predecessors;
	std::deque<node_rec> q;
	
	colours[object] = GREY;
	distances[object] = 0;
	predecessors[object] = 0;

	if (object == 0) return;
	
	/* Find out object's precise DWARF type and starting address. */
	process_image::addr_t object_actual_start_addr;
	auto descr = pmirror::self.discover_object_descr(
		(process_image::addr_t) object, shared_ptr<type_die>(), &object_actual_start_addr);
	assert(descr);
	void *object_actual_start = (void*) object_actual_start_addr;
	/* descr might be a subprogram... if so, nothing is adjacent, so we can null it */
	auto dwarf_type = dynamic_pointer_cast<type_die>(descr);
	
	/* Now we have the starting object -- build an adjacency list.... */
	q.push_back((node_rec) { object_actual_start, object_rep, dwarf_type });
	while (!q.empty())
	{
		node_rec u = q.front(); 
		q.pop_front();
		
		/* create the adjacency list for u, by flattening the subobject hierarchy */
		std::deque<node_rec> adj_u;
		build_adjacency_list_recursive(adj_u, u.obj, u.rep, /* subobj offset */ 0, u.dwarf_type);
		/* ^-- this starts at the top-level subobject, i.e. the object, so it builds
		 * the complete adjacency list for this node. */
				
		/* now that we have the adjacency list, process the adjacent nodes */
		for (std::deque<node_rec>::iterator i = adj_u.begin();
			i != adj_u.end();
			i++)
		{		
			node_rec& v = *i; // alias the noderec

			/* we didn't initialise all nodes' colours to white, so treat 'no colour' <=> white */
			if (colours.find(v.obj) == colours.end() || colours[v.obj] == WHITE)
			{
				colours[v.obj] = GREY;
				distances[v.obj] = distances[v.obj] + 1;
				predecessors[v.obj] = v.obj;
				q.push_back(v); // the queue takes its own copy of v
			}
		}
		
		/* blacken u, and call the function for it */
		colours[u.obj] = BLACK;
		// (int do_not_use, void *object, int form, int object_rep, int co_object_rep)
		on_blacken(u.obj, arg_n_minus_1, arg_n);
	}
	DEBUG_GUARD(fflush(debug_out))
	DEBUG_GUARD(fprintf(debug_out, "}\n"))
}

static std::string get_type_string(shared_ptr<type_die> t);
static std::string get_type_string(shared_ptr<type_die> t)
{
	std::ostringstream s;
	if (t) 
	{
		if (t->get_name()) s << *t->get_name();
		else s << "(anonymous type at 0x" << std::hex << t->get_offset() 
			<< std::dec << ")";
	}
	else s << "(no type discovered)";
	
	return s.str();
}
			
/* This function builds an adjacency list for the current node, by adding
 * *all* nodes, not just (despite the name) those pointed to by subobjects.
 * i.e. the top-level object is a zero-degree subobject. */
void build_adjacency_list_recursive(
	std::deque<node_rec>& adj_u,
	void *obj_u, int rep, int start_byte_offset, shared_ptr<type_die> type_at_this_offset)
{
	//fprintf(stderr, "Descending through subobjects of object at %08x, "
	//	"currently at subobject offset %x of form %s\n",
	//	(unsigned)obj_u, start_byte_offset, object_forms[start_subobject_form]);
	
	/* We start with the deepest subobject(s), so make a recursive call
	 * for each subobject at the top-level. */
// 	for (int i = 0; 
//     	get_subobject_offset(rep, start_subobject_form, i) != (size_t) -1;
// //		subobject_offsets[rep][start_subobject_form][i] != (size_t) -1;
// 		i++)

	// If someone tries to walk_bfs from a function pointer, we will try to
	// bootstrap the list from a queue consisting of a single object (the function)
	// and no type. If so, the list is already complete (i.e. empty), so return
	if (!type_at_this_offset) return;

	auto structured_type_at_this_offset
	 = dynamic_pointer_cast<dwarf::spec::with_data_members_die>(type_at_this_offset);
	 
	// We can assert this because the loop below only recurses on
	// structured-typed members. We don't need to worry about getting ints.
	assert(structured_type_at_this_offset);
	for (auto i_subobj = structured_type_at_this_offset->member_children_begin();
			i_subobj != structured_type_at_this_offset->member_children_end();
			++i_subobj)
	{
		/* This will iterate through all members. 
		 * We only want those with structured types. */
		if (!((*i_subobj)->get_type() && 
			dynamic_pointer_cast<dwarf::spec::with_data_members_die>((*i_subobj)->get_type())))
		{
			continue;
		}
	
		build_adjacency_list_recursive(adj_u, obj_u, rep,
			/* offset from adj_u that we are recursing on: */
				start_byte_offset + (*i_subobj)->calculate_addr(0, 0),
			/* DWARF type of the object at that address:  */
				(*i_subobj)->get_type()
		);
				
//            				get_subobject_offset(rep, start_subobject_form, i, i_subobj->get_type()),
//			//subobject_forms  [rep][start_subobject_form][i]);
//            get_subobject_form(rep, start_subobject_form, i));
	}

	//fprintf(stderr, "Finished recursive descent of object at %08x, ",
	//	(unsigned)obj_u);


	/* Now add node_recs for all the pointers in fields defined by
	 * the current subobject. */
//	for  (int i = 0;
//		//derefed_offsets[rep][start_subobject_form][i] != (size_t) -1;
//        get_derefed_offset(rep, start_subobject_form, i) != (size_t) -1;
//		i++)

	for (auto i_ptrmemb = structured_type_at_this_offset->member_children_begin();
			i_ptrmemb != structured_type_at_this_offset->member_children_end();
			++i_ptrmemb)
	{
		/* This will iterate through all members. 
		 * We only want those with pointer types. */
		if (!((*i_ptrmemb)->get_type() && 
			(dynamic_pointer_cast<dwarf::spec::pointer_type_die>((*i_ptrmemb)->get_type())
			|| dynamic_pointer_cast<dwarf::spec::reference_type_die>((*i_ptrmemb)->get_type()))
			))
		{
			continue;
		}

		// get the address of the pointed-to object
		void *pointed_to_object = 
			*(void**)(
				(char*)obj_u 
				+ start_byte_offset 
				+ //derefed_offsets[rep][start_subobject_form][i]);
				  (*i_ptrmemb)->calculate_addr(0, 0)
			);
		if (pointed_to_object != 0)
		{
			/* Find out object's precise DWARF type and starting address. */
			process_image::addr_t object_actual_start_addr;
			auto descr = pmirror::self.discover_object_descr(
				(process_image::addr_t) pointed_to_object, 
				shared_ptr<type_die>(), &object_actual_start_addr);
			void *object_actual_start = (void*) object_actual_start_addr;
			/* descr might be a subprogram... if so, nothing is adjacent, so we can null it */
			auto dwarf_type = dynamic_pointer_cast<type_die>(descr);

			DEBUG_GUARD(fprintf(debug_out, "\t%s_rep%d_at_%p -> %s_rep%d_at_%p + %d;\n", 
				get_type_string(type_at_this_offset).c_str(),
				rep, obj_u,
				get_type_string(dwarf_type).c_str(), 
				rep, object_actual_start, 
				(int)((char*)pointed_to_object - (char*)object_actual_start)))


			adj_u.push_back((node_rec){
				// the pointer value within structure u
				object_actual_start,
				rep,
				dwarf_type
			});
			//fprintf(stderr, "Added a pointed-to object at %08x, form %s\n",
			//	(unsigned)adj_u.back().first, object_forms[adj_u.back().second.second]);
		}
		//else fprintf(stderr, "Skipping null object\n");		
	}
}
