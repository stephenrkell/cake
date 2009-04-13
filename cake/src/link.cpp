#include <gcj/cni.h>
#include <org/antlr/runtime/tree/Tree.h>
#include <org/antlr/runtime/tree/CommonTree.h>
#include <cakeJavaLexer.h>
#include <cakeJavaParser.h>
#include <cake/SemanticError.h>
#include <iostream>
#include <vector>
#include <map>
#undef EOF
#include "cake.hpp"

namespace cake
{
	void link_derivation::write_makerules(std::ostream& out)
	{
		extract_event_correspondences();
		
		extract_value_correspondences();
	
		// if it's a link:
		compute_function_bindings(); // event correspondences: which calls should be bound (possibly indirectly) to which definitions?
		
		compute_form_value_correspondences(); 
			// use structural + lossless rules, plus provided correspondences
			// not just the renaming-only correspondences!
			// since other provided correspondences (e.g. use of "as") might bring compatibility "back"
			// in other words we need to traverse *all* correspondences, forming implicit ones by name-equiv
			// How do we enumerate all correspondences? need a starting set + transitive closure
			// starting set is set of types passed in bound functions?
			// *** no, just walk through the DWARF info -- it's finite. add explicit ones first?
			
			// in the future, XSLT-style notion of correspondences, a form correspondence can be
			// predicated on a context... is this sufficient expressivity for
			// 1:N, N:1 and N:M object identity conversions? we'll need to tweak the notion of
			// co-object relation also, clearly

		compute_static_value_correspondences();
			// -- note that when we support stubs and sequences, they will imply some nontrivial
			// value correspondences between function values. For now it's fairly simple 
			// -- wrapped if not in same rep domain, else not wrapped
		
		compute_dwarf_type_compatibility(); 
			// of all the pairs of corresponding forms, which are binary-compatible?
		
		compute_rep_domains(); // graph colouring ahoy
		
		output_rep_conversions(); // for the non-compatible types that need to be passed
		
		compute_interposition_points(); // which functions need wrapping between which domains
		
		output_symbol_renaming_rules(); // how to implement the above for static references, using objcopy and ld options
		
		output_formgens(); // one per rep domain
		
		output_wrappergens(); // one per inter-rep call- or return-direction, i.e. some multiple of 2!
		
		output_static_co_objects(); // function and global references may be passed dynamically, 
			// as may stubs and wrappers, so the co-object relation needs to contain them.
			// note that I really mean "references to statically-created objects", **but** actually
			// globals need not be treated any differently, unless there are two pre-existing static
			// obejcts we want to correspond, or unless they should be treated differently than their
			// DWARF-implied form would entail. 
			// We treat functions specially so that we don't have to generate rep-conversion functions
			// for functions -- we expand all the rep-conversion ahead-of-time by generating wrappers.
			// This works because functions are only ever passed by reference (identity). If we could
			// construct functions at run-time, things would be different!
			
		// output_stubs(); -- compile stubs from stub language expressions
		
	}
	
	void link_derivation::extract_event_correspondences() {}
		
	void link_derivation::extract_value_correspondences() {}
	void link_derivation::compute_function_bindings() {}
		
	void link_derivation::compute_form_value_correspondences() {}
	void link_derivation::compute_static_value_correspondences() {}

	void link_derivation::compute_dwarf_type_compatibility() {}		
	void link_derivation::compute_rep_domains() {}
		
	void link_derivation::output_rep_conversions() {}		
	void link_derivation::compute_interposition_points() {}
		
	void link_derivation::output_symbol_renaming_rules() {}		
	void link_derivation::output_formgens() {}
		
	void link_derivation::output_wrappergens() {}		
	void link_derivation::output_static_co_objects() {}	
}
