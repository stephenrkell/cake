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
using dwarf::spec::with_data_members_die;
using dwarf::spec::structure_type_die;
using std::make_pair;
using std::string;
using std::pair;
using std::endl;
using std::cerr;
using std::vector;
using std::ostringstream;
using boost::optional;

namespace cake
{
	string value_conversion::source_fq_namespace() const
	{ return w.ns_prefix + "::" + w.m_d.name_of_module(source); }
	string value_conversion::sink_fq_namespace() const
	{ return w.ns_prefix + "::" + w.m_d.name_of_module(sink); }
	
	shared_ptr<value_conversion> create_value_conversion(module_ptr source,
            shared_ptr<dwarf::spec::type_die> source_data_type,
            antlr::tree::Tree *source_infix_stub,
            module_ptr sink,
            shared_ptr<dwarf::spec::type_die> sink_data_type,
            antlr::tree::Tree *sink_infix_stub,
            antlr::tree::Tree *refinement,
			bool source_is_on_left,
			antlr::tree::Tree *corresp)
	{
		assert(false);
	}
	std::ostream& operator<<(std::ostream& s, const basic_value_conversion& c)
	{
		s << "{ " << c.source->get_filename() << " :: " 
			<< (c.source_data_type->get_name() ? *c.source_data_type->get_name() : "<anonymous>")
			<< " to "
			<< c.sink->get_filename() << " :: " 
			<< (c.sink_data_type->get_name() ? *c.sink_data_type->get_name() : "<anonymous>")
			<< ", " << (c.source_is_on_left ? "left to right" : "right to left")
			<< ", " << (c.init_only ? "initialization" : "update")
			<< " }";
		return s;
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
        emit_header(optional<string>(to_typename), false, false); // no struct keyword, no template<>
        m_out << "::" << endl;
        emit_signature(false, false); // NO return type, NO default argument
        m_out << endl << "{" << endl;
        m_out.inc_level();
	}
	
	void value_conversion::emit_header(optional<string> return_typename, 
		bool emit_struct_keyword/* = true*/,
		bool emit_template_prefix /* = true */,
		bool emit_return_typename /* = true */
		)
	{
		int rule_tag = w.m_d.val_corresp_numbering[shared_from_this()];
// 		cerr << "Retrieved number " << rule_tag 
// 			<< " for rule relating source data type " 
// 			<< *this->source_data_type
// 			<< " with sink data type "
// 			<< *this->sink_data_type
// 			<< endl;
		
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
            << rule_tag // RuleTag
            << ">" << endl;
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
		m_out << "{" << endl;
		m_out.inc_level();
		emit_signature();
		m_out << ";";
        m_out.dec_level();
        m_out << "};" << endl;
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
            << w.m_d.val_corresp_numbering[shared_from_this()] // RuleTag
            << ">" << endl;

	}
	
	// HACK to output the type of a pointer to this conversion function
	void value_conversion::emit_cxx_function_ptr_type(
		optional<const string& > decl_name)
	{
		m_out << "void *(*" << (decl_name ? *decl_name : "")
			<< ")(" << from_typename << "* ,"
				<< to_typename << " *)" << endl;
	}
	
	
	vector< pair < shared_ptr<type_die>, shared_ptr<type_die> > > 
	value_conversion::get_dependencies()
	{
		return vector< pair < shared_ptr<type_die>, shared_ptr<type_die> > >();
	}
	
	void reinterpret_value_conversion::emit_body()
	{
		m_out << "if (__cake_p_to) *__cake_p_to = *reinterpret_cast<const " 
			<< w.get_type_name(sink_data_type) << "*>(&__cake_from);" << endl
			<< "return *reinterpret_cast<const " 
			<< w.get_type_name(sink_data_type) << "*>(&__cake_from);" << endl;
	}
			
