#include "util.hpp"
#include "parser.hpp"
#include "module.hpp"
#include "link.hpp"
#include "valconv.hpp"
#include "wrapsrc.hpp"
#include <sstream>
#include <cmath>

using boost::dynamic_pointer_cast;
using boost::shared_ptr;
using dwarf::spec::member_die;
using dwarf::spec::type_die;
using dwarf::spec::with_named_children_die;

namespace cake
{
	std::string value_conversion::source_fq_namespace() const
	{ return w.ns_prefix + "::" + w.m_d.name_of_module(source); }
	std::string value_conversion::sink_fq_namespace() const
	{ return w.ns_prefix + "::" + w.m_d.name_of_module(sink); }
	
	boost::shared_ptr<value_conversion> create_value_conversion(module_ptr source,
            boost::shared_ptr<dwarf::spec::type_die> source_data_type,
            antlr::tree::Tree *source_infix_stub,
            module_ptr sink,
            boost::shared_ptr<dwarf::spec::type_die> sink_data_type,
            antlr::tree::Tree *sink_infix_stub,
            antlr::tree::Tree *refinement,
			bool source_is_on_left,
			antlr::tree::Tree *corresp)
	{
		assert(false);
	}
	
	value_conversion::value_conversion(wrapper_file& w,
		srk31::indenting_ostream& out,
		const basic_value_conversion& basic)
	 : basic_value_conversion(basic), w(w), m_out(out),
	 	source_concrete_type(source_data_type->get_concrete_type()),
		sink_concrete_type(sink_data_type->get_concrete_type()),
		from_typename(source_concrete_type ? w.get_type_name(source_concrete_type) : "(no concrete type)"),
		to_typename(sink_concrete_type ? w.get_type_name(sink_concrete_type) : "(no concrete type)")
	{
		assert(&w.m_out == &out);
	}
			
	void value_conversion::emit_preamble()
	{
        emit_header(boost::optional<std::string>(to_typename), false, false); // no struct keyword, no template<>
        m_out << "::" << std::endl;
        emit_signature(false, false); // NO return type, NO default argument
        m_out << std::endl << "{" << std::endl;
        m_out.inc_level();
	}
	
	void value_conversion::emit_header(boost::optional<std::string> return_typename, 
		bool emit_struct_keyword/* = true*/,
		bool emit_template_prefix /* = true */,
		bool emit_return_typename /* = true */
		)
	{
		m_out << (emit_template_prefix ? "template <>\n" : "" )
            << (emit_struct_keyword ? "struct " : "")
			<< ((emit_return_typename && return_typename) ? *return_typename + "\n" : "")
			<< "value_convert<"
            << from_typename // From
            << ", "
            << to_typename // To
            << ", "
			<< source_fq_namespace() << "::marker, " // FromComponent
			<< sink_fq_namespace() << "::marker, " // ToComponent
            << "0" // RuleTag
            << ">" << std::endl;
	}	
	void value_conversion::emit_signature(bool emit_return_type/* = true*/, 
		bool emit_default_argument /* = true */) 
	{
        m_out << (emit_return_type ? to_typename : "") 
			<< " operator()(const " << from_typename << "& __cake_from, " 
			<< to_typename << "*__cake_p_to"
			<< (emit_default_argument ? " = 0" : "")
			<< ") const";
	}
	
	void value_conversion::emit_forward_declaration()
	{
        emit_header(false, true);
		m_out << "{" << std::endl;
		m_out.inc_level();
		emit_signature();
		m_out << ";";
        m_out.dec_level();
        m_out << "};" << std::endl;
	}

	void value_conversion::emit_function_name()
	{
		/* m_out.flush();
		emit_header(false, false, false);
		m_out.flush();
		m_out << "::operator()";
		m_out.flush();*/ 
		m_out << "value_convert_function<"
            << from_typename // From
            << ", "
            << to_typename // To
            << ", "
			<< source_fq_namespace() << "::marker, " // FromComponent
			<< sink_fq_namespace() << "::marker, " // ToComponent
            << "0" // RuleTag
            << ">" << std::endl;

	}
	
