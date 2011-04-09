#include "util.hpp"
#include "parser.hpp"
#include "module.hpp"
#include "link.hpp"
#include "valconv.hpp"
#include "wrapsrc.hpp"
#include <sstream>
#include <cmath>

namespace cake
{
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
		from_typename(w.get_type_name(source_concrete_type)),
		to_typename(w.get_type_name(sink_concrete_type))
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
		bool emit_template_prefix /* = true */
		)
	{
		m_out << (emit_template_prefix ? "template <>\n" : "" )
            << (emit_struct_keyword ? "struct " : "")
			<< (return_typename ? *return_typename + "\n" : "")
			<< "value_convert<"
            << from_typename // From
            << ", "
            << to_typename // To
            << ", "
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
								(member_mapping_rule)
								{
									/* definite_member_name target; */ 
										read_definite_member_name(source_is_on_left ? rightValuePattern : leftValuePattern),
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
							(member_mapping_rule)
							{
								/* definite_member_name target; */ 
									read_definite_member_name(rightValuePattern),
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
							(member_mapping_rule)
							{
								/* definite_member_name target; */ 
									read_definite_member_name(leftValuePattern),
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
						(member_mapping_rule){
							/* definite_member_name target; */ 
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

		/* Emit toplevel mappings right now */
		for (auto i_name_matched = name_matched_mappings.begin();
				i_name_matched != name_matched_mappings.end();
				i_name_matched++)
		{
			auto pre_context_pair = std::make_pair(i_name_matched->second.pre_context, 
				w.m_d.name_of_module(i_name_matched->second.pre_context));
			auto post_context_pair = std::make_pair(i_name_matched->second.post_context, 
				w.m_d.name_of_module(i_name_matched->second.post_context));

			std::string from_ident;
			wrapper_file::environment env;
			env.insert(std::make_pair(std::string("__cake_from"), 
				(wrapper_file::bound_var_info) {
					"from",
					source_data_type,
					source_module
					}));
			wrapper_file::context ctxt(w, source_module, target_module, env);

			auto result = w.emit_stub_expression_as_statement_list(ctxt,
				i_name_matched->second.stub,
				sink_data_type);
				//, modules, pre_context_pair, post_context_pair,
				//boost::shared_ptr<dwarf::spec::type_die>(), env);
			m_out << "assert(" << result.success_fragment << ");" << std::endl;
			m_out << "__cake_p_buf->" << i_name_matched->first
				<< " = ";
			m_out << w.component_pair_classname(modules);
			m_out << "::value_convert_from_"
				<< ((modules.first == source_module) ? "first_to_second" : "second_to_first")
				<< "<" << "__typeof(__cake_p_buf->" << i_name_matched->first << ")"
				<< ">(__cake_from." << i_name_matched->first << ");" << std::endl;
		}

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