	struct member_has_name
	 : public std::unary_function<shared_ptr<member_die> , bool>
	{
		string m_name;
		member_has_name(const string& name) : m_name(name) {}
		bool operator()(shared_ptr<member_die> p_member) const
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
		
		auto found_target_field_type = found_target_field->get_type();
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
			
			found_source_field_type = found_source_field->get_type();
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
						/* cerr << "Testing corresp with source "
							<< *(arg.second->source_data_type)
							<< " and sink " 
							<< *(arg.second->sink_data_type)
							<< endl; */
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
			cerr << "Exception refers to target field " << *found_target_field
				<< " and ";
			if (unique_source_field) cerr << " and source field " << *found_source_field;
			else cerr << " with no unique source";
			cerr << endl;
			
			RAISE(owner->corresp, "corresponding fields lack corresponding type");
		}
	}

	primitive_value_conversion::primitive_value_conversion(wrapper_file& w,
			srk31::indenting_ostream& out, 
			const basic_value_conversion& basic,
			bool init_only,
			bool& init_is_identical)
		:   value_conversion(w, out, basic), 
			source_module(w.module_of_die(source_data_type)),
			target_module(w.module_of_die(sink_data_type)),
			modules(link_derivation::sorted(make_pair(source_module, target_module)))
		
	{
		// HACK: why do we have an init_only arg?
		assert(init_only == basic.init_only);
		
		// ... unless we find out otherwise
		init_is_identical = true; 
	
	}
	
	structural_value_conversion::structural_value_conversion(wrapper_file& w,
			srk31::indenting_ostream& out, 
			const basic_value_conversion& basic,
			bool init_only,
			bool& init_is_identical)
		:   value_conversion(w, out, basic), 
		    primitive_value_conversion(w, out, basic, init_only, init_is_identical)
	{
		/* Find explicitly assigned-to fields:
		 * the map is from the assigned-to- field
		 * to the rule details.
		 * NOTE that this should non-toplevel field corresps (FIXME).  */
		//std::map<definite_member_name, member_mapping_rule> field_corresps;
		
		// don't do this any more -- gets set by primitive_value_conversion, and we just &=
		//init_is_identical = true; // ... unless we find otherwise
		
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
						explicit_field_corresps.insert(make_pair(
							read_definite_member_name(
								source_is_on_left ? rightValuePattern : leftValuePattern),
								(member_mapping_rule_base)
								{
									/* structural_value_conversion *owner */
										this,
									/* definite_member_name target; */ 
										read_definite_member_name(source_is_on_left ? rightValuePattern : leftValuePattern),
									/* optional<definite_member_name> unique_source_field; */
										GET_TYPE(source_is_on_left ? leftInfixStub : rightInfixStub) == CAKE_TOKEN(DEFINITE_MEMBER_NAME) ?
											read_definite_member_name(source_is_on_left ? leftInfixStub : rightInfixStub)
											: optional<definite_member_name>(),
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
						if (!source_is_on_left) continue; // this field rule is not relevant
						INIT;
						BIND2(refinementRule, leftStubExpr);
						BIND2(refinementRule, leftInfixStub);
						BIND2(refinementRule, rightInfixStub);
						BIND2(refinementRule, rightValuePattern);
						explicit_field_corresps.insert(make_pair(
							read_definite_member_name(rightValuePattern),
							(member_mapping_rule_base)
							{
								/* structural_value_conversion *owner, */
									this,
								/* definite_member_name target; */ 
									read_definite_member_name(rightValuePattern),
								/* optional<definite_member_name> unique_source_field; */
									GET_TYPE(leftStubExpr) == CAKE_TOKEN(DEFINITE_MEMBER_NAME) ?
										read_definite_member_name(leftStubExpr)
										: optional<definite_member_name>(),
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
						if (source_is_on_left) continue; // this field rule is not relevant
						INIT;
						BIND2(refinementRule, leftValuePattern);
						BIND2(refinementRule, leftInfixStub);
						BIND2(refinementRule, rightInfixStub);
						BIND2(refinementRule, rightStubExpr);
						explicit_field_corresps.insert(make_pair(
							read_definite_member_name(leftValuePattern),
							(member_mapping_rule_base)
							{
								/* structural_value_conversion *owner, */
									this,
								/* definite_member_name target; */ 
									read_definite_member_name(leftValuePattern),
								/* optional<definite_member_name> unique_source_field; */
									GET_TYPE(rightStubExpr) == CAKE_TOKEN(DEFINITE_MEMBER_NAME) ?
										read_definite_member_name(rightStubExpr)
										: optional<definite_member_name>(),
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
						if (!source_is_on_left) continue; // not relevant in this direction
						init_is_identical = false;
						if (!init_only) continue; // not relevant unless we're generating a distinct init rule
						INIT;
						BIND2(refinementRule, leftStubExpr);
						BIND2(refinementRule, leftInfixStub);
						BIND2(refinementRule, rightInfixStub);
						BIND2(refinementRule, rightValuePattern);
						explicit_field_corresps.insert(make_pair(
							read_definite_member_name(rightValuePattern),
							(member_mapping_rule_base)
							{
								/* structural_value_conversion *owner, */
									this,
								/* definite_member_name target; */ 
									read_definite_member_name(rightValuePattern),
								/* optional<definite_member_name> unique_source_field; */
									GET_TYPE(leftStubExpr) == CAKE_TOKEN(DEFINITE_MEMBER_NAME) ?
										read_definite_member_name(leftStubExpr)
										: optional<definite_member_name>(),
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
					case (CAKE_TOKEN(RL_DOUBLE_ARROW_Q)):
					{
						if (source_is_on_left) continue; // not relevant in this direction
						init_is_identical = false;
						if (!init_only) continue; // not relevant unless we're generating a distinct init rule
						INIT;
						BIND2(refinementRule, leftValuePattern);
						BIND2(refinementRule, leftInfixStub);
						BIND2(refinementRule, rightInfixStub);
						BIND2(refinementRule, rightStubExpr);
						explicit_field_corresps.insert(make_pair(
							read_definite_member_name(leftValuePattern),
							(member_mapping_rule_base)
							{
								/* structural_value_conversion *owner, */
									this,
								/* definite_member_name target; */ 
									read_definite_member_name(leftValuePattern),
								/* optional<definite_member_name> unique_source_field; */
									GET_TYPE(rightStubExpr) == CAKE_TOKEN(DEFINITE_MEMBER_NAME) ?
										read_definite_member_name(rightStubExpr)
										: optional<definite_member_name>(),
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
			++i_mapping)
		{
			if (i_mapping->first.size() == 1)
			{
				explicit_toplevel_mappings.insert(
					make_pair(
						i_mapping->first.at(0), &i_mapping->second));
			}
			else
			{
				cerr << "WARNING: found a non-toplevel mapping (FIXME)" << endl;
			}
		}
		/* The NON-toplevel mappings are useful for our dependencies only.
		 * We should collect them togther into FIXME the dependencies description somehow. */

		/* Name-match toplevel subtrees not mentioned so far. */
		for (auto i_member
			 = dynamic_pointer_cast<structure_type_die>(sink_data_type)
			 	->member_children_begin();
			i_member != dynamic_pointer_cast<structure_type_die>(sink_data_type)
			 	->member_children_end();
			++i_member)
		{
			assert((*i_member)->get_name());
			if (name_matched_mappings.find(*(*i_member)->get_name())
				== name_matched_mappings.end())
			{
				/* We have a name-matching candidate -- look for like-named
				 * field in opposing structure. */

				auto source_as_struct = dynamic_pointer_cast<structure_type_die>(
					source_data_type);

				auto found = std::find_if(source_as_struct->member_children_begin(),
					source_as_struct->member_children_end(),
					member_has_name(*(*i_member)->get_name()));
				if (found != source_as_struct->member_children_end())
				{
					cerr << "Matched a name " << *(*i_member)->get_name()
						<< " in DIEs " << *source_as_struct
						<< " and " << *sink_data_type
						<< endl;
					name_matched_mappings.insert(make_pair(
						*(*i_member)->get_name(),
						(member_mapping_rule_base){
							/* structural_value_conversion *owner, */
								this,
							/* definite_member_name target; */ 
								definite_member_name(1, *(*i_member)->get_name()),
							/* optional<definite_member_name> unique_source_field; */
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
					cerr << "Didn't match name " << *(*i_member)->get_name()
						<< " from DIE " << *sink_data_type
						<< " in DIE " << *source_as_struct
						<< endl;
				}
			}
		}
	}

	vector< value_conversion::dep > structural_value_conversion::get_dependencies()
	{
		std::set<dep> working;
	
		// for each field (subobject) that corresponds, we require
		// a defined conversion
		for (auto i_mapping = name_matched_mappings.begin();
				i_mapping != name_matched_mappings.end();
				++i_mapping)
		{
			auto pair = make_pair(
					dynamic_pointer_cast<member_die>(
						dynamic_pointer_cast<with_named_children_die>(this->source_data_type)
							->named_child(i_mapping->second.target.at(0))
						)->get_type(),
					dynamic_pointer_cast<member_die>(
						dynamic_pointer_cast<with_named_children_die>(this->sink_data_type)
							->named_child(i_mapping->second.target.at(0))
						)->get_type()
					);

			assert(pair.first && pair.second);
			working.insert(pair);
		}
		for (auto i_mapping = explicit_toplevel_mappings.begin();
				i_mapping != explicit_toplevel_mappings.end();
				++i_mapping)
		{
			// FIXME: to accommodate foo.bar <--> foo.bar,
			// we need to capture that the dependency *must* adhere to the 
			// body of rules included by the Cake programmer
			
			// FIXME: what if we don't have a source member?!
			auto source_member = dynamic_pointer_cast<member_die>(
						dynamic_pointer_cast<with_named_children_die>(
							this->source_data_type
						)->named_child(i_mapping->first));
			if (!source_member) continue; // HACK: say "no dependencies", for now...
			// ... in practice, this will be caught by the C++ compiler
			
			auto sink_member = dynamic_pointer_cast<member_die>(
						dynamic_pointer_cast<with_named_children_die>(
							this->sink_data_type
						)->named_child(i_mapping->second->target.at(0)));
			auto source_member_type = source_member->get_type();
			auto sink_member_type = sink_member->get_type();
			
			auto pair = make_pair(source_member_type, sink_member_type);

			assert(pair.first && pair.second);
			auto retval = working.insert(pair);
			assert(retval.second); // assert that we actually inserted something
			// FIX: use multimap::equal_range to generate a list of all rules
			// that have the same string, then group these as constraints 
			// and add them to the dependency
		}
		//cerr << "DEPENDENCIES of conversion " 
		//	<< " from " << *this->source_data_type
		//	<< " to " << *this->sink_data_type
		//	<< ": total " << working.size() << endl;
		//cerr << "Listing:" << endl;
		//for (auto i_dep = working.begin(); i_dep != working.end(); ++i_dep)
		//{
		//	cerr << "require from " << *i_dep->first << " to " << *i_dep->second << endl;
		//}		
		return vector< dep >(working.begin(), working.end());
	}
	
	void primitive_value_conversion::emit_buffer_declaration()
	{
		/* Create or find buffer */
		m_out << w.get_type_name(sink_data_type) << " __cake_tmp, *__cake_p_buf;" << endl
			<< "if (__cake_p_to) __cake_p_buf = __cake_p_to; else __cake_p_buf = &__cake_tmp;" << endl;
		/* HACK: create a non-const "from" reference, because we want to index
		 * our templates by CV-qualifier-free typenames. */
		m_out << "auto __cake_nonconst_from = "
			"const_cast< "
				"boost::remove_const<"
					"boost::remove_reference< "
						"__typeof(__cake_from)"
					" >::type "
				" >::type &"
			" >(__cake_from);" << endl;
	
	}
	
	void primitive_value_conversion::emit_body()
	{
		emit_buffer_declaration();
		
		wrapper_file::environment basic_env; // empty!
		wrapper_file::context ctxt(
			this->w,
			source,
			sink,
			basic_env
		);
		// HMM. Should we add "__cake_from" to the environment
		ctxt.opt_val_corresp = (wrapper_file::context::val_info_s){ this->corresp };
		
		write_single_field(ctxt,
			"",  // no target selector needed
			optional<string>(""),  // no source selector needed
			make_ast("this", &cakeCParser::stubPrimitiveExpression), // our expression is just "this"
			GET_CHILD_COUNT(source_infix_stub) ? GET_CHILD(source_infix_stub, 0) : 0, // pass over INFIX_STUB_EXPR
			GET_CHILD_COUNT(sink_infix_stub) ? GET_CHILD(sink_infix_stub, 0) : 0);
			
		// output return statement
		m_out << "return *__cake_p_buf;" << endl;
	}
	
	void 
	primitive_value_conversion::write_single_field(
		wrapper_file::context& ref_ctxt,
		string target_field_selector,
		optional<string> unique_source_field_selector,
		antlr::tree::Tree *source_expr,
		antlr::tree::Tree *source_infix,
		antlr::tree::Tree *sink_infix)
	{
		// Assume our environment already contains all the source fields that the
		// rule could reference. 
		
		// copy the context: we actually don't want to propagate any changes we make
		wrapper_file::context ctxt = ref_ctxt;
		wrapper_file::environment basic_env = ctxt.env; 
		
		wrapper_file::environment extra_env;
		// Add the magic keyword fields
		if (unique_source_field_selector)
		{
			// If we have a source-side infix stub, we must emit that.
			// It may use "this", "here" and "there" (but not "that").
			// So first we insert magic "__cake_this", "__cake_here" and "__cake_there"
			// entries to the Cake environment for this.
			extra_env.insert(make_pair("__cake_this",
				(wrapper_file::bound_var_info) {
					"__cake_nonconst_from" + *unique_source_field_selector, // cxx name
					"__cake_nonconst_from" + *unique_source_field_selector, // typeof
					source_module,
					false,
					"__cake_default",
					"__cake_default"
				}));
			extra_env.insert(make_pair("__cake_here",
				(wrapper_file::bound_var_info) {
					"&__cake_from" + *unique_source_field_selector, // cxx name
					"&__cake_nonconst_from" + *unique_source_field_selector, // typeof
					source_module,
					false,
					"__cake_default"
					"__cake_default"
				}));
		}
		// we always have a "there"
		extra_env.insert(make_pair("__cake_there",
			(wrapper_file::bound_var_info) {
				"&((*__cake_p_buf)" + target_field_selector + ")", // cxx name
				"&((*__cake_p_buf)" + target_field_selector + ")", // typeof
				source_module,
				false,
				"__cake_default",
				"__cake_default"
			}));

		// compute the merged environment
		auto extended_env1 = w.merge_environment(ctxt.env, extra_env);
		ctxt.env = extended_env1;

		/* Evaluate the source expression in this environment. */
		auto status1 = w.emit_stub_expression_as_statement_list(ctxt,
			source_expr/*,
			sink_data_type*/);

		/* Keep things simple: assert there are no new bindings. 
		 * If there are, we'll have to stash the bindings with a different name, then 
		 * re-establish them in the post-stub. */
		assert(status1.new_bindings.size() == 0);
		// auto new_env1 = w.merge_environment(ctxt.env, status1.new_bindings);
		/* Bind the result under a special name (not __cake_it). 
		 * The source expression should always yield a value. */
		assert(status1.result_fragment != w.NO_VALUE && status1.result_fragment != "");
		// we extend the *basic* (shared) environment
		extended_env1["__cake_it"] = 
			(wrapper_file::bound_var_info) {
				status1.result_fragment,
				status1.result_fragment,  //shared_ptr<type_die>(),
				ctxt.modules.source,
				false,
				"__cake_default",
				"__cake_default"
			};

		/* Now we can evaluate the pre-stub if there is one. It should use "this" if
		 * it depends on the field value. */
		auto extended_env2 = extended_env1; // provisional value
		if (source_infix && GET_CHILD_COUNT(source_infix) > 0)
		{
			auto status2 = w.emit_stub_expression_as_statement_list(ctxt,
				source_infix);
			extended_env2 = w.merge_environment(extended_env1, status2.new_bindings);
				if (status2.result_fragment != w.NO_VALUE) extended_env2["__cake_it"] = 
					(wrapper_file::bound_var_info) {
						status2.result_fragment,
						status2.result_fragment,  //shared_ptr<type_die>(),
						ctxt.modules.source,
						false,
						"__cake_default",
						"__cake_default"
					};
		}
		// stash the result in the basic env
		assert(extended_env2.find("__cake_it") != extended_env2.end());
		basic_env[/*"__cake_source_" + target_field_selector.substr(1)*/ "__cake_it"]
		 = extended_env2["__cake_it"];

		// revert to the basic env, with this "it" extension
		ctxt.env = basic_env;
		//ctxt.env.erase("__cake_from");

		/* 3. Crossover the environment. This has the effect of applying depended-on correspondences. */
		ctxt.modules.current = target_module;
		assert(ctxt.env.find("__cake_this") == ctxt.env.end());
		assert(ctxt.env.find("__cake_here") == ctxt.env.end());
		assert(ctxt.env.find("__cake_there") == ctxt.env.end());
		/* Now we need to "cross over" the environment. This crossover is different
		 * from the one in the wrapper code. We don't need to allocate or sync
		 * co-objects: the allocation has already been done, and we're in the middle
		 * of a sync process right now, most likely. Nevertheless, crossover_environment
		 * should work. But first we erase "this", "here" and "there" from the environment
		 * if we added them -- these are side-specific. */
		// crossover point
		ctxt.modules.current = target_module;
		m_out << "// source->sink crossover point" << endl;
		auto crossed_env = w.crossover_environment(source_module, basic_env, target_module, 
			/* no constraints */ 
			std::multimap< string, shared_ptr<type_die> >());

		// always start with crossed-over environment
		ctxt.env = crossed_env;

		/* Now we add "that", "there" and "here" (but not "this") to the environment. */
		if (unique_source_field_selector)
		{
			ctxt.env.insert(make_pair("__cake_that",
				(wrapper_file::bound_var_info) {
					"__cake_nonconst_from" + *unique_source_field_selector, // cxx name
					"__cake_nonconst_from" + *unique_source_field_selector, // typeof
					target_module,
					false,
					"__cake_default",
					"__cake_default"
				}));

			ctxt.env.insert(make_pair("__cake_there",
				(wrapper_file::bound_var_info) {
					"&__cake_nonconst_from" + *unique_source_field_selector, // cxx name
					"&__cake_nonconst_from" + *unique_source_field_selector, // typeof
					target_module,
					false,
					"__cake_default",
					"__cake_default"
				}));
		} 

		// we can always add "here"
		ctxt.env.insert(make_pair("__cake_here",
			(wrapper_file::bound_var_info) {
				"&((*__cake_p_buf)" + target_field_selector + ")", // cxx name
				"&((*__cake_p_buf)" + target_field_selector + ")", // typeof
				target_module,
				false,
				"__cake_default",
				"__cake_default"
			}));
		// we can also always add "it"!
		ctxt.env.insert(make_pair("__cake_it", ctxt.env[
			/*"__cake_source_" + target_field_selector.substr(1)*/ "__cake_it"]));

		/* Now emit the post-stub. */
		auto new_env3 = ctxt.env; // provisional value
		if (sink_infix && GET_CHILD_COUNT(sink_infix) > 0)
		{
			auto status3 = w.emit_stub_expression_as_statement_list(ctxt,
				sink_infix);
			new_env3 = w.merge_environment(ctxt.env, status3.new_bindings);
				if (status3.result_fragment != w.NO_VALUE) new_env3["__cake_it"] = 
					(wrapper_file::bound_var_info) {
						status3.result_fragment,
						status3.result_fragment,  //shared_ptr<type_die>(),
						ctxt.modules.sink,
						false,
						"__cake_default",
						"__cake_default"
					};
		}

		/* Now we have an "it": either the output of the stub, if there was one,
		 * or else the converted field value. */

		assert(ctxt.env["__cake_it"].cxx_name != "");
		m_out << "(*__cake_p_buf)" << target_field_selector << " = " <<
			ctxt.env["__cake_it"].cxx_name << ";" << endl;
	}
	
	void structural_value_conversion::emit_body()
	{
		emit_buffer_declaration();
		/* Here's how we emit these.
		 * 
		 * 0. Build the list of sink-side (target) fields we are going to write.
		 * 1. Build an environment containing all the source-side fields that these depend on.
		 * 2. For each target field, emit its pre-stub (if any) and extend the environment with the result.
		 * 3. Crossover the environment. This has the effect of applying depended-on correspondences.
		 * 4. For each target field, emit its post-stub (if any) and extend the environment with the result.
		 * 5. For each target field, assign its value.
		 */
		
		/* 0. Build the list of sink-side (target) fields we are going to write. */
		map<string, member_mapping_rule *> target_fields_to_write;
		for (auto i_name_matched = name_matched_mappings.begin();
				i_name_matched != name_matched_mappings.end();
				++i_name_matched)
		{
			string target_field_selector;
			ostringstream s;
			for (auto i_name_part = i_name_matched->second.target.begin();
				i_name_part != i_name_matched->second.target.end();
				++i_name_part)
			{
				//if (i_name_part != i_name_matched->second.target.begin())
				{ s << "."; } // always begin selector with '.'!
				s << *i_name_part;
			}
			target_field_selector = s.str();
			
			assert(
				target_fields_to_write.find(target_field_selector)
				 == target_fields_to_write.end()
			); // i.e. that this key is not already present
			
			target_fields_to_write.insert(make_pair(target_field_selector, &i_name_matched->second));
		}
		for (auto i_explicit_toplevel = explicit_toplevel_mappings.begin();
				i_explicit_toplevel != explicit_toplevel_mappings.end();
				++i_explicit_toplevel)
		{
			// assert uniqueness in the multimap for now
			auto equal_range = explicit_toplevel_mappings.equal_range(i_explicit_toplevel->first);
			auto copied_first = equal_range.first;
			assert(++copied_first == equal_range.second); // means "size == 1"
			
			target_fields_to_write.insert(
				make_pair("." + i_explicit_toplevel->first, i_explicit_toplevel->second));
		}
		
		/* 1. Build an environment containing all the source-side fields that these depend on. */
		wrapper_file::environment basic_env;
		// insert the whole source object, bound by magic "__cake_from" ident
		// -- NOTE: must remove this before crossover, or we'll get an infinite loop!
		// FIXME: do we still need a binding of Cake name __cake_from?
	//	basic_env.insert(make_pair(string("__cake_from"), // cake name -- we will want "this" too, eventually
	//		(wrapper_file::bound_var_info) {
	//			"__cake_from", // cxx name
	//			/* source_data_type, */ "__cake_nonconst_from", // typeof
	//			source_module
	//			}));
		// approximate: all toplevel source-side fields, no non-toplevel ones
		/* Since we need not have a unique source field (might e.g. be an expression
		 * involving multiple source fields), build a Cake environment containing
		 * all source fields. */
		for (auto i_field = dynamic_pointer_cast<with_data_members_die>(
					source_data_type)->member_children_begin();
				i_field != dynamic_pointer_cast<with_data_members_die>(
					source_data_type)->member_children_end();
				++i_field)
		{
			assert((*i_field)->get_name());

			cerr << "adding name " << *(*i_field)->get_name() << endl;
			basic_env.insert(make_pair(*(*i_field)->get_name(),
				(wrapper_file::bound_var_info) {
					string("__cake_nonconst_from.") + *(*i_field)->get_name(), // cxx name
					"__cake_nonconst_from." + *(*i_field)->get_name(), // typeof
					source_module,
					true, // do not crossover!
					"__cake_default",
					"__cake_default"
				}));
		}
		// environment complete for now; create a context out of this environment
		wrapper_file::context ctxt(w, source_module, target_module, basic_env);
		ctxt.opt_val_corresp = (wrapper_file::context::val_info_s){ this->corresp };

		/* 2. For each target field, emit its pre-stub (if any) and extend the environment with the result.
		 * Note that the pre-stub needs to execute in a temporarily extended environment, with
		 * "this", "here" and "there". Rather than start a new C++ naming context using { }, we just hack
		 * our environment to map these Cake-keyword names to the relevant C++ expressions,
		 * then undo this hackery after we've bound a name to the result. Otherwise, we wouldn't be
		 * able to bind this name using "auto" and have it stay in scope for later. */
		for (auto i_target = target_fields_to_write.begin();
				i_target != target_fields_to_write.end();
				++i_target)
		{
			/* All dependencies should be in place now -- check this. */
			i_target->second->check_sanity();
			
			string target_field_selector = i_target->first;

			m_out << "{ // begin expression assigned to field " << target_field_selector;
			m_out.inc_level();

			/* If we *do* have a unique source field, then a bunch of other Cake idents
			 * are defined: "this", "here" and "there" (but not "that" -- I think). */
			optional<string> unique_source_field_selector;
			if (i_target->second->unique_source_field)
			{
				ostringstream s;
				for (auto i_name_part = i_target->second->unique_source_field->begin();
					i_name_part != i_target->second->unique_source_field->end();
					++i_name_part)
				{
					//if (i_name_part != i_target->second->unique_source_field->begin())
					{ s << "."; } // i.e. always begin with '.'
					s << *i_name_part;
				}
				unique_source_field_selector = s.str();
			}
			
			assert(target_field_selector == "" || *target_field_selector.begin() == '.');
			assert(!unique_source_field_selector
				|| *unique_source_field_selector == ""
				|| *unique_source_field_selector->begin() == '.');
			
			/* What's wrong with this? */
			write_single_field(ctxt, target_field_selector, unique_source_field_selector,
				i_target->second->stub, i_target->second->pre_stub, i_target->second->post_stub);
			
			m_out.dec_level();
			m_out << "}" << endl;
		}
			/* Didn't I have this single-field-at-a-time approach before?
			 * Why didn't it work?
			 */
			
			
// 			if (unique_source_field_selector)
// 			{
// 				// If we have a source-side infix stub, we must emit that.
// 				// It may use "this", "here" and "there" (but not "that").
// 				// So first we insert magic "__cake_this", "__cake_here" and "__cake_there"
// 				// entries to the Cake environment for this.
// 				extra_env.insert(make_pair("__cake_this",
// 					(wrapper_file::bound_var_info) {
// 						"__cake_nonconst_from." + *unique_source_field_selector, // cxx name
// 						"__cake_nonconst_from." + *unique_source_field_selector, // typeof
// 						source_module,
// 						false,
// 						"__cake_default"
// 					}));
// 				extra_env.insert(make_pair("__cake_here",
// 					(wrapper_file::bound_var_info) {
// 						"&__cake_from." + *unique_source_field_selector, // cxx name
// 						"&__cake_nonconst_from." + *unique_source_field_selector, // typeof
// 						source_module,
// 						false,
// 						"__cake_default"
// 					}));
// 			}
// 			// we always have a "there"
// 			extra_env.insert(make_pair("__cake_there",
// 				(wrapper_file::bound_var_info) {
// 					"&__cake_p_buf->" + target_field_selector, // cxx name
// 					"&__cake_p_buf->" + target_field_selector, // typeof
// 					source_module,
// 					false,
// 					"__cake_default"
// 				}));
// 			
// 			// compute the merged environment
// 			auto extended_env1 = w.merge_environment(ctxt.env, extra_env);
// 			ctxt.env = extended_env1;
// 			
// 			/* Evaluate the source expression in this environment. */
// 			auto status1 = w.emit_stub_expression_as_statement_list(ctxt,
// 				i_target->second->stub/*,
// 				sink_data_type*/);
// 			
// 			/* Keep things simple: assert there are no new bindings. 
// 			 * If there are, we'll have to stash the bindings with a different name, then 
// 			 * re-establish them in the post-stub. */
// 			assert(status1.new_bindings.size() == 0);
// 			// auto new_env1 = w.merge_environment(ctxt.env, status1.new_bindings);
// 			/* Bind the result under a special name (not __cake_it). 
// 			 * The source expression should always yield a value. */
// 			assert(status1.result_fragment != w.NO_VALUE && status1.result_fragment != "");
// 			// we extend the *basic* (shared) environment
// 			extended_env1["__cake_it"] = 
// 				(wrapper_file::bound_var_info) {
// 					status1.result_fragment,
// 					status1.result_fragment,  //shared_ptr<type_die>(),
// 					ctxt.modules.source,
// 					false,
// 					"__cake_default" };
// 			
// 			/* Now we can evaluate the pre-stub if there is one. It should use "this" if
// 			 * it depends on the field value. */
// 			auto extended_env2 = extended_env1; // provisional value
// 			if (i_target->second->pre_stub && GET_CHILD_COUNT(i_target->second->pre_stub) > 0)
// 			{
// 				auto status2 = w.emit_stub_expression_as_statement_list(ctxt,
// 					i_target->second->pre_stub);
// 				extended_env2 = w.merge_environment(extended_env1, status2.new_bindings);
// 					if (status2.result_fragment != w.NO_VALUE) extended_env2["__cake_it"] = 
// 						(wrapper_file::bound_var_info) {
// 							status2.result_fragment,
// 							status2.result_fragment,  //shared_ptr<type_die>(),
// 							ctxt.modules.source,
// 							false,
// 							"__cake_default"
// 						};
// 			}
// 			// stash the result in the basic env
// 			assert(extended_env2.find("__cake_it") != extended_env2.end());
// 			basic_env["__cake_source_" + target_field_selector]
// 			 = extended_env2["__cake_it"];
// 			
// 			// revert to the basic env, with this extension
// 			ctxt.env = basic_env;
// 		} // end for i_target
// 
// 		/* Now our environment contains bindings for each evaluated source expression,
// 		 * we can delete "from" and the original fields from the environment. 
// 		 * HMM. But should we? What if we have 
// 		 *
// 		 * destfield (twiddle(f, that))<-- f + g;   ?
// 		 * 
// 		 * Here our stub depends on retaining a binding of f in the post-context,
// 		 * i.e. that f can be crossed-over independently of the result of f and g.
// 		 * I don't see anything desperately wrong with this.
// 		 * Nevertheless, let's go for the simpler option now.  */
// 		for (auto i_field = dynamic_pointer_cast<with_data_members_die>(
// 					source_data_type)->member_children_begin();
// 				i_field != dynamic_pointer_cast<with_data_members_die>(
// 					source_data_type)->member_children_end();
// 				++i_field)
// 		{
// 			assert((*i_field)->get_name());
// 
// 			basic_env.erase(*(*i_field)->get_name());
// 		} 
// 		basic_env.erase("__cake_from");
// 
// 		/* 3. Crossover the environment. This has the effect of applying depended-on correspondences. */
// 		ctxt.modules.current = target_module;
// 		assert(ctxt.env.find("__cake_this") == ctxt.env.end());
// 		assert(ctxt.env.find("__cake_here") == ctxt.env.end());
// 		assert(ctxt.env.find("__cake_there") == ctxt.env.end());
// 		/* Now we need to "cross over" the environment. This crossover is different
// 		 * from the one in the wrapper code. We don't need to allocate or sync
// 		 * co-objects: the allocation has already been done, and we're in the middle
// 		 * of a sync process right now, most likely. Nevertheless, crossover_environment
// 		 * should work. But first we erase "this", "here" and "there" from the environment
// 		 * if we added them -- these are side-specific. */
// 		// crossover point
// 		ctxt.modules.current = target_module;
// 		m_out << "// source->sink crossover point" << endl;
// 		auto crossed_env = w.crossover_environment(source_module, basic_env, target_module, 
// 			/* no constraints */ 
// 			std::multimap< string, shared_ptr<type_die> >());
// 		
// 		/* 4, 5. Now finish the job: for each target field, compute any post-stub and assign. */
// 		for (auto i_target = target_fields_to_write.begin();
// 				i_target != target_fields_to_write.end();
// 				++i_target)
// 		{
// 			// always start with crossed-over environment
// 			ctxt.env = crossed_env;
// 			
// 			// HACK: code cloned from above
// 			optional<string> unique_source_field_selector;
// 			if (i_target->second->unique_source_field)
// 			{
// 				ostringstream s;
// 				for (auto i_name_part = i_target->second->unique_source_field->begin();
// 					i_name_part != i_target->second->unique_source_field->end();
// 					++i_name_part)
// 				{
// 					if (i_name_part != i_target->second->unique_source_field->begin())
// 					{ s << "."; }
// 					s << *i_name_part;
// 				}
// 				unique_source_field_selector = s.str();
// 			}
// 				
// 			// XXX: recover the stashed source value __cake_source + target_field_selector!
// 			// In the simple-source-expression, no-pre-stub case, how do we map from field name to this value?
// 			// -- the target side will have a name for it (not necessarily the source name!) 
// 			// -- we want this name to map to __cake_source_ + target_field_selector
// 			// In the more complex pre-stub and/or source expression case, how do we reference __cake_it?
// 			// -- the stub can *only* use "here", "there" and "that"
// 			// AH. It's a non-problem. Implicitly, we *always* write __cake_it!
// 			// That's all the sink-side rule ever does -- there are no sink-side expressions.
// 			
// 			/* Now we add "that", "there" and "here" (but not "this") to the environment. */
// 			if (i_target->second->unique_source_field)
// 			{
// 				ctxt.env.insert(make_pair("__cake_that",
// 					(wrapper_file::bound_var_info) {
// 						"__cake_nonconst_from." + *unique_source_field_selector, // cxx name
// 						"__cake_nonconst_from." + *unique_source_field_selector, // typeof
// 						target_module,
// 						false,
// 						"__cake_default"
// 					}));
// 
// 				ctxt.env.insert(make_pair("__cake_there",
// 					(wrapper_file::bound_var_info) {
// 						"&__cake_nonconst_from." + *unique_source_field_selector, // cxx name
// 						"&__cake_nonconst_from." + *unique_source_field_selector, // typeof
// 						target_module,
// 						false,
// 						"__cake_default"
// 					}));
// 			} 
// 			string target_field_selector = i_target->first;
// 			// we can always add "here"
// 			ctxt.env.insert(make_pair("__cake_here",
// 				(wrapper_file::bound_var_info) {
// 					"&__cake_p_buf->" + target_field_selector, // cxx name
// 					"&__cake_p_buf->" + target_field_selector, // typeof
// 					target_module,
// 					false,
// 					"__cake_default"
// 				}));
// 			// we can also always add "it"!
// 			ctxt.env.insert(make_pair("__cake_it", ctxt.env["__cake_source_" + i_target->first]));
// 
// 			/* Now emit the post-stub. */
// 			auto new_env3 = ctxt.env; // provisional value
// 			if (i_target->second->post_stub && GET_CHILD_COUNT(i_target->second->post_stub) > 0)
// 			{
// 				auto status3 = w.emit_stub_expression_as_statement_list(ctxt,
// 					i_target->second->post_stub);
// 				new_env3 = w.merge_environment(ctxt.env, status3.new_bindings);
// 					if (status3.result_fragment != w.NO_VALUE) new_env3["__cake_it"] = 
// 						(wrapper_file::bound_var_info) {
// 							status3.result_fragment,
// 							status3.result_fragment,  //shared_ptr<type_die>(),
// 							ctxt.modules.sink,
// 							false,
// 							"__cake_default"
// 						};
// 			}
// 			
// 			/* Now we have an "it": either the output of the stub, if there was one,
// 			 * or else the converted field value. */
// 			
// // 			auto result = w.emit_stub_expression_as_statement_list(ctxt,
// // 				i_name_matched->second.stub/*,
// // 				sink_data_type*/);
// // 				//, modules, pre_context_pair, post_context_pair,
// // 				//boost::shared_ptr<dwarf::spec::type_die>(), env);
// // 			m_out << "assert(" << result.success_fragment << ");" << std::endl;
// // 			m_out << "__cake_p_buf->" << target_field_selector
// // 				<< " = ";
// // 			m_out << w.component_pair_classname(modules);
// // 			m_out << "::value_convert_from_"
// // 				<< ((modules.first == source_module) ? "first_to_second" : "second_to_first")
// // 				<< "<" << "__typeof(__cake_p_buf->" << target_field_selector << ")"
// // 				<< ">(__cake_from." << target_field_selector << ");" << std::endl;
// 			
// 			assert(ctxt.env["__cake_it"].cxx_name != "");
// 			m_out << "__cake_p_buf->" << target_field_selector << " = " <<
// 				ctxt.env["__cake_it"].cxx_name << ";" << endl;
				
//		} // end for i_target

		// output return statement
		m_out << "return *__cake_p_buf;" << endl;
	}

}