	// HACK to output the type of a pointer to this conversion function
	void value_conversion::emit_cxx_function_ptr_type(
		boost::optional<const std::string& > decl_name)
	{
		m_out << "void *(*" << (decl_name ? *decl_name : "")
			<< ")(" << from_typename << "* ,"
				<< to_typename << " *)" << std::endl;
	}
	
	
	std::vector< std::pair < boost::shared_ptr<dwarf::spec::type_die>,
		                             boost::shared_ptr<dwarf::spec::type_die>
									>
							> value_conversion::get_dependencies()
	{
		return std::vector< std::pair < boost::shared_ptr<dwarf::spec::type_die>,
		                             boost::shared_ptr<dwarf::spec::type_die>
									> >();
	}
		
	void reinterpret_value_conversion::emit_body()
	{
		m_out << "if (__cake_p_to) *__cake_p_to = *reinterpret_cast<const " 
			<< w.get_type_name(sink_data_type) << "*>(&__cake_from);" << std::endl
			<< "return *reinterpret_cast<const " 
			<< w.get_type_name(sink_data_type) << "*>(&__cake_from);" << std::endl;
	}
			
	struct member_has_name
	 : public std::unary_function<boost::shared_ptr<dwarf::spec::member_die> , bool>
	{
		std::string m_name;
		member_has_name(const std::string& name) : m_name(name) {}
		bool operator()(boost::shared_ptr<dwarf::spec::member_die> p_member) const
		{
			return p_member->get_name() && (*p_member->get_name() == m_name);
		}
	};
	
	structural_value_conversion::member_mapping_rule::member_mapping_rule(
		const member_mapping_rule_base& val)
	: member_mapping_rule_base(val) 
	{}
	
	void structural_value_conversion::member_mapping_rule::check_sanity()
	{
		/* Check the sanity of this rule. In particular ,we should abort
		 * with an error message if there is no correspondence
		 * between the types of the corresponded fields, unless they are 
		 * pointers or base types.
		 * 
		 * This is tricky because if our source is an expression, not a single
		 * field, then its type is known only to the C++ compiler. For now,
		 * check that there is SOME corresponding type to the target type.
		 * If we have a simple source field, we can check more precisely. */
		
		auto found_target = dynamic_pointer_cast<with_named_children_die>(owner->sink_data_type)
			->resolve(target.begin(), target.end());
		assert(found_target);
		
		auto found_target_field = dynamic_pointer_cast<member_die>(found_target);
		assert(found_target_field && found_target_field->get_type());
		
		auto found_target_field_type = *found_target_field->get_type();
		assert(found_target_field_type);
		
		shared_ptr<member_die> found_source_field;
		shared_ptr<type_die> found_source_field_type;
		if (unique_source_field)
		{
			auto found_source = dynamic_pointer_cast<with_named_children_die>(owner->source_data_type)
				->resolve(unique_source_field->begin(), unique_source_field->end());
			assert(found_source);
			
			found_source_field = dynamic_pointer_cast<member_die>(found_source);
			assert(found_source_field && found_source_field->get_type());
			
			found_source_field_type = *found_source_field->get_type();
			assert(found_source_field_type);
		}
		
		/* Check that there exists some correspondence,
		 * or it's a pointer or base type. */
		auto value_corresps = owner->w.m_d.val_corresps_for_iface_pair(
			owner->w.m_d.sorted(owner->source, owner->sink));
		
		typedef link_derivation::val_corresp_map_t::value_type ent;
		
		if (!(
			(std::find_if(
					value_corresps.first, value_corresps.second, 
					[found_target_field_type, found_source_field_type](const ent& arg) { 
						/* std::cerr << "Testing corresp with source "
							<< *(arg.second->source_data_type)
							<< " and sink " 
							<< *(arg.second->sink_data_type)
							<< std::endl; */
						return (arg.second->sink_data_type->iterator_here()
									== found_target_field_type->iterator_here()
							) && 
							(!found_source_field_type || 
								arg.second->source_data_type->iterator_here()
									== found_source_field_type->iterator_here()); 
					}
				) != value_corresps.second)
		||      (found_target_field_type->get_concrete_type()
			&&   found_target_field_type->get_concrete_type()->get_tag() == DW_TAG_pointer_type)
		||      (found_target_field_type->get_concrete_type()
			&&   found_target_field_type->get_concrete_type()->get_tag() == DW_TAG_reference_type)
		||      (found_target_field_type->get_concrete_type()
			&&   found_target_field_type->get_concrete_type()->get_tag() == DW_TAG_base_type)
		
			))
		{
			std::cerr << "Exception refers to target field " << *found_target_field
				<< " and ";
			if (unique_source_field) std::cerr << " and source field " << *found_source_field;
			else std::cerr << " with no unique source";
			std::cerr << std::endl;
			
			RAISE(owner->corresp, "corresponding fields lack corresponding type");
		}
	}

