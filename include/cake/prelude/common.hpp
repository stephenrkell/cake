#ifndef CAKE_COMMON_HPP_
#define CAKE_COMMON_HPP_

//#include <iostream>
#include <cassert> // is this okay?
#include <utility>
#include <srk31/array.hpp>
#include <boost/optional.hpp>
#include <boost/type_traits/remove_const.hpp>
//#include <boost/type_traits/remove_volatile.hpp>
#include <boost/type_traits/remove_reference.hpp>
#include <boost/type_traits/remove_pointer.hpp>
#include <boost/type_traits/is_pointer.hpp>
#define REP_ID(ident) ident::marker::rep_id
// don't bother with volatile for now
#define REMOVE_CV(t_id) boost::remove_const< t_id >::type
#define REMOVE_REF(t_id) boost::remove_reference< t_id >::type
#define REMOVE_PTR(t_id) cake::dereferenced< t_id >::type
#if defined (X86_64) || (defined (__x86_64__))
#define __CAKE_SIGNED_16BIT_INT short
#define __CAKE_UNSIGNED_16BIT_INT unsigned short
#define __CAKE_SIGNED_32BIT_INT int
#define __CAKE_UNSIGNED_32BIT_INT unsigned int
#define __CAKE_SIGNED_64BIT_INT long
#define __CAKE_UNSIGNED_64BIT_INT unsigned long
#else
#define __CAKE_SIGNED_16BIT_INT short
#define __CAKE_UNSIGNED_16BIT_INT unsigned short
#define __CAKE_SIGNED_32BIT_INT int
#define __CAKE_UNSIGNED_32BIT_INT unsigned int
#define __CAKE_SIGNED_64BIT_INT long long
#define __CAKE_UNSIGNED_64BIT_INT unsigned long long
#endif

namespace cake 
{ 
    //typedef struct { int data; } unspecified_wordsize_type;
    typedef struct __cake_dummy_struct 
	{ 
		struct __cake_dummy_struct *self;
		operator void*() { return self; }
	} unspecified_wordsize_type;
#if defined (X86_64) || (defined (__x86_64__))
	typedef long wordsize_integer_type;
#else
	typedef int wordsize_integer_type;
#endif

	template <typename T> 
	struct dereferenced
	{};
	
	template <typename T>
	struct dereferenced<T*>
	{
		typedef T type;
	};
	template <typename T>
	struct dereferenced<boost::optional<T> >
	{
		typedef T type;
	};
	
	// default conversions
	template <typename To, typename From>
	struct default_cast
	{
		To operator()(const From& from) const
		{
			return from; // use default conversions
		}
	};
	template <typename To>
	struct default_cast<To, void>
	{
		To operator()(void) const
		{
			return To(); // use default value
		}
	};
	// specialize for pointers
	template <typename ToPointee, typename FromPointee>
	struct default_cast<ToPointee*, FromPointee*>
	{
		ToPointee* operator()(FromPointee *from) const
		{
			// we have to do reinterpret, *and* 
			return reinterpret_cast<ToPointee*>(from); // use reinterpret
		}
	};
	
// 	// specialize *again* for const from-pointers
// 	template <typename ToPointee, typename FromPointee>
// 	struct default_cast<ToPointee*, const FromPointee*>
// 	{
// 		ToPointee* operator()(FromPointee *from) const
// 		{
// 			// we have to do reinterpret, *and* 
// 			return reinterpret_cast<ToPointee*>(
// 				const_cast<FromPointee*>(from)
// 			); // use reinterpret
// 		}
// 	};	
	// specialize for arrays
	template <typename ToEl, typename FromEl, int Dim>
	struct default_cast<ToEl[Dim], FromEl[Dim]>
	{
		srk31::array<ToEl, Dim> operator()(const FromEl (&from)[Dim]) const
		{
			return srk31::array<ToEl, Dim>(from); 
		}
	};
	// specialize for srk31::arrays as if they were arrays
	template <typename ToEl, typename FromEl, int Dim>
	struct default_cast<ToEl[Dim], srk31::array<FromEl, Dim> >
	{
		srk31::array<ToEl, Dim> operator()(const srk31::array<FromEl, Dim>& from) const
		{
			return srk31::array<ToEl, Dim>(from); 
		}
	};
	
	// we use this instead of void in certain places, because
	// void isn't first-class
	struct no_value_t 
	{
		template <typename T>
		operator T() const { return T(); }
	};

	
	template <typename First, typename Second>
	struct unify_types
	{
		typedef void type;
	};
	template <typename NonVoid>
	struct unify_types<NonVoid, void>
	{
		typedef NonVoid type;
	};
	template <typename NonVoid>
	struct unify_types<void, NonVoid>
	{
		typedef NonVoid type;
	};
	template <typename NonVoid>
	struct unify_types<NonVoid, cake::no_value_t>
	{
		typedef NonVoid type;
	};
	template <typename NonVoid>
	struct unify_types<cake::no_value_t, NonVoid>
	{
		typedef NonVoid type;
	};
//	template <typename Same>
//	struct unify_types<Same, Same>
//	{
//		typedef Same type;
//	};
	/* HACK: it looks like C++ isn't smart enough to understand 
	 * a specialization for "the same type, for any type", 
	 * so we have to enumerate a bunch of cases. */
#define unify_base_pair(t) \
	template <> struct unify_types<t, t> { typedef t type; };
	unify_base_pair(bool)
	unify_base_pair(char)
	unify_base_pair(wchar_t)
	unify_base_pair(unsigned char)
	unify_base_pair(__CAKE_SIGNED_16BIT_INT)
	unify_base_pair(__CAKE_UNSIGNED_16BIT_INT)	
	unify_base_pair(__CAKE_SIGNED_32BIT_INT)
	unify_base_pair(__CAKE_UNSIGNED_32BIT_INT)
	unify_base_pair(__CAKE_SIGNED_64BIT_INT)
	unify_base_pair(__CAKE_UNSIGNED_64BIT_INT) 
	unify_base_pair(float)
	unify_base_pair(double)
	unify_base_pair(long double) 
/* Now we should also do unifying integral types of different sizes... */
	// FIXME
/* Pointers: for now, all pointers unify to void*. */
	template <typename PtrTarget1, typename PtrTarget2>
	struct unify_types<PtrTarget1*, PtrTarget2*>
	{
		typedef void *type;
	};
	
	
	// this is similar, but makes an assignable thing (lvalue) out of its arg
	// -- again, we have to use the struct template
	template <typename Target>
	struct assignable
	{
		Target& operator()(Target& from) const
		{
			return from; // use default conversions
		}
	};
	template <typename TargetEl, int Dim>
	struct assignable<TargetEl[Dim]>
	{
		srk31::array_wrapper<TargetEl, Dim> operator()(TargetEl (&from)[Dim]) const
		{
			return srk31::array_wrapper<TargetEl, Dim>(from); // use default conversions
		}
	};

}

#endif
