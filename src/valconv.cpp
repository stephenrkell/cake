#include "util.hpp"
#include "parser.hpp"
#include "module.hpp"
#include "link.hpp"
#include "valconv.hpp"
#include "wrapsrc.hpp"
#include <sstream>
#include <cmath>

using boost::dynamic_pointer_cast;
using std::shared_ptr;
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
	{ return w.m_d.get_ns_prefix() + "::" + w.m_d.name_of_module(source); }
	string value_conversion::sink_fq_namespace() const
	{ return w.m_d.get_ns_prefix() + "::" + w.m_d.name_of_module(sink); }
	link_derivation::iface_pair
	codegen_context::get_ifaces() const
	{
		return derivation.sorted(make_pair(modules.source, modules.sink));
	}
	std::ostream& operator<<(std::ostream& s, const environment& env)
	{	
		for (auto i_el = env.begin(); i_el != env.end(); ++i_el)
		{
			s << i_el->first << " : " <<  i_el->second.cxx_name << std::endl;
		}
		return s;
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
			<< " @0x" << std::hex << c.source_data_type->get_offset() << std::dec
			<< " to "
			<< c.sink->get_filename() << " :: " 
			<< (c.sink_data_type->get_name() ? *c.sink_data_type->get_name() : "<anonymous>")
			<< " @0x" << std::hex << c.sink_data_type->get_offset() << std::dec
			<< ", " << (c.source_is_on_left ? "left to right" : "right to left")
			<< ", " << (c.init_only ? "initialization" : "update")
			<< " }";
		return s;
	}
	
	value_conversion::value_conversion(wrapper_file& w,
		const basic_value_conversion& basic)
	 : basic_value_conversion(basic), w(w), 
	 	source_concrete_type(source_data_type->get_concrete_type()),
		sink_concrete_type(sink_data_type->get_concrete_type()),
		from_typename(source_concrete_type ? w.get_type_name(source_concrete_type) : "(no concrete type)"),
		to_typename(sink_concrete_type ? w.get_type_name(sink_concrete_type) : "(no concrete type)")
	{
		
	}
			
	void value_conversion::emit_preamble()
	{
        emit_header(optional<string>(to_typename), false, false); // no struct keyword, no template<>
        m_out() << "::" << endl;
        emit_signature(false, false); // NO return type, NO default argument
        m_out() << endl << "{" << endl;
        m_out().inc_level();
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
		
		m_out() << (emit_template_prefix ? "template <>\n" : "" )
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
	
	/* In the virtual case, we might want to emit a reference type as the return type. 
	 * This is because
	 * in the uio case, and maybe others, we really have the stub function (uio_setup)
	 * allocate the co-object. But, to avoid engaging the co-object runtime
	 * (WHY?)
	 * */
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
		m_out() << (emit_return_type ? (treat_target_type_as_user_allocated() ? to_typename + "&" : to_typename ) : "") 
			<< " operator()(const " << from_typename << "& __cake_from, " 
			<< to_typename << "*__cake_p_to"
			<< (emit_default_argument ? " = 0" : "")
			<< ") const";
	}
	
	bool virtual_value_conversion::treat_target_type_as_user_allocated()
	{
		// yes if incomplete
		if (!w.compiler.cxx_is_complete_type(canonicalise_type(sink_data_type, sink, w.compiler))) return true;
		
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
			
			auto found = sink->get_ds().toplevel()->resolve_visible(
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
	
	srk31::indenting_ostream&
	value_conversion::m_out()
	{ return *w.p_out; }
		
	void value_conversion::emit_signature(bool emit_return_type/* = true*/, 
		bool emit_default_argument /* = true */) 
	{
		// HACK: if our target type is incomplete, the best we can do is return a reference
		string reference_insert;
		if (!w.compiler.cxx_is_complete_type(canonicalise_type(sink_data_type, sink, w.compiler))) reference_insert = "&";
		else reference_insert = "";
	
		m_out() << (emit_return_type ? (to_typename + reference_insert) : "") 
			<< " operator()(const " << from_typename << "& __cake_from, " 
			<< to_typename << "*__cake_p_to"
			<< (emit_default_argument ? " = 0" : "")
			<< ") const";
	}
	
	void value_conversion::emit_forward_declaration()
	{
        emit_header(/* false */ optional<string>(), true); // FIXME: looks buggy
		m_out() << "{" << endl;
		m_out().inc_level();
		emit_signature();
		m_out() << ";";
        m_out().dec_level();
        m_out() << "};" << endl;
	}

	void value_conversion::emit_function_name()
	{
		/* m_out().flush();
		emit_header(false, false, false);
		m_out().flush();
		m_out() << "::operator()";
		m_out().flush();*/ 
		m_out() << "value_convert_function<"
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
		m_out() << "void *(*" << (decl_name ? *decl_name : "")
			<< ")(" << from_typename << "* ,"
				<< to_typename << " *)" << endl;
	}
	
	
	vector< pair < shared_ptr<type_die>, shared_ptr<type_die> > >
	value_conversion::get_dependencies()
	{
		return vector< pair < shared_ptr<type_die>, shared_ptr<type_die> > >();
	}
	vector<shared_ptr<value_conversion> >
	value_conversion::dep_is_satisfied(
		link_derivation::val_corresp_iterator begin,
		link_derivation::val_corresp_iterator end,
		const value_conversion::dep& m_dep)
	{
		vector<shared_ptr<value_conversion> > working_concrete;
		vector<shared_ptr<value_conversion> > working_half_exact;
		vector<shared_ptr<value_conversion> > working_exact;
		for (auto i_candidate = begin; i_candidate != end; ++i_candidate)
		{
			auto& candidate = *i_candidate;
			
			cerr << "Is dependency on a conversion from " << m_dep.first->summary()
				<< " to " << m_dep.second->summary()
				<< " satisfied by corresp (at " << candidate.second.get()
				<< ") " << *candidate.second << "? ";

			bool first_dieset_match
			 = (&candidate.second->source_data_type->get_ds() == &m_dep.first->get_ds());
			bool second_dieset_match
			 = (&candidate.second->sink_data_type->get_ds() == &m_dep.second->get_ds());

			bool first_exact = data_types_are_identical(
				candidate.second->source_data_type, 
				m_dep.first
				);
			bool second_exact = data_types_are_identical(
			 	candidate.second->sink_data_type,
				m_dep.second
			);
			bool exact_match = first_exact && second_exact;
			bool first_concrete_match = data_types_are_identical(
				candidate.second->source_data_type->get_concrete_type(), 
				m_dep.first->get_concrete_type()
				);
			bool second_concrete_match = data_types_are_identical(
				candidate.second->sink_data_type->get_concrete_type(),
				m_dep.second->get_concrete_type()
			);
			bool concrete_match = first_concrete_match && second_concrete_match;
			
			if (!first_dieset_match || !second_dieset_match)
			{
				cerr << "no (wrong diesets:";
				if (!first_dieset_match) cerr << " first" ;
				if (!second_dieset_match) cerr << " second";
				cerr << ")" << endl;
				continue;
			}
			if (!concrete_match) 
			{
				cerr << "no (no concrete match:";
				if (!first_concrete_match) cerr << " first[candidate: "
					<< (candidate.second->source_data_type->get_concrete_type()->ident_path_from_root() ? 
					   definite_member_name(*candidate.second->source_data_type->get_concrete_type()->ident_path_from_root())
					   : definite_member_name())
					<< ", dep: "
					<< (m_dep.first->get_concrete_type()->ident_path_from_root() ?
					   definite_member_name(*m_dep.second->get_concrete_type()->ident_path_from_root())
					   : definite_member_name())
					<< "]";
				if (!second_concrete_match) cerr << " second[candidate: "
					<< (candidate.second->sink_data_type->get_concrete_type()->ident_path_from_root() ? 
					   definite_member_name(*candidate.second->sink_data_type->get_concrete_type()->ident_path_from_root())
					   : definite_member_name())
					<< ", dep: "
					<< (m_dep.second->get_concrete_type()->ident_path_from_root() ?
					   definite_member_name(*m_dep.second->get_concrete_type()->ident_path_from_root())
					   : definite_member_name())
					<< "]";
				cerr << ")" << endl;
				continue;
			}
			
			// if we got two half-exacts, should have exact
			assert(!(first_exact && second_exact) || exact_match);
			
			
			cerr << (concrete_match ? (string("yes, " + 
				(exact_match) ? "exactly"
			 :  (first_exact) ? "half-exactly (first)"
			 :  (second_exact) ? "half-exactly (second)"
			 :                   "concrete only")) : "no") << endl;
			 
			if (exact_match) working_exact.push_back(candidate.second);
			else if (first_exact || second_exact) working_half_exact.push_back(candidate.second);
			else if (concrete_match) working_concrete.push_back(candidate.second);
		}
		
		shared_ptr < srk31::concatenating_sequence< vector<shared_ptr<value_conversion> >::iterator > > p_out_seq
		 = std::make_shared< srk31::concatenating_sequence< vector<shared_ptr<value_conversion> >::iterator > >();
		p_out_seq->append(working_exact.begin(), working_exact.end())
		.append(working_half_exact.begin(), working_half_exact.end())
		.append(working_concrete.begin(), working_concrete.end());
		auto retval
		 = vector< shared_ptr<value_conversion> >(p_out_seq->begin(), p_out_seq->end());
		// sanity check
		auto exact_size = srk31::count(working_exact.begin(), working_exact.end());
		auto half_size = srk31::count(working_half_exact.begin(), working_half_exact.end());
		auto concrete_size = srk31::count(working_concrete.begin(), working_concrete.end());
		assert(retval.size() == srk31::count(p_out_seq->begin(), p_out_seq->end()));
		assert(retval.size() == exact_size + half_size + concrete_size);
		return retval;
	}

	void reinterpret_value_conversion::emit_body()
	{
		m_out() << "if (__cake_p_to) *__cake_p_to = *reinterpret_cast<const " 
			<< w.get_type_name(sink_data_type) << "*>(&__cake_from);" << endl
			<< "return *reinterpret_cast<const " 
			<< w.get_type_name(sink_data_type) << "*>(&__cake_from);" << endl;
	}
	
	void virtual_value_conversion::emit_body()
	{
		//m_out() << "assert(false);" << endl;
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
		
		shared_ptr<type_die> found_target_field_type = found_target_field->get_type();
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
		 * or it's a pointer or base type.
		 * For array types, we replace the array type with its ultimate element type. */
		auto value_corresps = owner->w.m_d.val_corresps_for_iface_pair(
			owner->w.m_d.sorted(owner->source, owner->sink));
		
		typedef link_derivation::val_corresp_map_t::value_type ent;
		
		if (found_source_field_type &&  // source field is optional
			found_source_field_type->get_concrete_type()->get_tag() == DW_TAG_array_type)
		{
			found_source_field_type = dynamic_pointer_cast<spec::array_type_die>(found_source_field_type)
				->ultimate_element_type();
		}
		if (found_target_field_type->get_concrete_type()->get_tag() == DW_TAG_array_type)
		{
			found_target_field_type = dynamic_pointer_cast<spec::array_type_die>(found_target_field_type)
				->ultimate_element_type();
		}		
		if (!(
			(std::find_if(
					value_corresps.first, value_corresps.second, 
					[found_target_field_type, found_source_field_type](const ent& arg) { 
						/* cerr << "Testing corresp with source "
							<< *(arg.second->source_data_type)
							<< " and sink " 
							<< *(arg.second->sink_data_type)
							<< endl; */
						return (data_types_are_identical(arg.second->sink_data_type,
									found_target_field_type)
							) && 
							(!found_source_field_type || 
								data_types_are_identical(arg.second->source_data_type,
									found_source_field_type)); 
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
				<< " of type " << *found_target_field_type
				<< ", concrete type " << *found_target_field_type->get_concrete_type();
			if (unique_source_field) cerr << " and source field " << *found_source_field
				<< " of type " << *found_source_field_type
				<< ", concrete type " << *found_source_field_type->get_concrete_type();
			else cerr << " with no unique source";
			cerr << endl;
			
			RAISE(owner->corresp, "corresponding fields lack corresponding type");
		}
	}

	primitive_value_conversion::primitive_value_conversion(wrapper_file& w,
			const basic_value_conversion& basic,
			bool init_only,
			bool& out_init_and_update_are_identical)
		:   value_conversion(w, basic), 
			source_module(w.m_d.module_of_die(source_data_type)),
			target_module(w.m_d.module_of_die(sink_data_type)),
			modules(link_derivation::sorted(make_pair(source_module, target_module)))
		
	{
		// HACK: why do we have an init_only arg?
		assert(init_only == basic.init_only);
		
		// ... unless we find out otherwise
		out_init_and_update_are_identical = true; 
	
	}
	
	structural_value_conversion::structural_value_conversion(wrapper_file& w,
			const basic_value_conversion& basic,
			bool init_only,
			bool& out_init_and_update_are_identical)
		:   value_conversion(w, basic), 
		    primitive_value_conversion(w, basic, init_only, out_init_and_update_are_identical)
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
										source_is_on_left ? rightInfixStub : leftInfixStub,
									/* rule_ast */
										refinementRule
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
									rightInfixStub,
								/* rule_ast */
									refinementRule
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
									leftInfixStub,
								/* rule_ast */
									refinementRule
							}));

					}
					break;
					case (CAKE_TOKEN(LR_DOUBLE_ARROW_Q)):
					{
						if (!source_is_on_left) continue; // not relevant in this direction
						// source is on left, target on right
						out_init_and_update_are_identical = false;
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
									rightInfixStub,
								/* rule_ast */
									refinementRule
							}));
					}
					break;
					case (CAKE_TOKEN(RL_DOUBLE_ARROW_Q)):
					{
						if (source_is_on_left) continue; // not relevant in this direction
						// source is on right, target on left
						out_init_and_update_are_identical = false;
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
									leftInfixStub,
								/* rule_ast */
									refinementRule
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
						<< " in DIEs " << source_as_struct->summary()
						<< " and " << sink_data_type->summary()
						<< endl;
					name_matched_mappings.insert(make_pair(
						*(*i_member)->get_name(),
						(member_mapping_rule_base){
							/* structural_value_conversion *owner, */
								this,
							/* definite_member_name target; */
							// for these, we use the non-cxxified name, so that 
							// ... what? So that we can pull idents out and look them
							// up in the DWARF, e.g. as in get_dependencies
								definite_member_name(1, *(*i_member)->get_name()),
							/* optional<definite_member_name> unique_source_field; */
								definite_member_name(1, *(*i_member)->get_name()),
							/* antlr::tree::Tree *stub; */ 
							// for the stub, we use the cxxified name
								make_definite_member_name_expr(
									definite_member_name(1, w.compiler.cxx_name_from_die(
									/* we have to use the source/sink DIE that is appropriate
									 * -- which is always the source value! i.e. the stub */
										dynamic_pointer_cast<structure_type_die>(
											source_concrete_type
										)->named_child(*(*i_member)->get_name())
									))),
							/* module_ptr pre_context; */ 
								source_module,
							/* antlr::tree::Tree *pre_stub; */ 
								0,
							/* module_ptr post_context; */ 
								target_module,
							/* antlr::tree::Tree *post_stub; */ 
								0,
							/* rule_ast */
								0
						}));
				}
				else
				{
					cerr << "Didn't match name " << *(*i_member)->get_name()
						<< " from DIE " << sink_data_type->summary()
						<< " in DIE " << source_as_struct->summary()
						<< endl;
				}
			}
		}
	}

	vector< value_conversion::dep > structural_value_conversion::get_dependencies()
	{
		std::set<dep> working;
	
		auto maybe_insert_dependency = [&working](
			const dep& the_dep, 
			const member_mapping_rule& rule) {
			/* We discard dependencies that are pointers. 
			 * We turn array dependencies into their ultimate element type. */
			
			dep to_add = the_dep;
			
			auto first_concrete = the_dep.first->get_concrete_type();
			auto second_concrete = the_dep.second->get_concrete_type();
			if (!first_concrete || !second_concrete)
			{
				cerr << "Warning: discarding dependency involving void type " 
					<< (!first_concrete ? the_dep.first->summary() : the_dep.second->summary())
					<< endl;
				return;
			}
			
			if (first_concrete->get_tag() == DW_TAG_pointer_type
			|| second_concrete->get_tag() == DW_TAG_pointer_type)
			{
				if (!(first_concrete->get_tag() == DW_TAG_pointer_type
				&& second_concrete->get_tag() == DW_TAG_pointer_type))
				{
					RAISE(rule.rule_ast, "requires non-pointer-to-pointer correspondence");
				}
				
				// now we know that they're both pointers
				// just skip it! The user has to ensure the right corresps are defined
				return;
			}
			
			if (first_concrete->get_tag() == DW_TAG_array_type)
			{
				to_add.first = dynamic_pointer_cast<spec::array_type_die>(first_concrete)
					->ultimate_element_type();
				assert(to_add.first);
			}
			if (second_concrete->get_tag() == DW_TAG_array_type)
			{
				to_add.second = dynamic_pointer_cast<spec::array_type_die>(second_concrete)
					->ultimate_element_type();
				assert(to_add.second);
			}
			
			working.insert(to_add);
		};
	
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
			maybe_insert_dependency(pair, i_mapping->second);
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
				
				maybe_insert_dependency(pair, *i_mapping->second);
				
				// auto retval = working.insert(pair);
				// assert(retval.second); // assert that we actually inserted something
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

		m_out() << dest_type_string << " __cake_nonconst_from = "
			"const_cast< "
				<< dest_type_string <<
			" >(__cake_from);" << endl;
	
	
	}
	
	void primitive_value_conversion::emit_target_buffer_declaration()
	{
		/* Create or find buffer...
		 * if we have an incomplete type, don't create the buffer! */
		m_out() << w.get_type_name(sink_data_type);
		if (!w.compiler.cxx_is_complete_type(canonicalise_type(sink_data_type, sink, w.compiler))) m_out() << " /* incomplete, so no __cake_tmp, */";
		else m_out() << " __cake_tmp, ";
	
		m_out() << " *__cake_p_buf;" << endl
		<< "if (__cake_p_to) __cake_p_buf = __cake_p_to; else ";
		if (!w.compiler.cxx_is_complete_type(canonicalise_type(sink_data_type, sink, w.compiler))) m_out() << "assert(false);";
		else m_out() << " __cake_p_buf = &__cake_tmp;" << endl;
	}

	// override for the virtual corresp case
	void virtual_value_conversion::emit_target_buffer_declaration()
	{
		/* Create or find buffer */
		// if we are user-allocated, we just declare a pointer
		if (treat_target_type_as_user_allocated())
		{
			m_out() << w.get_type_name(sink_data_type) << " *__cake_p_buf = __cake_p_to;" << endl
				<< "// assert (__cake_p_to); /* can be null to begin; sink stub will provide value... */" << endl;
		}
		else
		{
			m_out() << w.get_type_name(sink_data_type) << " *__cake_p_buf = __cake_p_to;" << endl
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
			(source_infix_stub && GET_CHILD_COUNT(source_infix_stub)) ? GET_CHILD(source_infix_stub, 0) : 0, // pass over INFIX_STUB_EXPR
			(sink_infix_stub && GET_CHILD_COUNT(sink_infix_stub)) ? GET_CHILD(sink_infix_stub, 0) : 0,
			true /* force writing to void target */);
			
		// output return statement
		m_out() << "return *__cake_p_buf;" << endl;
	}
	
	void 
	primitive_value_conversion::write_single_field(
		wrapper_file::context& ref_ctxt,
		string target_field_selector,
		optional<string> unique_source_field_selector,
		antlr::tree::Tree *source_expr,
		antlr::tree::Tree *source_infix,
		antlr::tree::Tree *sink_infix,
		bool write_void_target
		)
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
					false,
				}));
			extra_env.insert(make_pair("__cake_here",
				(wrapper_file::bound_var_info) {
					"&__cake_nonconst_from" + (is_void_source ? "" : *unique_source_field_selector), // cxx name
					"&__cake_nonconst_from" + (is_void_source ? "" : *unique_source_field_selector), // typeof
					source_module,
					false,
					bound_var_info::IS_A_POINTER
				}));
		}
		// we always have a "there"
		extra_env.insert(make_pair("__cake_there",
			(wrapper_file::bound_var_info) { // v-- works okay for empty selector (void target) too
				"&((*__cake_p_buf)" + target_field_selector + ")", // cxx name
				"&((*__cake_p_buf)" + target_field_selector + ")", // typeof
				source_module,
				false,
				bound_var_info::IS_A_POINTER
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
		if (source_infix && 
			(GET_TYPE(source_infix) != CAKE_TOKEN(INFIX_STUB_EXPR)
			|| GET_CHILD_COUNT(source_infix) > 0))
		{
			if (GET_TYPE(source_infix) == CAKE_TOKEN(INFIX_STUB_EXPR))
			{
				source_infix = GET_CHILD(source_infix, 0);
			}
			auto status2 = w.emit_stub_expression_as_statement_list(ctxt,
				source_infix);
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
		m_out() << "// source->sink crossover point" << endl;
		auto crossed_env = w.crossover_environment_and_sync(
			source_module, basic_env, target_module, 
			/* no constraints */ 
			multimap< string, pair< antlr::tree::Tree *, shared_ptr<type_die> > >(), false, true);

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
					false,
					bound_var_info::IS_A_POINTER
				}));
		} 

		// we can always add "here"
		ctxt.env.insert(make_pair("__cake_here",
			(wrapper_file::bound_var_info) {
				"&((*__cake_p_buf)" + target_field_selector + ")", // cxx name
				"&((*__cake_p_buf)" + target_field_selector + ")", // typeof
				target_module,
				false,
				bound_var_info::IS_A_POINTER
			}));
		// we should have "it" in the environment, just from crossover
		assert(ctxt.env.find("__cake_it") != ctxt.env.end());

		/* Now emit the post-stub. */
		//auto new_env3 = ctxt.env; // provisional value
		if (sink_infix && 
			(GET_TYPE(sink_infix) != CAKE_TOKEN(INFIX_STUB_EXPR)
			|| GET_CHILD_COUNT(sink_infix) > 0))
		{
			// HACK: how did this AST change happen?
			if (GET_TYPE(sink_infix) == CAKE_TOKEN(INFIX_STUB_EXPR))
			{
				sink_infix = GET_CHILD(sink_infix, 0);
			}
			auto status3 = w.emit_stub_expression_as_statement_list(ctxt,
				sink_infix);
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
		 * or else the converted field value.
		 *
		 * If we're assigning to an array type, we have to wrap it so that assignment
		 * will work. So, we use a template that makes this decision for us. */
		if (!is_void_target || write_void_target)
		{
			assert(ctxt.env["__cake_it"].cxx_name != "");
			m_out() << "::cake::assignable< __typeof( (*__cake_p_buf)" << target_field_selector << ") >()("
				                                     "(*__cake_p_buf)" << target_field_selector << ")" 
													 << " = " 
				<< " ::cake::default_cast < __typeof( " 
					<< "(*__cake_p_buf)" << target_field_selector << "), "
					<< " __typeof(" << ctxt.env["__cake_it"].cxx_name << ") >()( "
				<< ctxt.env["__cake_it"].cxx_name 
				<< ")"
				<< ";" << endl;
		}
	}
	
	void 
	primitive_value_conversion::emit_pre_stub_co_object_substitution_check(
		codegen_context& ctxt)
	{
		/* Under these conditions: 
		 * - we are an init-only rule
		 * - our stub has yielded a pointer
		 * - our target object is in the co-object relation
		 * - (that's "__cake_there" for a pre-stub, "here" for a post-stub)
		 * ... we take the pointer it returns and make it our co-object. */
		assert(init_only);
		auto found_it = ctxt.env.find("__cake_it");
		assert(found_it != ctxt.env.end());
		string it_cxxname = found_it->second.cxx_name;
		auto found_there = ctxt.env.find("__cake_there");
		assert(found_there != ctxt.env.end());
		string there_cxxname = found_there->second.cxx_name;
		m_out() << "if (boost::is_pointer<__typeof(" << it_cxxname << " )>::value)" << endl
			<< "{" << endl;
		m_out().inc_level();
			/* Note: we're replacing the __p_to co-object with what the stub returned. */
			m_out() << "replace_co_object(" 
				<< there_cxxname << ", "
				<< "*reinterpret_cast<void**>(&" << it_cxxname << ")" << ", "
				<< "REP_ID(" << w.ns_prefix << "::" << w.m_d.name_of_module(
					ctxt.modules.current == ctxt.modules.source 
					? ctxt.modules.sink : ctxt.modules.source) << "), "
				<< "REP_ID(" << w.ns_prefix << "::"
					<< w.m_d.name_of_module(ctxt.modules.current == ctxt.modules.source 
						? ctxt.modules.source : ctxt.modules.sink)
				<< "));" << endl;
		m_out().dec_level();
		m_out() << "}" << endl;
	}
	void 
	primitive_value_conversion::emit_post_stub_co_object_substitution_check(
		codegen_context& ctxt)
	{
		assert(init_only);
		auto found_it = ctxt.env.find("__cake_it");
		assert(found_it != ctxt.env.end());
		string it_cxxname = found_it->second.cxx_name;
		auto found_here = ctxt.env.find("__cake_here");
		assert(found_here != ctxt.env.end());
		string here_cxxname = found_here->second.cxx_name;
		m_out() << "if (boost::is_pointer<__typeof(" << it_cxxname << " )>::value)" << endl
			<< "{" << endl;
		m_out().inc_level();
			/* Note: we're replacing the __p_to co-object with what the stub returned. */
			m_out() << "replace_co_object(" 
				<< here_cxxname << ", "
				<< "*reinterpret_cast<void**>(&" << it_cxxname << ")" << ", "
				<< "REP_ID(" << w.ns_prefix << "::" << w.m_d.name_of_module(
					ctxt.modules.current == ctxt.modules.source 
					? ctxt.modules.source : ctxt.modules.sink) << "), "
				<< "REP_ID(" << w.ns_prefix << "::"
					<< w.m_d.name_of_module(ctxt.modules.current == ctxt.modules.source 
						? ctxt.modules.sink : ctxt.modules.source)
				<< "));" << endl;
		m_out().dec_level();
		m_out() << "}" << endl;
	
	}
	
	bool
	structural_value_conversion::should_crossover_source_field(
			shared_ptr<member_die> p_field
	)
	{
		assert(p_field->get_type());
		return source_type_has_correspondence(p_field->get_type()->get_concrete_type());
	}
	bool
	virtual_value_conversion::should_crossover_source_field(
			shared_ptr<member_die> p_field
		)
	{
		assert(p_field->get_name());
		string field_name = *p_field->get_name();
		shared_ptr<type_die> field_type = p_field->get_type();
		// we allow references too, because we're virtual
		return this->structural_value_conversion::should_crossover_source_field(p_field)
			|| (field_type->get_concrete_type()->get_tag() == DW_TAG_reference_type
				&& source_type_has_correspondence(
					dynamic_pointer_cast<spec::reference_type_die>(field_type->get_concrete_type())
							->get_type()));
	}
	
	bool structural_value_conversion::source_type_has_correspondence(
		shared_ptr<type_die> t
	)
	{
		auto ifaces = link_derivation::sorted(sink, source);
		
		// all void types have a correspondence, trivially
		if (!t) return true;
		
		// all pointer types have a correspondence, perhaps trivially
		if (t->get_concrete_type()->get_tag() == DW_TAG_pointer_type) return true;
		
		// array types have a corresondence if their ultimate element type does
		if (t->get_concrete_type()->get_tag() == DW_TAG_array_type
				&& source_type_has_correspondence(
					dynamic_pointer_cast<spec::array_type_die>(t->get_concrete_type())
						->ultimate_element_type())) return true;
		
		// otherwise, we have to look 'em up
		return this->w.m_d.val_corresp_supergroups[ifaces].find(
			make_pair(this->source,
				canonicalise_type(t, this->source, w.compiler)))
				!= w.m_d.val_corresp_supergroups[ifaces].end();
	}
	
	string
	structural_value_conversion::flatten_selector_dmn(const definite_member_name& dmn)
	{
		ostringstream s;
		for (auto i_name_part = dmn.begin();
			i_name_part != dmn.end();
			++i_name_part)
		{
			//if (i_name_part != i_name_matched->second.target.begin())
			{ s << "."; } // always begin selector with '.'!
			s << *i_name_part;
		}
		return s.str();
	}
	
	definite_member_name
	structural_value_conversion::cxxify_selector_dmn(
		const definite_member_name& dmn,
		shared_ptr<spec::with_data_members_die> start_type
	)
	{
		definite_member_name cxxified_selector;
		shared_ptr<with_data_members_die> cur_dwarf_type = start_type;
		for (auto i_el = dmn.begin(); i_el != dmn.end(); ++i_el)
		{
			if (!cur_dwarf_type)
			{
				cerr << "Error refers to selector: " << dmn << endl;
				 RAISE(this->corresp,
					"selector is not a path through subobject tree");
			}
			auto cur_memb = dynamic_pointer_cast<member_die>(
				cur_dwarf_type->named_child(*i_el)
			);
			cxxified_selector.push_back(
				w.compiler.cxx_name_from_die(
					cur_memb
				)
			);
			/* The last element in the selector need not denote a structured thing, 
			 * so we don't raise an error if the cast fails, until the next iteration (if any). */
			auto tmp_dwarf_type = cur_memb->get_type();
			auto tmp_with_members_type = dynamic_pointer_cast<with_data_members_die>(tmp_dwarf_type);
			if (tmp_with_members_type) cur_dwarf_type = tmp_with_members_type;
			else cur_dwarf_type = shared_ptr<with_data_members_die>();
		}
		return cxxified_selector;
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
		vector<string> target_field_selectors;
		/* Subtlety: in what order do we emit the logic implied by each field 
		 * corresp? We want to give the user some control. HMM. */
		for (auto i_name_matched = name_matched_mappings.begin();
				i_name_matched != name_matched_mappings.end();
				++i_name_matched)
		{
			string target_field_selector = flatten_selector_dmn(
				cxxify_selector_dmn(
					i_name_matched->second.target,
					dynamic_pointer_cast<with_data_members_die>(sink_data_type->get_concrete_type())
				)
			);
			
			assert(
				target_fields_to_write.find(target_field_selector)
				 == target_fields_to_write.end()
			); // i.e. that this key is not already present
			
			assert(target_field_selector != "");
			target_fields_to_write.insert(make_pair(target_field_selector, &i_name_matched->second));
			target_field_selectors.push_back(target_field_selector);
		}
		
		/* We MUST emit the explicit toplevel mappings in program order. So sort them now. */
		vector< pair< string, explicit_toplevel_mappings_t::mapped_type > > explicit_toplevel_mappings_vec;
		cerr << "Vector has " << explicit_toplevel_mappings_vec.size() << " elements." << endl;
		std::copy(explicit_toplevel_mappings.begin(), explicit_toplevel_mappings.end(),
			std::back_inserter(explicit_toplevel_mappings_vec));
		std::sort(explicit_toplevel_mappings_vec.begin(), explicit_toplevel_mappings_vec.end(),
			[](const explicit_toplevel_mappings_t::value_type& arg1, 
			   const explicit_toplevel_mappings_t::value_type& arg2)
			{
				// order by program order
				cerr << "Comparing line number " << arg1.second->rule_ast->getLine(arg1.second->rule_ast)
				 << " with " << arg2.second->rule_ast->getLine(arg2.second->rule_ast) << endl;
				return arg1.second->rule_ast->getLine(arg1.second->rule_ast)
				     < arg2.second->rule_ast->getLine(arg2.second->rule_ast);
			});
		cerr << "Vector has " << explicit_toplevel_mappings_vec.size() << " elements." << endl;
		for (auto i_explicit_toplevel = explicit_toplevel_mappings_vec.begin();
				i_explicit_toplevel != explicit_toplevel_mappings_vec.end();
				++i_explicit_toplevel)
		{
			// assert uniqueness in the multimap for now
			auto equal_range = explicit_toplevel_mappings.equal_range(i_explicit_toplevel->first);
			auto copied_first = equal_range.first;
			assert(++copied_first == equal_range.second); // means "size == 1"
			
			auto selector = i_explicit_toplevel->first == "" ? 
						""
						: "." + w.compiler.cxx_name_from_die(
								dynamic_pointer_cast<with_data_members_die>(
									sink_data_type->get_concrete_type()
								)->named_child(i_explicit_toplevel->first)
							);
			target_fields_to_write.insert(
				make_pair(
					selector, 
					i_explicit_toplevel->second
				)
			);
			target_field_selectors.push_back(selector);
		}
		
		/* We build the set of idents used in source and sink stubs, so that we can build
		 * a more minimal source-side environment. This is a bit of a HACK -- we
		 * should really do a precise analysis for each rule.*/
		set<string> source_side_idents_referenced;
		vector<antlr::tree::Tree *> source_side_ident_contexts;
		auto grab_idents = [&source_side_idents_referenced](antlr::tree::Tree *t) {
			if (GET_TYPE(t) == CAKE_TOKEN(IDENT))
			{
				source_side_idents_referenced.insert(
					unescape_ident(CCP(GET_TEXT(t)))
				);
				return true;
			}
			return false; 
		};
		for (auto i_explicit_toplevel = explicit_toplevel_mappings_vec.begin();
				i_explicit_toplevel != explicit_toplevel_mappings_vec.end();
				++i_explicit_toplevel)
		{
			// the ident-grabber function
			walk_ast_depthfirst(
				i_explicit_toplevel->second->stub, 
				source_side_ident_contexts, 
				grab_idents
			);
			walk_ast_depthfirst(
				i_explicit_toplevel->second->pre_stub, 
				source_side_ident_contexts, 
				grab_idents
			);
			walk_ast_depthfirst(
				i_explicit_toplevel->second->post_stub, 
				source_side_ident_contexts, 
				grab_idents
			);
		}
		// Now do name-matched
		for (auto i_name_matched = name_matched_mappings.begin();
			i_name_matched != name_matched_mappings.end();
			++i_name_matched) source_side_idents_referenced.insert(i_name_matched->first);
		// Now do overall stubs
		walk_ast_depthfirst(
			this->source_infix_stub, 
			source_side_ident_contexts, 
			grab_idents
		);
		walk_ast_depthfirst(
			this->sink_infix_stub, 
			source_side_ident_contexts, 
			grab_idents
		);
		
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
			string field_name = w.compiler.cxx_name_from_die(*i_field);
			shared_ptr<type_die> field_type = (*i_field)->get_type();
			
			/* We can skip fields that are not referenced anywhere. 
			 * EXCEPT if we are dealing with a field that has been renamed 
			 * -- then its reference will not look like its name. */
			if (*(*i_field)->get_name() == field_name &&
				source_side_idents_referenced.find(field_name)
				 == source_side_idents_referenced.end()) continue;
			
			/* We only crossover if the field's type has a corresponding type
			 * on the other side (or is a pointer, or array whose ultimate element type
			 * has a corresponding type). */
			auto ifaces
			 = link_derivation::sorted(sink, source);
			bool should_crossover = should_crossover_source_field(*i_field);

			cerr << "adding name " << field_name << endl;
			basic_env.insert(make_pair(field_name,
				(wrapper_file::bound_var_info) {
					string("__cake_nonconst_from.") + field_name, // cxx name
					"__cake_nonconst_from." + field_name, // typeof
					source_module,
					!should_crossover // was true / "do not crossover!" -- WHY NOT? "once only"/balanced semantics?
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
					false,
					bound_var_info::IS_A_POINTER
				}));
			extra_env.insert(make_pair("__cake_there",
				(wrapper_file::bound_var_info) {
					"__cake_p_buf", // cxx name
					"__cake_p_buf", // typeof
					source_module,
					false,
					bound_var_info::IS_A_POINTER
				}));
			auto saved_env = ctxt.env;
			ctxt.env = extra_env;
			auto return_status = w.emit_stub_expression_as_statement_list(
				ctxt, 
				GET_CHILD(source_infix_stub, 0)
			);
			// add "__cake_it"
			ctxt.env["__cake_it"] = (wrapper_file::bound_var_info) {
				return_status.result_fragment, // cxx name
				return_status.result_fragment, // typeof
				source_module,
				false
			};
			if (init_only && /* we only replace in the pre-stub if there is no post-stub */
				(!sink_infix_stub || GET_CHILD_COUNT(sink_infix_stub) == 0)
			) emit_pre_stub_co_object_substitution_check(ctxt);
			// restore previous environment + new additions
			ctxt.env = w.merge_environment(saved_env, return_status.new_bindings);
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

				m_out() << "{ // begin expression assigned to field " 
					<< (target_field_selector == "" ? "(no field)" : target_field_selector);
				m_out().inc_level();

				/* If we *do* have a unique source field, then a bunch of other Cake idents
				 * are defined: "this", "here" and "there" (but not "that" -- I think). */
				optional<string> unique_source_field_selector;
				if (i_target->second->unique_source_field)
				{
					unique_source_field_selector
					 = flatten_selector_dmn(
					 	cxxify_selector_dmn(
							*i_target->second->unique_source_field,
							dynamic_pointer_cast<with_data_members_die>(source_data_type->get_concrete_type())
						)
					);
				}

				assert(target_field_selector == "" || *target_field_selector.begin() == '.');
				assert(!unique_source_field_selector
					|| *unique_source_field_selector == ""
					|| *unique_source_field_selector->begin() == '.');

				/* What's wrong with this? */
				write_single_field(ctxt, target_field_selector, unique_source_field_selector,
					i_target->second->stub, i_target->second->pre_stub, i_target->second->post_stub);

				m_out().dec_level();
				m_out() << "}" << endl;
			}
		}
		/* Emit overall sink-side stub. */
		if (sink_infix_stub && GET_CHILD_COUNT(sink_infix_stub) > 0)
		{
			// make sure we are in the sink module context now
			ctxt.modules.current = target_module;
			// we cross over this env...
			// there may or may not be stuff in there (e.g. empty struct)
			//assert(basic_env.size() > 0);
			auto crossed_env = w.crossover_environment_and_sync(
				source_module, ctxt.env, target_module, 
				/* no constraints */ 
				std::multimap< string, pair< antlr::tree::Tree *, shared_ptr<type_die> > >(), false, true);
		
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
					false,
					bound_var_info::IS_A_POINTER
				}));
			crossed_env.insert(make_pair("__cake_there",
				(wrapper_file::bound_var_info) {
					"(&__cake_nonconst_from)", // cxx name
					"(&__cake_nonconst_from)", // typeof
					target_module,
					false,
					bound_var_info::IS_A_POINTER
				}));
			auto saved_env = ctxt.env;
			ctxt.env = crossed_env;
			
			auto return_status = w.emit_stub_expression_as_statement_list(
				ctxt, 
				GET_CHILD(sink_infix_stub, 0)
			);
			
			// finally, add __cake_it
			ctxt.env["__cake_it"] = (wrapper_file::bound_var_info) {
				return_status.result_fragment,
				return_status.result_fragment,
				target_module,
				false
			};
			
			post_sink_stub_hook(ctxt.env, return_status);
			if (init_only) emit_post_stub_co_object_substitution_check(ctxt);			
			ctxt.env = saved_env;
		}

		// output return statement
		m_out() << "return *__cake_p_buf;" << endl;
	}
	
	void 
	virtual_value_conversion::post_sink_stub_hook(
		const environment& env, 
		const post_emit_status& return_status
	)
	{
		if (treat_target_type_as_user_allocated())
		{
			m_out() << "__cake_p_buf = __cake_p_to = " << return_status.result_fragment
				<< ";" << endl;
		}
	}

}