	structural_value_conversion::structural_value_conversion(wrapper_file& w,
			srk31::indenting_ostream& out, 
			const basic_value_conversion& basic)
		: value_conversion(w, out, basic), source_module(w.module_of_die(source_data_type)),
			target_module(w.module_of_die(sink_data_type)),
			modules(link_derivation::sorted(std::make_pair(source_module, target_module)))
	{
		/* Find explicitly assigned-to fields:
		 * the map is from the assigned-to- field
		 * to the rule details.
		 * NOTE that this includes non-toplevel field corresps.  */
		//std::map<definite_member_name, member_mapping_rule> field_corresps;
		if (refinement)
		{
			INIT;
			FOR_ALL_CHILDREN(refinement)
			{
				ALIAS2(n, refinementRule);
				switch(GET_TYPE(refinementRule))
				{
					case (CAKE_TOKEN(BI_DOUBLE_ARROW)):
					{
						INIT;
						BIND2(refinementRule, leftValuePattern);
						BIND2(refinementRule, leftInfixStub);
						BIND2(refinementRule, rightInfixStub);
						BIND2(refinementRule, rightValuePattern);
						explicit_field_corresps.insert(std::make_pair(
							read_definite_member_name(
								source_is_on_left ? rightValuePattern : leftValuePattern),
								(member_mapping_rule_base)
								{
									/* structural_value_conversion *owner */
										this,
									/* definite_member_name target; */ 
										read_definite_member_name(source_is_on_left ? rightValuePattern : leftValuePattern),
									/* boost::optional<definite_member_name> unique_source_field; */
										GET_TYPE(source_is_on_left ? leftInfixStub : rightInfixStub) == CAKE_TOKEN(DEFINITE_MEMBER_NAME) ?
											read_definite_member_name(source_is_on_left ? leftInfixStub : rightInfixStub)
											: boost::optional<definite_member_name>(),
									/* antlr::tree::Tree *stub; */
										source_is_on_left ? leftValuePattern : rightValuePattern,
									/* module_ptr pre_context; */
										source_is_on_left ? w.module_of_die(source_data_type) : w.module_of_die(sink_data_type),
									/* antlr::tree::Tree *pre_stub; */
										source_is_on_left ? leftInfixStub : rightInfixStub,
									/* module_ptr post_context; */
										source_is_on_left ? w.module_of_die(sink_data_type) : w.module_of_die(source_data_type),
									/* antlr::tree::Tree *post_stub; */
										source_is_on_left ? rightInfixStub : leftInfixStub
								}));
					}
					break;
					case (CAKE_TOKEN(LR_DOUBLE_ARROW)):
					{
						assert(source_is_on_left);
						INIT;
						BIND2(refinementRule, leftStubExpr);
						BIND2(refinementRule, leftInfixStub);
						BIND2(refinementRule, rightInfixStub);
						BIND2(refinementRule, rightValuePattern);
						explicit_field_corresps.insert(std::make_pair(
							read_definite_member_name(rightValuePattern),
							(member_mapping_rule_base)
							{
								/* structural_value_conversion *owner, */
									this,
								/* definite_member_name target; */ 
									read_definite_member_name(rightValuePattern),
								/* boost::optional<definite_member_name> unique_source_field; */
									GET_TYPE(leftStubExpr) == CAKE_TOKEN(DEFINITE_MEMBER_NAME) ?
										read_definite_member_name(leftStubExpr)
										: boost::optional<definite_member_name>(),
								/* antlr::tree::Tree *stub; */
									leftStubExpr,
								/* module_ptr pre_context; */
									w.module_of_die(source_data_type),
								/* antlr::tree::Tree *pre_stub; */
									leftInfixStub,
								/* module_ptr post_context; */
									w.module_of_die(sink_data_type),
								/* antlr::tree::Tree *post_stub; */
									rightInfixStub
							}));

					}
					break;
					case (CAKE_TOKEN(RL_DOUBLE_ARROW)):
					{
						assert(!source_is_on_left);
						INIT;
						BIND2(refinementRule, leftValuePattern);
						BIND2(refinementRule, leftInfixStub);
						BIND2(refinementRule, rightInfixStub);
						BIND2(refinementRule, rightStubExpr);
						explicit_field_corresps.insert(std::make_pair(
							read_definite_member_name(leftValuePattern),
							(member_mapping_rule_base)
							{
								/* structural_value_conversion *owner, */
									this,
								/* definite_member_name target; */ 
									read_definite_member_name(leftValuePattern),
								/* boost::optional<definite_member_name> unique_source_field; */
									GET_TYPE(rightStubExpr) == CAKE_TOKEN(DEFINITE_MEMBER_NAME) ?
										read_definite_member_name(rightStubExpr)
										: boost::optional<definite_member_name>(),
								/* antlr::tree::Tree *stub; */
									rightStubExpr,
								/* module_ptr pre_context; */
									w.module_of_die(sink_data_type),
								/* antlr::tree::Tree *pre_stub; */
									rightInfixStub,
								/* module_ptr post_context; */
									w.module_of_die(source_data_type),
								/* antlr::tree::Tree *post_stub; */
									leftInfixStub
							}));

					}
					break;
					case (CAKE_TOKEN(LR_DOUBLE_ARROW_Q)):
					{
						assert(source_is_on_left);
						INIT;
						BIND2(refinementRule, leftStubExpr);
						BIND2(refinementRule, leftInfixStub);
						BIND2(refinementRule, rightInfixStub);
						BIND2(refinementRule, rightValuePattern);

						assert(false);
					}
					break;
					case (CAKE_TOKEN(RL_DOUBLE_ARROW_Q)):
					{
						assert(!source_is_on_left);
						INIT;
						BIND2(refinementRule, leftValuePattern);
						BIND2(refinementRule, leftInfixStub);
						BIND2(refinementRule, rightInfixStub);
						BIND2(refinementRule, rightStubExpr);

						assert(false);
					}
					break;
					default: RAISE(refinementRule, "expected a correspondence operator");
					add_this_one:

					break;
				}
			}
		}

		/* Which of these are toplevel? 
		 * These are the toplevel explicit mappings. */
		//std::multimap<std::string, member_mapping_rule*> explicit_toplevel_mappings;
		for (auto i_mapping = explicit_field_corresps.begin(); 
			i_mapping != explicit_field_corresps.end();
			i_mapping++)
		{
			if (i_mapping->first.size() == 1)
			{
				explicit_toplevel_mappings.insert(
					std::make_pair(
						i_mapping->first.at(0), &i_mapping->second));
			}
			else
			{
				std::cerr << "WARNING: found a non-toplevel mapping (FIXME)" << std::endl;
			}
		}
		/* The NON-toplevel mappings are useful for our dependencies only.
		 * We should collect them togther into FIXME the a dependencies description somehow. */

		/* Name-match toplevel subtrees not mentioned so far. */
		for (auto i_member
			 = boost::dynamic_pointer_cast<dwarf::spec::structure_type_die>(sink_data_type)
			 	->member_children_begin();
			i_member != boost::dynamic_pointer_cast<dwarf::spec::structure_type_die>(sink_data_type)
			 	->member_children_end();
			i_member++)
		{
			assert((*i_member)->get_name());
			if (name_matched_mappings.find(*(*i_member)->get_name())
				== name_matched_mappings.end())
			{
				/* We have a name-matching candidate -- look for like-named
				 * field in opposing structure. */

				auto source_as_struct = boost::dynamic_pointer_cast<dwarf::spec::structure_type_die>(
					source_data_type);

				auto found = std::find_if(source_as_struct->member_children_begin(),
					source_as_struct->member_children_end(),
					member_has_name(*(*i_member)->get_name()));
				if (found != source_as_struct->member_children_end())
				{
					std::cerr << "Matched a name " << *(*i_member)->get_name()
						<< " in DIEs " << *source_as_struct
						<< " and " << *sink_data_type
						<< std::endl;
					name_matched_mappings.insert(std::make_pair(
						*(*i_member)->get_name(),
						(member_mapping_rule_base){
							/* structural_value_conversion *owner, */
								this,
							/* definite_member_name target; */ 
								definite_member_name(1, *(*i_member)->get_name()),
							/* boost::optional<definite_member_name> unique_source_field; */
								definite_member_name(1, *(*i_member)->get_name()),
							/* antlr::tree::Tree *stub; */ 
								make_definite_member_name_expr(
									definite_member_name(1, *(*i_member)->get_name())),
							/* module_ptr pre_context; */ 
								source_module,
							/* antlr::tree::Tree *pre_stub; */ 
								0,
							/* module_ptr post_context; */ 
								target_module,
							/* antlr::tree::Tree *post_stub; */ 
								0
						}));
				}
				else
				{
					std::cerr << "Didn't match name " << *(*i_member)->get_name()
						<< " from DIE " << *sink_data_type
						<< " in DIE " << *source_as_struct
						<< std::endl;
				}
			}
		}
	}

