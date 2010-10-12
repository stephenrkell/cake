// #include <gcj/cni.h>
// #include <org/antlr/runtime/tree/Tree.h>
// #include <org/antlr/runtime/tree/CommonTree.h>
// #include <cakeJavaLexer.h>
// #include <cakeJavaParser.h>
// #include <cake/SemanticError.h>
// #include <iostream>
// #include <vector>
// #include <map>
// #undef EOF
// #include "cake.hpp"
// #include "util.hpp"
// #include "treewalk_helpers.hpp"
// #include <dwarf.h>
// #include <dwarfpp.h>
// 
// namespace cake
// {
// 	class dwarf_pred
// 	{
// 		dwarf::abi_information *db;
// 		std::vector<dwarf_pred> sub_preds;
// 	public:
// 		enum tri_state { TRUE, FALSE, DONT_CARE };
// 		typedef tri_state (*eval_event_handler)(dwarf_pred *, Dwarf_Off);
// 		// application
// 		static tri_state eval(dwarf_pred&, 
// 	
// 		// constructor
// 		dwarf_pred(dwarf::abi_information *db, kind p, ...);
// 		
// 		/* Examples from switch-simple.cake:
// 		 * 		.gtk_dialog_new : _ -> GtkDialog ptr;
// 		 * becomes
// 		 * 	MODULE_HAS_MEMBER(gtk_dialog_new) and MEMBER_IS_SUBPROGRAM(MODULE_GET_MEMBER(gtk_dialog_new))
// 		 *   and SUBPROGRAM_RETURNS_AT(MODULE_GET_MEMBER(
// 		 *
// 		 */
// 	
// 	
// 		enum kind {
// 			// module-specific
// 			MODULE_HAS_MEMBER
// 			MEMBER_IS_SUBPROGRAM,
// 			MEMBER_IS_VALUE,
// 			MEMBER_IS_TYPE,
// 			
// 			// value-specific
// 			VALUE_HAS_FORM, // form by DWARF offset
// 						
// 			// subprogram-specific
// 			SUBPROGRAM_HAS_PARAMETER,
// 			SUBPROGRAM_HAS_PARAMETER_AT,
// 			SUBPROGRAM_HAS_PARAMETER_NAMED,
// 			SUBPROGRAM_RETURNS,
// 			SUBPROGRAM_RETURNS_AT,
// 			SUBPROGRAM_RETURNS_NAMED,
// 			
// 			// type-specific
// 			FORM_IS_STRUCTURED,
// 			FORM_IS_CONJUNCTION, // struct
// 			FORM_IS_DISJUNCTION, // union
// 			FORM_IS_ARRAY, // associative
// 			FORM_IS_OBJECT, // C++/Java
// 			FORM_IS_POINTER,
// 			FORM_HAS_MEMBER,
// 			
// 			// functions
// 			MODULE_GET_MEMBER_NAMED,
// 			MODULE_GET_MEMBER_AT,
// 			VALUE_GET_MEMBER_NAMED,
// 			VALUE_GET_MEMBER_AT,
// 			SUBPROGRAM_GET_PARAMETER_NAMED,
// 			SUBPROGRAM_GET_PARAMETER_AT,
// 			SUBPROGRAM_GET_RETURN_NAMED,
// 			SUBPROGRAM_GET_RETURN_AT			
// 			
// 			// connectives
// 			NOT,
// 			AND,
// 			OR,
// 			XOR
// 			
// 			
// 		
// 		} primitives;
// 		
// 		union {
// 			struct {
// 			
// 			} unary_connective;
// 			struct {
// 			
// 			} binary_connective;
// 			struct {
// 			
// 			} unary_function;
// 			struct {
// 			
// 			} binary_function;
// 			struct {
// 			
// 			} unary_builtin_pred;
// 			struct {
// 			
// 			} binary_builtin_pred;
// 			Dwarf_Off dwarf_el;
// 		}
// 	};
// }	
// 	
