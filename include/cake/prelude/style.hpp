/* #include <iostream> */

namespace cake
{
	template <typename Arg1, typename Arg2, int StyleTag = 0>
	struct equal
	{
		bool operator()(const Arg1& arg1, const Arg2& arg2) const
		{
			/* std::cerr << "Tested whether " << arg1 << " equals " << arg2 << std::endl; */
			return arg1 == arg2;
		}
	};
	template <>
	struct equal<const char *, const char *, 0>
	{
		bool operator()(const char *const& arg1, const char *const& arg2) const
		{
			// could use strcmp, but I want to keep things self-contained for now
			const char *pos1 = arg1;
			const char *pos2 = arg2;
			while (*pos1 != '\0' && *pos2 != '\0' && *pos1 == *pos2)
			{
				++pos1; ++pos2;
			}
			if (*pos1 != *pos2) return false; else return true;
		}
	};
	template <>
	struct equal<unspecified_wordsize_type, unspecified_wordsize_type, 0>
	{
		bool operator()(const unspecified_wordsize_type& arg1, 
			const unspecified_wordsize_type& arg2) const
		{
			// FIXME: can make this a compile-time fail?
			throw "Insufficient type information to test equality.";
			return false;
		}		
	};
	template <typename Arg1>
	struct equal<Arg1, unspecified_wordsize_type, 0>
	{
		bool operator()(const Arg1& arg1, const unspecified_wordsize_type arg2) const
		{
			return equal<Arg1, Arg1, 0>()(arg1, static_cast<Arg1>(reinterpret_cast<int>(arg2)));
		}
	};
	template <typename Arg1>
	struct equal<Arg1*, unspecified_wordsize_type, 0>
	{
		bool operator()(Arg1 *const & arg1, const unspecified_wordsize_type arg2) const
		{
			return equal<Arg1*, Arg1*, 0>()(arg1, reinterpret_cast<Arg1*>(arg2));
		}
	};	
	template <typename Arg1>
	struct equal<Arg1*, wordsize_integer_type, 0>
	{
		bool operator()(Arg1 *const & arg1, const wordsize_integer_type arg2) const
		{
			return equal<Arg1*, Arg1*, 0>()(arg1, reinterpret_cast<Arg1*>(arg2));
		}
	};	
	template <typename Arg2>
	struct equal<unspecified_wordsize_type, Arg2, 0>
	{
		bool operator()(const unspecified_wordsize_type arg1, const Arg2& arg2) const
		{
			return equal<Arg2, Arg2, 0>()(
				static_cast<Arg2>(*reinterpret_cast<const wordsize_integer_type*>(&arg1)), 
				arg2);
		}
	};
	template <typename Arg2>
	struct equal<unspecified_wordsize_type, Arg2*, 0>
	{
		bool operator()(const unspecified_wordsize_type arg1, Arg2 *const& arg2) const
		{
			return equal<Arg2*, Arg2*, 0>()(*reinterpret_cast<const Arg2* const*>(&arg1), arg2);
		}
	};
	template <typename Arg2>
	struct equal<wordsize_integer_type, Arg2*, 0>
	{
		bool operator()(const wordsize_integer_type arg1, Arg2 *const& arg2) const
		{
			return equal<Arg2*, Arg2*, 0>()(reinterpret_cast<Arg2*>(arg1), arg2);
		}
	};

	template <typename RetValType, int RuleTag = 0>
	struct retval_of
	{
		RetValType operator()(const RetValType& arg) const 
		{ return arg; }
	};

	template <typename RetValType, int RuleTag = 0>
	struct failure
	{
		RetValType operator()() const 
		{ return -1; }
	};

	template <typename RetValType, int RuleTag = 0>
	struct success
	{
		RetValType operator()() const 
		{ return 0; }
	};
	template <typename RetValPtrTargetType, int RuleTag /* = 0 */>
	struct success<RetValPtrTargetType*, RuleTag>
	{
		RetValPtrTargetType* operator()() const 
		{ return reinterpret_cast<RetValPtrTargetType*>(this); }
	};


	// now partially specialise for pointers
	template <typename RetValType, int RuleTag>
	struct failure<RetValType*, RuleTag>
	{
		RetValType* operator()() const 
		{ return 0; }
	};

	template <typename RetValType, int RuleTag = 0>
	struct success_of
	{
		bool operator()(const RetValType& arg) const
		{ return true; }
	};
	// overrides for wordsize integer-valued types
	template <> struct success_of<unsigned long, 0> 
	{ bool operator()(unsigned long arg) const { return (arg == 0); } };
	template <> struct success_of<signed long, 0> 
	{ bool operator()(signed long arg) const { return (arg == 0); } };
	// FIXME: come up with an architecture-independent way of describing
	// specialisations for other sizes of integer

	// specialisation for pointer types
	template <typename Arg> struct success_of<Arg*, 0> 
	{ bool operator()(const Arg *const& arg) const { return (arg != 0); } };

	// function template conveniences for the above
	//static
	template <typename RetValType, int StyleTag = 0> 
	RetValType 
	__cake_value_of(const RetValType& arg)
	{ return retval_of<RetValType, StyleTag>().operator()(arg); }

	//static
	template <typename Arg, int StyleTag = 0> 
	bool 
	__cake_success_of(const Arg& arg)
	{ return success_of<Arg, StyleTag>().operator()(arg); }


	struct style_base
	{

	};

	template <int StyleTag> 
	struct style_traits {}; 
	
	template <>
	struct style_traits<0> : style_base
	{
		// define the style's DWARF types for each kind of literal in the Cake language
		typedef const char *STRING_LIT;
		typedef long double CONST_ARITH;
		
		template <typename LiteralType>
		LiteralType construct_literal(LiteralType arg)
		{
			return arg;
		}
		
		template <typename Arg>
		static STRING_LIT string_lit(Arg arg) { return arg; }
		template <typename Arg>
		static int int_lit(Arg arg) { return arg; }
		template <typename Arg>
		static long double float_lit(Arg arg) { return arg; }
		//template <typename Arg>
		//void void_value(Arg arg) {}
		template <typename Arg>
		static void *null_value(Arg arg) { return 0; }
		template <typename Arg>
		static int true_value(Arg arg) { return 1; }
		template <typename Arg>
		static int false_value(Arg arg) { return 0; }
	
	};
}

/* Toplevel definitions MUST use the prefix __cake_ */
// FIXME: why do we need these (at toplevel, or anywhere)?
template <typename Arg, int StyleTag = 0>
bool __cake_success_of(const Arg& arg)
{
	return cake::success_of<Arg, StyleTag>()(arg);
}

template <typename Arg, int StyleTag = 0>
Arg __cake_value_of(const Arg& arg)
{
	return cake::retval_of<Arg, StyleTag>()(arg);
}

template <typename Arg, int StyleTag = 0>
Arg __cake_failure_with_type_of(const Arg& arg)
{
	return cake::failure<Arg, StyleTag>()();
}

template <typename Arg, int StyleTag = 0>
Arg __cake_success_with_type_of(const Arg& arg)
{
	return cake::success<Arg, StyleTag>()();
}