	std::vector< value_conversion::dep > structural_value_conversion::get_dependencies()
	{
		std::set<dep> working;
	
		// for each field (subobject) that corresponds, we require
		// a defined conversion
		for (auto i_mapping = name_matched_mappings.begin();
				i_mapping != name_matched_mappings.end();
				i_mapping++)
		{
			auto pair = std::make_pair(
					*boost::dynamic_pointer_cast<dwarf::spec::member_die>(
						boost::dynamic_pointer_cast<dwarf::spec::with_named_children_die>(this->source_data_type)
							->named_child(i_mapping->second.target.at(0))
						)->get_type(),
					*boost::dynamic_pointer_cast<dwarf::spec::member_die>(
						boost::dynamic_pointer_cast<dwarf::spec::with_named_children_die>(this->sink_data_type)
							->named_child(i_mapping->second.target.at(0))
						)->get_type()
					);

			assert(pair.first && pair.second);
			working.insert(pair);
		}
		for (auto i_mapping = explicit_toplevel_mappings.begin();
				i_mapping != explicit_toplevel_mappings.end();
				i_mapping++)
		{
			// FIXME: to accommodate foo.bar <--> foo.bar,
			// we need to capture that the dependency *must* adhere to the 
			// body of rules included by the Cake programmer
			auto pair = std::make_pair(
					*boost::dynamic_pointer_cast<dwarf::spec::member_die>(
						boost::dynamic_pointer_cast<dwarf::spec::with_named_children_die>(
							this->source_data_type
						)->named_child(i_mapping->first))->get_type(),
					*boost::dynamic_pointer_cast<dwarf::spec::member_die>(
						boost::dynamic_pointer_cast<dwarf::spec::with_named_children_die>(
							this->sink_data_type
						)->named_child(i_mapping->second->target.at(0)))->get_type()
					);

			assert(pair.first && pair.second);
			auto retval = working.insert(pair);
			assert(retval.second); // assert that we actually inserted something
			// FIX: use multimap::equal_range to generate a list of all rules
			// that have the same string, then group these as constraints 
			// and add them to the dependency
		}
		std::cerr << "DEPENDENCIES of conversion " 
			<< " from " << *this->source_data_type
			<< " to " << *this->sink_data_type
			<< ": total " << working.size() << std::endl;
		std::cerr << "Listing:" << std::endl;
		for (auto i_dep = working.begin(); i_dep != working.end(); i_dep++)
		{
			std::cerr << "require from " << *i_dep->first << " to " << *i_dep->second << std::endl;
		}		
		return std::vector< dep >(working.begin(), working.end());
	}
	
