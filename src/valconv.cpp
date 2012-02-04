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
	link_derivation::iface_pair
	codegen_context::get_ifaces() const
	{
		return derivation.sorted(make_pair(modules.source, modules.sink));
	}
	
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
	
	/* In the virtual case, we might want to emit a reference type as the return type. */
	void virtual_value_conversion::emit_header(optional<string> return_typename, 
		bool emit_struct_keyword/* = true */, bool emit_template_prefix/* = true */,
		bool emit_return_typename/* = true*/)
	{
		if (return_typename && treat_target_type_as_user_allocated())
		{
			this->structural_value_conversion::emit_header(*return_typename + "&",
				emit_struct_keyword, emit_template_prefix, emit_return_typename
				);
		} else this->structural_value_conversion::emit_header(return_typename,
				emit_struct_keyword, emit_template_prefix, emit_return_typename
				);
	}
	
	/* Same job, but for emit_signature. */
	void virtual_value_conversion::emit_signature(bool emit_return_type/* = true*/, 
		bool emit_default_argument /* = true */) 
	{
		m_out << (emit_return_type ? (treat_target_type_as_user_allocated() ? to_typename + "&" : to_typename ) : "") 
			<< " operator()(const " << from_typename << "& __cake_from, " 
			<< to_typename << "*__cake_p_to"
			<< (emit_default_argument ? " = 0" : "")
			<< ") const";
	}
	
	bool virtual_value_conversion::treat_target_type_as_user_allocated()
	{
		if (explicit_field_corresps.size() == 0
		 && sink_infix_stub
		 && GET_CHILD_COUNT(sink_infix_stub) > 0
		 && GET_TYPE(GET_CHILD(sink_infix_stub, 0)) == CAKE_TOKEN(INVOKE_WITH_ARGS))
		{
			// we might be on
			auto invoke_ast = GET_CHILD(sink_infix_stub, 0);
			auto invoked_function_ast = GET_CHILD(invoke_ast, 
				GET_CHILD_COUNT(invoke_ast) - 1);
			definite_member_name dmn;
			if (GET_TYPE(invoked_function_ast) == CAKE_TOKEN(DEFINITE_MEMBER_NAME))
			{
				dmn = read_definite_member_name(invoked_function_ast);
			}
			else if (GET_TYPE(invoked_function_ast) == CAKE_TOKEN(IDENT))
			{
				dmn = vector<string>(1, unescape_ident(CCP(GET_TEXT(invoked_function_ast))));
			}
			else return false;
			
			auto found = sink->get_ds().toplevel()->visible_resolve(
				dmn.begin(), dmn.end());
			if (!found) return false;
			auto subprogram = dynamic_pointer_cast<subprogram_die>(found);
			if (!subprogram) return false;
			if (!subprogram->get_type()) return false;
			
			if (subprogram->get_type()->get_concrete_type()->get_tag() == DW_TAG_pointer_type
			|| subprogram->get_type()->get_concrete_type()->get_tag() == DW_TAG_pointer_type)
				return true;
		}
		return false;
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
        emit_header(/* false */ optional<string>(), true); // FIXME: looks buggy
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
	
	void virtual_value_conversion::emit_body()
	{
		//m_out << "assert(false);" << endl;
		this->structural_value_conversion::emit_body();
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
		
		auto found_target = dynamic_pointer_cast<with_named_children_die>(owner->sink_concrete_type)
			->resolve(target.begin(), target.end());
		assert(found_target);
		
		auto found_target_field = dynamic_pointer_cast<member_die>(found_target);
		assert(found_target_field && found_target_field->get_type());
		
		auto found_target_field_type = found_target_field->get_type();
		assert(found_target_field_type);
		
		shared_ptr<member_die> found_source_field;
		shared_ptr<type_die> found_source_field_type;
		if (unique_source_field && unique_source_field->size() > 0)
		{
			auto found_source = dynamic_pointer_cast<with_named_children_die>(owner->source_concrete_type)
				->resolve(unique_source_field->begin(), unique_source_field->end());
			assert(found_source);
			
			found_source_field = dynamic_pointer_cast<member_die>(found_source);
			assert(found_source_field && found_source_field->get_type());
			
			found_source_field_type = found_source_field->get_type();
			assert(found_source_field_type);
		} else unique_source_field = optional<definite_member_name>(); // kill the "void" dmn
		
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
			source_module(w.m_d.module_of_die(source_data_type)),
			target_module(w.m_d.module_of_die(sink_data_type)),
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
						if (GET_TYPE(leftValuePattern) == CAKE_TOKEN(KEYWORD_VOID))
							RAISE(leftValuePattern, 
								"cannot use void field specifier in bidirectional rule.");
						if (GET_TYPE(rightValuePattern) == CAKE_TOKEN(KEYWORD_VOID))
							RAISE(rightValuePattern,
								"cannot use void field specifier in bidirectional rule.");
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
										GET_TYPE(source_is_on_left ? leftInfixStub : rightInfixStub) == CAKE_TOKEN(NAME_AND_INTERPRETATION) ?
											read_definite_member_name(GET_CHILD(source_is_on_left ? leftInfixStub : rightInfixStub, 0))
											: optional<definite_member_name>(),
									/* antlr::tree::Tree *stub; */
										source_is_on_left ? leftValuePattern : rightValuePattern,
									/* module_ptr pre_context; */
										source_is_on_left ? w.m_d.module_of_die(source_data_type) : w.m_d.module_of_die(sink_data_type),
									/* antlr::tree::Tree *pre_stub; */
										source_is_on_left ? leftInfixStub : rightInfixStub,
									/* module_ptr post_context; */
										source_is_on_left ? w.m_d.module_of_die(sink_data_type) : w.m_d.module_of_die(source_data_type),
									/* antlr::tree::Tree *post_stub; */
										source_is_on_left ? rightInfixStub : leftInfixStub
								}));
					}
					break;
					case (CAKE_TOKEN(LR_DOUBLE_ARROW)):
					{
						if (!source_is_on_left) continue; // this field rule is not relevant
						// target is on right, source is on left
						INIT;
						BIND2(refinementRule, leftStubExpr);
						BIND2(refinementRule, leftInfixStub);
						BIND2(refinementRule, rightInfixStub);
						BIND2(refinementRule, rightValuePattern);
						definite_member_name target_member_name;
						if (GET_TYPE(rightValuePattern) == CAKE_TOKEN(KEYWORD_VOID))
							target_member_name = std::vector<string>();
						else target_member_name = read_definite_member_name(rightValuePattern);
						explicit_field_corresps.insert(make_pair(
							target_member_name,
							(member_mapping_rule_base)
							{
								/* structural_value_conversion *owner, */
									this,
								/* definite_member_name target; */ 
									target_member_name,
								/* optional<definite_member_name> unique_source_field; */
									GET_TYPE(leftStubExpr) == CAKE_TOKEN(NAME_AND_INTERPRETATION) ?
										read_definite_member_name(GET_CHILD(leftStubExpr, 0))
										: optional<definite_member_name>(),
								/* antlr::tree::Tree *stub; */
									leftStubExpr,
								/* module_ptr pre_context; */
									w.m_d.module_of_die(source_data_type),
								/* antlr::tree::Tree *pre_stub; */
									leftInfixStub,
								/* module_ptr post_context; */
									w.m_d.module_of_die(sink_data_type),
								/* antlr::tree::Tree *post_stub; */
									rightInfixStub
							}));

					}
					break;
					case (CAKE_TOKEN(RL_DOUBLE_ARROW)):
					{
						if (source_is_on_left) continue; // this field rule is not relevant
						// source is on right, target on left
						INIT;
						BIND2(refinementRule, leftValuePattern);
						BIND2(refinementRule, leftInfixStub);
						BIND2(refinementRule, rightInfixStub);
						BIND2(refinementRule, rightStubExpr);
						definite_member_name target_member_name;
						if (GET_TYPE(leftValuePattern) == CAKE_TOKEN(KEYWORD_VOID))
							target_member_name = vector<string>();
						else target_member_name = read_definite_member_name(leftValuePattern);
						explicit_field_corresps.insert(make_pair(
							target_member_name,
							(member_mapping_rule_base)
							{
								/* structural_value_conversion *owner, */
									this,
								/* definite_member_name target; */ 
									target_member_name,
								/* optional<definite_member_name> unique_source_field; */
									GET_TYPE(rightStubExpr) == CAKE_TOKEN(NAME_AND_INTERPRETATION) ?
										read_definite_member_name(GET_CHILD(rightStubExpr, 0))
										: optional<definite_member_name>(),
								/* antlr::tree::Tree *stub; */
									rightStubExpr,
								/* module_ptr pre_context; */
									w.m_d.module_of_die(sink_data_type),
								/* antlr::tree::Tree *pre_stub; */
									rightInfixStub,
								/* module_ptr post_context; */
									w.m_d.module_of_die(source_data_type),
								/* antlr::tree::Tree *post_stub; */
									leftInfixStub
							}));

					}
					break;
					case (CAKE_TOKEN(LR_DOUBLE_ARROW_Q)):
					{
						if (!source_is_on_left) continue; // not relevant in this direction
						// source is on left, target on right
						init_is_identical = false;
						if (!init_only) continue; // not relevant unless we're generating a distinct init rule
						INIT;
						BIND2(refinementRule, leftStubExpr);
						BIND2(refinementRule, leftInfixStub);
						BIND2(refinementRule, rightInfixStub);
						BIND2(refinementRule, rightValuePattern);
						definite_member_name target_member_name;
						if (GET_TYPE(rightValuePattern) == CAKE_TOKEN(KEYWORD_VOID))
							target_member_name = vector<string>();
						else target_member_name = read_definite_member_name(rightValuePattern);
						explicit_field_corresps.insert(make_pair(
							target_member_name,
							(member_mapping_rule_base)
							{
								/* structural_value_conversion *owner, */
									this,
								/* definite_member_name target; */ 
									target_member_name,
								/* optional<definite_member_name> unique_source_field; */
									GET_TYPE(leftStubExpr) == CAKE_TOKEN(NAME_AND_INTERPRETATION) ?
										read_definite_member_name(GET_CHILD(leftStubExpr, 0))
										: optional<definite_member_name>(),
								/* antlr::tree::Tree *stub; */
									leftStubExpr,
								/* module_ptr pre_context; */
									w.m_d.module_of_die(source_data_type),
								/* antlr::tree::Tree *pre_stub; */
									leftInfixStub,
								/* module_ptr post_context; */
									w.m_d.module_of_die(sink_data_type),
								/* antlr::tree::Tree *post_stub; */
									rightInfixStub
							}));
					}
					break;
					case (CAKE_TOKEN(RL_DOUBLE_ARROW_Q)):
					{
						if (source_is_on_left) continue; // not relevant in this direction
						// source is on right, target on left
						init_is_identical = false;
						if (!init_only) continue; // not relevant unless we're generating a distinct init rule
						INIT;
						BIND2(refinementRule, leftValuePattern);
						BIND2(refinementRule, leftInfixStub);
						BIND2(refinementRule, rightInfixStub);
						BIND2(refinementRule, rightStubExpr);
						definite_member_name target_member_name;
						if (GET_TYPE(leftValuePattern) == CAKE_TOKEN(KEYWORD_VOID))
							target_member_name = vector<string>();
						else target_member_name = read_definite_member_name(leftValuePattern);
						explicit_field_corresps.insert(make_pair(
							target_member_name,
							(member_mapping_rule_base)
							{
								/* structural_value_conversion *owner, */
									this,
								/* definite_member_name target; */ 
									target_member_name,
								/* optional<definite_member_name> unique_source_field; */
									GET_TYPE(rightStubExpr) == CAKE_TOKEN(NAME_AND_INTERPRETATION) ?
										read_definite_member_name(GET_CHILD(rightStubExpr, 0))
										: optional<definite_member_name>(),
								/* antlr::tree::Tree *stub; */
									rightStubExpr,
								/* module_ptr pre_context; */
									w.m_d.module_of_die(sink_data_type),
								/* antlr::tree::Tree *pre_stub; */
									rightInfixStub,
								/* module_ptr post_context; */
									w.m_d.module_of_die(source_data_type),
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
		
//		vector<string> touched_toplevel_field_names;
		
		for (auto i_mapping = explicit_field_corresps.begin(); 
			i_mapping != explicit_field_corresps.end();
			++i_mapping)
		{
			if (i_mapping->first.size() <= 1)
			{
//				if (i_mapping->first.size() > 0) touched_toplevel_field_names.insert(
//					i_mapping->first.at(0));
				explicit_toplevel_mappings.insert(
					make_pair(
						(i_mapping->first.size() > 0 ? i_mapping->first.at(0) : ""), 
						&i_mapping->second));
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
			 = dynamic_pointer_cast<structure_type_die>(sink_concrete_type)
			 	->member_children_begin();
			i_member != dynamic_pointer_cast<structure_type_die>(sink_concrete_type)
			 	->member_children_end();
			++i_member)
		{
			assert((*i_member)->get_name());
			if (name_matched_mappings.find(*(*i_member)->get_name())
				== name_matched_mappings.end()
				&& explicit_toplevel_mappings.find(*(*i_member)->get_name())
				== explicit_toplevel_mappings.end())
			{
				/* We have a name-matching candidate -- look for like-named
				 * field in opposing structure. */

				auto source_as_struct = dynamic_pointer_cast<structure_type_die>(
					source_concrete_type);

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
						dynamic_pointer_cast<with_named_children_die>(this->source_concrete_type)
							->named_child(i_mapping->second.target.at(0))
						)->get_type(),
					dynamic_pointer_cast<member_die>(
						dynamic_pointer_cast<with_named_children_die>(this->sink_concrete_type)
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
			auto source_member_type = source_member ? source_member->get_type() : shared_ptr<type_die>();
			auto sink_member_type = sink_member ? sink_member->get_type() : shared_ptr<type_die>();
			
			if (source_member_type && sink_member_type)
			{
				auto pair = make_pair(source_member_type, sink_member_type);

				assert(pair.first && pair.second);
				auto retval = working.insert(pair);
				assert(retval.second); // assert that we actually inserted something
				// FIX: use multimap::equal_range to generate a list of all rules
				// that have the same string, then group these as constraints 
				// and add them to the dependency
			}
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
	
//	void primitive_value_conversion::emit_declare_buffer
	
	void primitive_value_conversion::emit_source_object_alias()
	{
		/* HACK: create a non-const "from" reference, because we want to index
		 * our templates by CV-qualifier-free typenames. */
		string dest_type_string	=
				"boost::remove_const<"
					"boost::remove_reference< "
						"__typeof(__cake_from)"
					" >::type "
				" >::type &";

		m_out << dest_type_string << " __cake_nonconst_from = "
			"const_cast< "
				<< dest_type_string <<
			" >(__cake_from);" << endl;
	
	
	}
	
	void primitive_value_conversion::emit_target_buffer_declaration()
	{
		/* Create or find buffer */
		m_out << w.get_type_name(sink_data_type) << " __cake_tmp, *__cake_p_buf;" << endl
			<< "if (__cake_p_to) __cake_p_buf = __cake_p_to; else __cake_p_buf = &__cake_tmp;" << endl;
	}

	// override for the virtual corresp case
	void virtual_value_conversion::emit_target_buffer_declaration()
	{
		/* Create or find buffer */
		// if we are user-allocated, we just declare a pointer
		if (treat_target_type_as_user_allocated())
		{
			m_out << w.get_type_name(sink_data_type) << " *__cake_p_buf = __cake_p_to;" << endl
				<< "// assert (__cake_p_to); /* can be null to begin; sink stub will provide value... */" << endl;
		}
		else
		{
			m_out << w.get_type_name(sink_data_type) << " *__cake_p_buf = __cake_p_to;" << endl
				<< "assert (__cake_p_to); /* no temporary buffer option for this virtual corresp */" << endl;
		}
	}
	
	void primitive_value_conversion::emit_initial_declarations()
	{
		emit_source_object_alias();
		emit_target_buffer_declaration();
	}
	
	void primitive_value_conversion::emit_body()
	{
		emit_initial_declarations();
		
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

		bool is_void_target = (target_field_selector == "");
		bool is_void_source = (
			GET_TYPE(source_expr) == CAKE_TOKEN(NAME_AND_INTERPRETATION)
			&& GET_TYPE(GET_CHILD(source_expr, 0)) == CAKE_TOKEN(KEYWORD_VOID));
		
		wrapper_file::environment extra_env;
		// Add the magic keyword fields
		// HACK: for void source expressions, "this" and "that" refer to
		// the containing object.
		if (unique_source_field_selector || is_void_source)
		{
			// If we have a source-side infix stub, we must emit that.
			// It may use "this", "here" and "there" (but not "that").
			// So first we insert magic "__cake_this", "__cake_here" and "__cake_there"
			// entries to the Cake environment for this.
			extra_env.insert(make_pair("__cake_this",
				(wrapper_file::bound_var_info) {
					"__cake_nonconst_from" + (is_void_source ? "" : *unique_source_field_selector), // cxx name
					"__cake_nonconst_from" + (is_void_source ? "" : *unique_source_field_selector), // typeof
					source_module,
					false
				}));
			extra_env.insert(make_pair("__cake_here",
				(wrapper_file::bound_var_info) {
					"&__cake_nonconst_from" + (is_void_source ? "" : *unique_source_field_selector), // cxx name
					"&__cake_nonconst_from" + (is_void_source ? "" : *unique_source_field_selector), // typeof
					source_module,
					false
				}));
		}
		// we always have a "there"
		extra_env.insert(make_pair("__cake_there",
			(wrapper_file::bound_var_info) { // v-- works okay for empty selector (void target) too
				"&((*__cake_p_buf)" + target_field_selector + ")", // cxx name
				"&((*__cake_p_buf)" + target_field_selector + ")", // typeof
				source_module,
				false
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
		 * The source expression should always yield a value,
		 * *unless* this is a void-target rule,
		 * or unless this is a void-source, in which case it must come from the source stub. */
		// we extend the *basic* (shared) environment
		if (status1.result_fragment != w.NO_VALUE && status1.result_fragment != "")
		{
			extended_env1["__cake_it"] = 
				(wrapper_file::bound_var_info) {
					status1.result_fragment,
					status1.result_fragment,  //shared_ptr<type_die>(),
					ctxt.modules.source,
					false
				};
			extended_env1["__cake_source_value"] = extended_env1["__cake_it"];
		}
		// the semantics of "that" mean that we have to remember this value
		assert((extended_env1.find("__cake_it") != extended_env1.end())
		|| is_void_source);
		// else our source expression yielded no value --
		// okay if we're void target OR if the infix stub is going to yield a value

		/* Now we can evaluate the pre-stub if there is one. It should use "this" if
		 * it depends on the field value. */
		auto extended_env2 = extended_env1; // provisional value
		if (source_infix && GET_CHILD_COUNT(source_infix) > 0)
		{
			auto status2 = w.emit_stub_expression_as_statement_list(ctxt,
				GET_CHILD(source_infix, 0));
			extended_env2 = w.merge_environment(extended_env1, status2.new_bindings);
			if (status2.result_fragment != w.NO_VALUE) extended_env2["__cake_it"] = 
				(wrapper_file::bound_var_info) {
					status2.result_fragment,
					status2.result_fragment,  //shared_ptr<type_die>(),
					ctxt.modules.source,
					false
				};
		}
		// stash the result (if we have one) in the basic env
		if (extended_env2.find("__cake_it") != extended_env2.end())
		{
			basic_env[/*"__cake_source_" + target_field_selector.substr(1)*/ "__cake_it"]
				 = extended_env2["__cake_it"];
		} else assert(is_void_source);
		if (extended_env2.find("__cake_it") != extended_env2.end())
		{
			basic_env["__cake_source_value"] = extended_env2["__cake_it"];
		}

		// revert to the basic env, with this "it" and "source value" extensions
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
		auto crossed_env = w.crossover_environment_and_sync(
			source_module, basic_env, target_module, 
			/* no constraints */ 
			std::multimap< string, shared_ptr<type_die> >(), false, true);

		// always start with crossed-over environment
		ctxt.env = crossed_env;

		/* Now we add "that", "there" and "here" (but not "this") to the environment. */
		if (unique_source_field_selector)
		{
			/* Semantics of "that" -- it's the result of converting the value
			 * on the other side. */
			assert(ctxt.env.find("__cake_source_value") != ctxt.env.end());
			ctxt.env.insert(make_pair("__cake_that", ctxt.env["__cake_source_value"]));
			
			//	(wrapper_file::bound_var_info) {
			//		"__cake_nonconst_from" + *unique_source_field_selector, // cxx name
			//		"__cake_nonconst_from" + *unique_source_field_selector, // typeof
			//		target_module,
			//		false,
			//		"__cake_default",
			//		"__cake_default"
			//	}));

			ctxt.env.insert(make_pair("__cake_there",
				(wrapper_file::bound_var_info) {
					"&__cake_nonconst_from" + *unique_source_field_selector, // cxx name
					"&__cake_nonconst_from" + *unique_source_field_selector, // typeof
					target_module,
					false
				}));
		} 

		// we can always add "here"
		ctxt.env.insert(make_pair("__cake_here",
			(wrapper_file::bound_var_info) {
				"&((*__cake_p_buf)" + target_field_selector + ")", // cxx name
				"&((*__cake_p_buf)" + target_field_selector + ")", // typeof
				target_module,
				false
			}));
		// we should have "it" in the environment, just from crossover
		assert(ctxt.env.find("__cake_it") != ctxt.env.end());

		/* Now emit the post-stub. */
		//auto new_env3 = ctxt.env; // provisional value
		if (sink_infix && GET_CHILD_COUNT(sink_infix) > 0)
		{
			auto status3 = w.emit_stub_expression_as_statement_list(ctxt,
				GET_CHILD(sink_infix, 0));
			ctxt.env = w.merge_environment(ctxt.env, status3.new_bindings);
			if (status3.result_fragment != w.NO_VALUE) ctxt.env["__cake_it"] = 
				(wrapper_file::bound_var_info) {
					status3.result_fragment,
					status3.result_fragment,  //shared_ptr<type_die>(),
					ctxt.modules.sink,
					false
				};
		}

		/* Now we have an "it": either the output of the stub, if there was one,
		 * or else the converted field value. */
		if (!is_void_target)
		{
			assert(ctxt.env["__cake_it"].cxx_name != "");
			m_out << "(*__cake_p_buf)" << target_field_selector << " = " <<
				ctxt.env["__cake_it"].cxx_name << ";" << endl;
		}
	}
	
	void structural_value_conversion::emit_body()
	{
		emit_initial_declarations();
		/* Here's how we emit these.
		 * 
		 * 0. Build the list of sink-side (target) fields we are going to write.
		 * 1. Build an environment containing all the source-side fields that these depend on.
		 * 2. For each target field, emit its pre-stub (if any) and extend the environment with the result.
		 * 3. Crossover the environment. This has the effect of applying depended-on correspondences.
		 * 4. For each target field, emit its post-stub (if any) and extend the environment with the result.
		 * 5. For each target field, assign its value.
		 *
		 * Actually I missed out some steps.
		 * 1a. Emit the overall source-side stub.
		 * 5a. Emit the overall sink-side stub.
		 */
		
		/* 0. Build the list of sink-side (target) fields we are going to write. */
		multimap<string, member_mapping_rule *> target_fields_to_write;
		set<string> target_field_selectors;
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
			
			assert(target_field_selector != "");
			target_fields_to_write.insert(make_pair(target_field_selector, &i_name_matched->second));
			target_field_selectors.insert(target_field_selector);
		}
		for (auto i_explicit_toplevel = explicit_toplevel_mappings.begin();
				i_explicit_toplevel != explicit_toplevel_mappings.end();
				++i_explicit_toplevel)
		{
			// assert uniqueness in the multimap for now
			auto equal_range = explicit_toplevel_mappings.equal_range(i_explicit_toplevel->first);
			auto copied_first = equal_range.first;
			assert(++copied_first == equal_range.second); // means "size == 1"
			
			auto selector = i_explicit_toplevel->first == "" ? 
						""
						: "." + i_explicit_toplevel->first;
			target_fields_to_write.insert(
				make_pair(
					selector, 
					i_explicit_toplevel->second
				)
			);
			target_field_selectors.insert(selector);
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
					source_concrete_type)->member_children_begin();
				i_field != dynamic_pointer_cast<with_data_members_die>(
					source_concrete_type)->member_children_end();
				++i_field)
		{
			assert((*i_field)->get_name());

			cerr << "adding name " << *(*i_field)->get_name() << endl;
			basic_env.insert(make_pair(*(*i_field)->get_name(),
				(wrapper_file::bound_var_info) {
					string("__cake_nonconst_from.") + *(*i_field)->get_name(), // cxx name
					"__cake_nonconst_from." + *(*i_field)->get_name(), // typeof
					source_module,
					false // was true / "do not crossover!" -- WHY NOT? "once only"/balanced semantics?
				}));
		}
		// environment complete for now; create a context out of this environment
		wrapper_file::context ctxt(w, source_module, target_module, basic_env);
		ctxt.opt_val_corresp = (wrapper_file::context::val_info_s){ this->corresp };
		
		/* 1a. Emit the overall source-side stub, if there is one. */
		if (source_infix_stub && GET_CHILD_COUNT(source_infix_stub) > 0)
		{
			environment extra_env = basic_env;
			// source-side stubs have "this", "here" and "there"
			extra_env.insert(make_pair("__cake_this",
				(wrapper_file::bound_var_info) {
					"__cake_nonconst_from", // cxx name
					"__cake_nonconst_from", // typeof
					source_module,
					false
				}));
			extra_env.insert(make_pair("__cake_here",
				(wrapper_file::bound_var_info) {
					/* GAH. What is the address of a local reference? 
					 * I think it should be the address of the referenced object. */
					"&__cake_nonconst_from", // cxx name
					"&__cake_nonconst_from", // typeof
					source_module,
					false
				}));
			extra_env.insert(make_pair("__cake_there",
				(wrapper_file::bound_var_info) {
					"__cake_p_buf", // cxx name
					"__cake_p_buf", // typeof
					source_module,
					false
				}));
			auto saved_env = ctxt.env;
			ctxt.env = extra_env;
			auto return_status = w.emit_stub_expression_as_statement_list(
				ctxt, 
				GET_CHILD(source_infix_stub, 0)
			);
			// restore previous environment
			ctxt.env = saved_env;
			// FIXME: do stuff with the return status (merge environment, use success?)
		}

		/* 2. For each target field, emit its pre-stub (if any) and extend the environment with the result.
		 * Note that the pre-stub needs to execute in a temporarily extended environment, with
		 * "this", "here" and "there". Rather than start a new C++ naming context using { }, we just hack
		 * our environment to map these Cake-keyword names to the relevant C++ expressions,
		 * then undo this hackery after we've bound a name to the result. Otherwise, we wouldn't be
		 * able to bind this name using "auto" and have it stay in scope for later. */
		for (auto i_selector = target_field_selectors.begin();
				i_selector != target_field_selectors.end();
				++i_selector)
		{
			auto rules_for_this_selector = target_fields_to_write.equal_range(*i_selector);
			auto rules_count = srk31::count(rules_for_this_selector.first, rules_for_this_selector.second);
			assert(*i_selector == "" || rules_count == 1);
			
			for (auto i_target = rules_for_this_selector.first;
					i_target != rules_for_this_selector.second;
					++i_target)
			{
				string target_field_selector = i_target->first;
				bool is_void_target = (target_field_selector == "");
				
				/* All dependencies should be in place now -- check this. */
				if (!is_void_target) i_target->second->check_sanity();

				m_out << "{ // begin expression assigned to field " 
					<< (target_field_selector == "" ? "(no field)" : target_field_selector);
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
		}
		/* Emit overall sink-side stub. Since we have  */
		if (sink_infix_stub && GET_CHILD_COUNT(sink_infix_stub) > 0)
		{
			// make sure we are in the sink module context now
			ctxt.modules.current = target_module;
			// we cross over this env...
			// there should already be some stuff in there
			assert(basic_env.size() > 0);
			auto crossed_env = w.crossover_environment_and_sync(
				source_module, basic_env, target_module, 
				/* no constraints */ 
				std::multimap< string, shared_ptr<type_die> >(), false, true);
		
			// sink-side stubs have "this", "that", "here" and "there". 
			// NOTE: "this" and "that" mean the same here!? These semantics are
			// messed up, but they are all I can think of right now that makes sense.
			// Maybe "that" is never needed, and "this" or "there" are all we should use?
// 			extra_env.insert(make_pair("__cake_that",
// 				(wrapper_file::bound_var_info) {
// 					"(*__cake_p_buf)", // cxx name
// 					"(*__cake_p_buf)", // typeof
// 					target_module,
// 					false
// 				}));

			crossed_env.insert(make_pair("__cake_this",
				(wrapper_file::bound_var_info) {
					"(*__cake_p_buf)", // cxx name
					"(*__cake_p_buf)", // typeof
					target_module,
					false
				}));
			crossed_env.insert(make_pair("__cake_here",
				(wrapper_file::bound_var_info) {
					"__cake_p_buf", // cxx name
					"__cake_p_buf", // typeof
					target_module,
					false
				}));
			crossed_env.insert(make_pair("__cake_there",
				(wrapper_file::bound_var_info) {
					"(&__cake_nonconst_from)", // cxx name
					"(&__cake_nonconst_from)", // typeof
					target_module,
					false
				}));
			auto saved_env = ctxt.env;
			ctxt.env = crossed_env;
			
			auto return_status = w.emit_stub_expression_as_statement_list(
				ctxt, 
				GET_CHILD(sink_infix_stub, 0)
			);
			
			ctxt.env = saved_env;
		}

		// output return statement
		m_out << "return *__cake_p_buf;" << endl;
	}

}
