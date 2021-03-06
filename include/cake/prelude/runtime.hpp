#include "common.hpp"
#include "repman.h"

//#include <cstdarg>
#include <initializer_list>
#include <vector>
#include <map>
#include <string>
#include <iostream>

namespace cake
{
	struct conv_table_key
	{
		std::vector<std::string> first;
		std::vector<std::string> second;
		bool from_first_to_second;
		int rule_tag;
		bool operator<(const conv_table_key& arg) const
		{
			return first < arg.first
			|| (first == arg.first && second < arg.second)
			|| (first == arg.first && second == arg.second && from_first_to_second < arg.from_first_to_second)
			|| (first == arg.first && second == arg.second && from_first_to_second == arg.from_first_to_second && rule_tag < arg.rule_tag);
		}
	};
	struct conv_table_value
	{
		size_t to_size;
		/* void *func; // */ conv_func_t func;
	};
	typedef std::map<conv_table_key, conv_table_value> conv_table_t;
	
	struct init_table_key
	{
		std::vector<std::string> first;
		bool from_first_to_second;
		bool operator<(const init_table_key& arg) const
		{
			return first < arg.first
			|| (first == arg.first && from_first_to_second < arg.from_first_to_second);
		}
	};
	struct init_table_value
	{
		size_t to_size;
		std::vector<std::string> to_typename;
		conv_func_t func;
	};
	typedef std::map<init_table_key, init_table_value> init_table_t;
	
	/* How to do canonicalisation: 
	 * lazily
	 * the "canonical" type is the first one found 
	 * by depth-first search through the containing file's DIEs? 
	 * then cache these?
	 * That will introduce big delays in some cases. Can do better? 
	 * We have a mapping from CU DIE to component name. */

	template <typename T>
	struct co_objects_allocator
	{
		/* When we call ensure_co_objects on a non-pointer, we do nothing. */
		inline void operator()(int source_rep_id, const T& obj, void *stackptr_helper, 
			int target_rep_id, bool object_is_leaf) const
		{ return; }
	};
	
	template <typename T>
	struct co_objects_allocator<T*>
	{
		/* When we call ensure_co_objects on a pointer, we walk it! */
		typedef T *arg_type;
		inline void operator()(int source_rep_id, const arg_type& obj, 
			void *stackptr_helper, int target_rep_id,
			bool object_is_leaf) const
		{
			if (!components_table_inited) init_components_table();
			if (!component_pairs_table_inited) init_component_pairs_table();
			
// 			walk_bfs(
// 				source_rep_id,
// 				obj,
// 				stackptr_helper,
// 				target_rep_id,
// 				allocate_co_object_idem,
// 				source_rep_id,
// 				target_rep_id,
// 				object_is_leaf
// 			);
			// ALWAYS_OVERRIDE
			// let's just call allocate_co_object_idem once
			allocate_co_object_idem(
				obj,
				source_rep_id,
				target_rep_id,
				object_is_leaf
			);
		}
	};
	
	template <typename T>
	void ensure_co_objects_allocated(int source_rep_id, const T& obj, void *stackptr_helper, 
		int target_rep_id,
		bool object_is_leaf)
	{
		co_objects_allocator<T>()(source_rep_id, obj, stackptr_helper, target_rep_id, object_is_leaf);
	}
	
	template <typename T>
	struct pointer_ensurer
	{
		inline void *operator()(const T& obj) const { assert(false); }
	};
	template <typename T>
	struct pointer_ensurer<T*>
	{
		typedef T *arg_type;
		inline void *operator()(const arg_type& obj) const 
		{ return const_cast<void *>(static_cast<const void *>(obj)); }
	};	
	template <>
	struct pointer_ensurer<wordsize_integer_type>
	{
		inline void *operator()(wordsize_integer_type obj) const 
		{ return *reinterpret_cast<void**>(&obj); }
	};	
	template <>
	struct pointer_ensurer<unspecified_wordsize_type>
	{
		inline void *operator()(unspecified_wordsize_type obj) const 
		{ return *reinterpret_cast<void**>(&obj); }
	};	
	template <typename T>
	void *ensure_is_a_pointer(const T& obj)
	{
		return pointer_ensurer<T>()(obj);
	}

	struct co_obj_replacement_notifier
	{
		std::vector<void **> objs;
		co_obj_replacement_notifier(std::initializer_list<void **> arg)
		{
// 			va_list args;
// 			va_start(args, arg);
// 			void *obj;
// 			do
// 			{
// 				obj = va_arg(args, __typeof(obj));
// 				if (obj) objs.push_back(&obj);
// 			} while (obj);
			for (auto i = arg.begin(); i != arg.end(); ++i)
			{
				if (*i && **i) objs.push_back(*i);
			}
			
		}
		
		void notify(void *old_addr, void *new_addr)
		{
			std::cerr << "Notifier at " << this << " notified of old address " << old_addr
				 << " and new address " << new_addr << std::endl;
			for (auto i = objs.begin(); i != objs.end(); ++i)
			{
				if (**i == old_addr) **i = new_addr;
			}
		}
	};
	
	inline addr_change_cb_t make_replacer_cb(co_obj_replacement_notifier& notifier)
	{
		auto member_fun_ptr = &co_obj_replacement_notifier::notify;
		addr_change_cb_t retval
		 = *reinterpret_cast<void(**)(void*, void*, void*)>(&member_fun_ptr);
		std::cerr << "Snarfed function pointer " << retval << std::endl;
		return retval;
	}
}