	void structural_value_conversion::emit_body()
	{
		/* Create or find buffer */
		m_out << w.get_type_name(sink_data_type) << " __cake_tmp, *__cake_p_buf;" << std::endl
			<< "if (__cake_p_to) __cake_p_buf = __cake_p_to; else __cake_p_buf = &__cake_tmp;" << std::endl;
		/* HACK: create a non-const "from" reference, because we want to index
		 * our templates by CV-qualifier-free typenames. */
		m_out << "auto __cake_nonconst_from = "
			"const_cast< "
				"boost::remove_const<"
					"boost::remove_reference< "
						"__typeof(__cake_from)"
					" >::type "
				" >::type &"
			" >(__cake_from);" << std::endl;

		/* Emit toplevel field mappings right now */
		for (auto i_name_matched = name_matched_mappings.begin();
				i_name_matched != name_matched_mappings.end();
				i_name_matched++)
		{
			/* These are module--name pairs for the source and sink modules. */
			auto pre_context_pair = std::make_pair(i_name_matched->second.pre_context, 
				w.m_d.name_of_module(i_name_matched->second.pre_context));
			auto post_context_pair = std::make_pair(i_name_matched->second.post_context, 
				w.m_d.name_of_module(i_name_matched->second.post_context));
			
			/* All dependencies should be in place now -- check this. */
			i_name_matched->second.check_sanity();

			/* Build an environment for the code fragment handling this field. */
// #define REMOVE_CONST(s) "boost::remove_const<__typeof(" + (s) + ")>::type"
#define REMOVE_CONST(s) (s)
			wrapper_file::environment env;
			// insert the whole source object, bound by magic "__cake_from" ident
			// -- NOTE: must remove this before crossover, or we'll get an infinite loop!
			env.insert(std::make_pair(std::string("__cake_from"), // cake name -- we will want "this" too, eventually
				(wrapper_file::bound_var_info) {
					"from", // cxx name
					/* source_data_type, */ REMOVE_CONST(std::string("__cake_nonconst_from")), // typeof
					source_module
					}));
			
			/* Since we need not have a unique source field (might e.g. be an expression
			 * involving multiple source fields, build a Cake environment containing
			 * all source fields. */
			for (auto i_field = boost::dynamic_pointer_cast<dwarf::spec::with_data_members_die>(
						source_data_type)->member_children_begin();
					i_field != boost::dynamic_pointer_cast<dwarf::spec::with_data_members_die>(
						source_data_type)->member_children_end();
					i_field++)
			{
				assert((*i_field)->get_name());

				std::cerr << "adding name " << *(*i_field)->get_name() << std::endl;
				env.insert(std::make_pair(*(*i_field)->get_name(),
					(wrapper_file::bound_var_info) {
						std::string("__cake_nonconst_from.") + *(*i_field)->get_name(), // cxx name
						REMOVE_CONST(std::string("__cake_nonconst_from.") + *(*i_field)->get_name()), // typeof
						source_module
					}));
			}
			
			std::string target_field_selector;
			std::ostringstream s;
			for (auto i_name_part = i_name_matched->second.target.begin();
				i_name_part != i_name_matched->second.target.end();
				i_name_part++)
			{
				if (i_name_part != i_name_matched->second.target.begin())
				{ s << "."; }
				s << *i_name_part;
			}
			target_field_selector = s.str();
			/* If we *do* have a unique source field, then a bunch of other Cake idents
			 * are defined: "this", "here" and "there" (but not "that" -- I think). */
			boost::optional<std::string> unique_source_field_selector;
			if (i_name_matched->second.unique_source_field)
			{
				std::ostringstream s;
				for (auto i_name_part = i_name_matched->second.unique_source_field->begin();
					i_name_part != i_name_matched->second.unique_source_field->end();
					i_name_part++)
				{
					if (i_name_part != i_name_matched->second.unique_source_field->begin())
					{ s << "."; }
					s << *i_name_part;
				}
				unique_source_field_selector = s.str();
				// If we have a source-side infix stub, we must emit that.
				// It may use "this", "here" and "there" (but not "that").
				// So first we insert magic "__cake_this", "__cake_here" and "__cake_there"
				// entries to the Cake environment for this.
				env.insert(std::make_pair("__cake_this",
					(wrapper_file::bound_var_info) {
						"__cake_nonconst_from." + *unique_source_field_selector, // cxx name
						REMOVE_CONST("__cake_nonconst_from." + *unique_source_field_selector), // typeof
						source_module
					}));
				env.insert(std::make_pair("__cake_here",
					(wrapper_file::bound_var_info) {
						"&__cake_from." + *unique_source_field_selector, // cxx name
						REMOVE_CONST("&__cake_nonconst_from." + *unique_source_field_selector), // typeof
						source_module
					}));
			}
			// we always have a "there"
			env.insert(std::make_pair("__cake_there",
				(wrapper_file::bound_var_info) {
					"&__cake_p_buf->" + target_field_selector, // cxx name
					"&__cake_p_buf->" + target_field_selector, // typeof
					source_module
				}));
			
			// environment complete for now; create a context out of this environment
			wrapper_file::context ctxt(w, source_module, target_module, env);

			/* First we evaluate the source expression. */
			auto status1 = w.emit_stub_expression_as_statement_list(ctxt,
				i_name_matched->second.stub/*,
				sink_data_type*/);
				
			auto new_env1 = w.merge_environment(ctxt.env, status1.new_bindings);
			if (status1.result_fragment != w.NO_VALUE) new_env1["__cake_it"] = 
				(wrapper_file::bound_var_info) {
					status1.result_fragment,
					status1.result_fragment,  //shared_ptr<type_die>(),
					ctxt.modules.source };
			
			ctxt.env = new_env1;
			/* Now we can evaluate the pre-stub if there is one. It should use "this" if
			 * it depends on the field value. */
			auto new_env2 = new_env1; // provisional value
			if (i_name_matched->second.pre_stub)
			{
				auto status2 = w.emit_stub_expression_as_statement_list(ctxt,
					i_name_matched->second.pre_stub);
				new_env2 = w.merge_environment(ctxt.env, status2.new_bindings);
					if (status2.result_fragment != w.NO_VALUE) new_env2["__cake_it"] = 
						(wrapper_file::bound_var_info) {
							status2.result_fragment,
							status2.result_fragment,  //shared_ptr<type_die>(),
							ctxt.modules.source 
						};
			}
			
			/* Now we need to "cross over" the environment. This crossover is different
			 * from the one in the wrapper code. We don't need to allocate or sync
			 * co-objects: the allocation has already been done, and we're in the middle
			 * of a sync process right now, most likely. Nevertheless, crossover_environment
			 * should work. But first we erase "this", "here" and "there" from the environment
			 * if we added them -- these are side-specific. */
			// crossover point
			ctxt.modules.current = target_module;
			if (i_name_matched->second.unique_source_field)
			{
				new_env2.erase("__cake_this");
				new_env2.erase("__cake_here");
			}
			new_env2.erase("__cake_there");
			new_env2.erase("__cake_from");
			m_out << "// source->sink crossover point" << std::endl;
			ctxt.env = w.crossover_environment(source_module, new_env2, target_module, 
				/* no constraints */ 
				std::multimap< std::string, boost::shared_ptr<dwarf::spec::type_die> >());
			
			/* Now we add "that", "there" and "here" (but not "this") to the environment. */
			if (i_name_matched->second.unique_source_field)
			{
				env.insert(std::make_pair("__cake_that",
					(wrapper_file::bound_var_info) {
						"__cake_nonconst_from." + *unique_source_field_selector, // cxx name
						REMOVE_CONST("__cake_nonconst_from." + *unique_source_field_selector), // typeof
						target_module
					}));

				env.insert(std::make_pair("__cake_there",
					(wrapper_file::bound_var_info) {
						"&__cake_nonconst_from." + *unique_source_field_selector, // cxx name
						REMOVE_CONST("&__cake_nonconst_from." + *unique_source_field_selector), // typeof
						target_module
					}));
			} // we can always add "here"
			env.insert(std::make_pair("__cake_here",
				(wrapper_file::bound_var_info) {
					"&__cake_p_buf->" + target_field_selector, // cxx name
					"&__cake_p_buf->" + target_field_selector, // typeof
					target_module
				}));	
#undef REMOVE_CONST
			/* Now emit the post-stub. */
			auto new_env3 = ctxt.env; // provisional value
			if (i_name_matched->second.post_stub)
			{
				auto status3 = w.emit_stub_expression_as_statement_list(ctxt,
					i_name_matched->second.post_stub);
				new_env3 = w.merge_environment(ctxt.env, status3.new_bindings);
					if (status3.result_fragment != w.NO_VALUE) new_env3["__cake_it"] = 
						(wrapper_file::bound_var_info) {
							status3.result_fragment,
							status3.result_fragment,  //shared_ptr<type_die>(),
							ctxt.modules.sink 
						};
			}
			
			/* Now we have an "it": either the output of the stub, if there was one,
			 * or else the converted field value. */
			
// 			auto result = w.emit_stub_expression_as_statement_list(ctxt,
// 				i_name_matched->second.stub/*,
// 				sink_data_type*/);
// 				//, modules, pre_context_pair, post_context_pair,
// 				//boost::shared_ptr<dwarf::spec::type_die>(), env);
// 			m_out << "assert(" << result.success_fragment << ");" << std::endl;
// 			m_out << "__cake_p_buf->" << target_field_selector
// 				<< " = ";
// 			m_out << w.component_pair_classname(modules);
// 			m_out << "::value_convert_from_"
// 				<< ((modules.first == source_module) ? "first_to_second" : "second_to_first")
// 				<< "<" << "__typeof(__cake_p_buf->" << target_field_selector << ")"
// 				<< ">(__cake_from." << target_field_selector << ");" << std::endl;
			
			m_out << "__cake_p_buf->" << target_field_selector << " = " <<
				env["__cake_it"].cxx_name << ";" << std::endl;
				
		} // end for i_name_matched

		/* Recurse on others */
		// FIXME -- I think we don't actually recurse, we just use
		// dependency handling to ensure that other mappings described
		// in the lower-level rules are added. HMM. Somewhere we 
		// have to make sure that the source fragments describing these
		// lower-level rules are actually discoverable from the dependency record.

		// output return statement
		m_out << "return *__cake_p_buf;" << std::endl;
	}

}
