#include <cstdlib>

extern "C" {
void walk_bfs(int object_rep, void *object, int object_form, int co_object_rep,
	void (*on_blacken)(int, void*, int, int, int), int arg_n_minus_1, int arg_n);
}
#include "rep_man-shared.h"
#include "rep_man_tables.h"

#include <map>
#include <deque>
#include <cstdio>

/* debugging */
FILE* debug_out = NULL;
#ifndef DEBUGGING_OUTPUT_FILENAME
#define DEBUGGING_OUTPUT_FILENAME NULL
#endif
const char *debugging_output_filename = DEBUGGING_OUTPUT_FILENAME;
#define DEBUG_GUARD(stmt) if (debug_out != NULL) { stmt; }

/* C++ prototypes */

// our representation of nodes in the graph
typedef std::pair<void*, std::pair<int, int> > node_rec; 

void build_adjacency_list_recursive(std::deque<node_rec>& adj_u,
	void *obj_u, int rep, int subobject_offset, int subobject_form);

enum node_colour { WHITE, GREY, BLACK };

void walk_bfs(int object_rep, void *object, int object_form, int co_object_rep,
	void (*on_blacken)(int, void*, int, int, int), int arg_n_minus_1, int arg_n)
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

	q.push_back(std::make_pair(object, std::make_pair(object_rep, object_form)));
	while (!q.empty())
	{
		node_rec u = q.front(); 
		q.pop_front();
		// handy aliases for elements of u
		void *& u_obj_addr = u.first;
		int& u_rep = u.second.first;
		int& u_form = u.second.second;
		
		/* create the adjacency list for u, by flattening the subobject hierarchy */
		std::deque<node_rec> adj_u;
		build_adjacency_list_recursive(adj_u, u_obj_addr, u_rep, /* subobj offset */ 0, u_form);
		/* ^-- this starts at the top-level subobject, i.e. the object, so it builds
		 * the complete adjacency list for this node. */
				
		/* now that we have the adjacency list, process the adjacent nodes */
		for (std::deque<node_rec>::iterator i = adj_u.begin();
			i != adj_u.end();
			i++)
		{		
			node_rec& v = *i; // alias the noderec
			void *& v_obj_addr = v.first;
			int& v_rep = v.second.first;
			int& v_form = v.second.second;
			
			/* we didn't initialise all nodes' colours to white, so treat 'no colour' <=> white */
			if (colours.find(v_obj_addr) == colours.end() || colours[v_obj_addr] == WHITE)
			{
				colours[v_obj_addr] = GREY;
				distances[v_obj_addr] = distances[u_obj_addr] + 1;
				predecessors[v_obj_addr] = u_obj_addr;
				q.push_back(v); // the queue takes its own copy of v
			}
		}
		
		/* blacken u, and call the function for it */
		colours[u_obj_addr] = BLACK;
		// (int do_not_use, void *object, int form, int object_rep, int co_object_rep)
		on_blacken(u_rep, u_obj_addr, u_form, arg_n_minus_1, arg_n);
		//                            ^-- this is the form of the object we're blackening
	}
	DEBUG_GUARD(fflush(debug_out))
	DEBUG_GUARD(fprintf(debug_out, "}\n"))
}

/* This function builds an adjacency list for the current node, by adding
 * *all* nodes, not just (despite the name) those pointed to by subobjects.
 * i.e. the top-level object is a zero-degree subobject. */
void build_adjacency_list_recursive(
	std::deque<node_rec>& adj_u,
	void *obj_u, int rep, int start_byte_offset, int start_subobject_form)
{
	//fprintf(stderr, "Descending through subobjects of object at %08x, "
	//	"currently at subobject offset %x of form %s\n",
	//	(unsigned)obj_u, start_byte_offset, object_forms[start_subobject_form]);
	
	/* We start with the deepest subobject(s), so make a recursive call
	 * for each subobject at the top-level. */
	for (int i = 0; 
    	get_subobject_offset(rep, start_subobject_form, i) != (size_t) -1;
//		subobject_offsets[rep][start_subobject_form][i] != (size_t) -1;
		i++)
	{
		build_adjacency_list_recursive(adj_u, obj_u, rep,
			start_byte_offset + //subobject_offsets[rep][start_subobject_form][i],
            				get_subobject_offset(rep, start_subobject_form, i),
			//subobject_forms  [rep][start_subobject_form][i]);
            get_subobject_form(rep, start_subobject_form, i));
	}

	//fprintf(stderr, "Finished recursive descent of object at %08x, ",
	//	(unsigned)obj_u);


	/* Now add node_recs for all the pointers in fields defined by
	 * the current subobject. */
	for  (int i = 0;
		//derefed_offsets[rep][start_subobject_form][i] != (size_t) -1;
        get_derefed_offset(rep, start_subobject_form, i) != (size_t) -1;
		i++)
	{
		// get the address of the pointed-to object
		void *pointed_to_object = 
			(void*)*(unsigned*)((char*)obj_u + start_byte_offset + //derefed_offsets[rep][start_subobject_form][i]);
            													get_derefed_offset(rep, start_subobject_form, i));
		if (pointed_to_object != 0)
		{
			DEBUG_GUARD(fprintf(debug_out, "\t%s_rep%d_at_%p -> %s_rep%d_at_%p;\n", 
				//object_forms[start_subobject_form], rep, obj_u, 
                get_object_form(start_subobject_form), rep, obj_u,
//				object_forms[derefed_forms[rep][start_subobject_form][i]], rep, pointed_to_object))
				get_object_form(get_derefed_form(rep, start_subobject_form, i)), rep, pointed_to_object))

			adj_u.push_back(std::make_pair(
				// the pointer value within structure u
				pointed_to_object,
					std::make_pair(
						// the rep of this value
						rep,
						// the form of this value
						//derefed_forms[rep][start_subobject_form][i]
                        get_derefed_form(rep, start_subobject_form, i)
					)
				)
			);
			//fprintf(stderr, "Added a pointed-to object at %08x, form %s\n",
			//	(unsigned)adj_u.back().first, object_forms[adj_u.back().second.second]);
		}
		//else fprintf(stderr, "Skipping null object\n");		
	}
}
