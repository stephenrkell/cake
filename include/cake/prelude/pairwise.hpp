namespace cake
{
	/* Q: When do we specialise these templates with RuleTag != 0?
     * A: (tentative) every value correspondence is defined by one? RuleTag is arbitrary?
     * 
     * Q: Do we ever invoke these templates directly, cf. using
     * convert_from_{first,second}_to_{second,first}?
     * A: (tentative) No. */
	template <typename From, typename To, int RuleTag = 0>  
    struct value_convert 
    { 
    	To operator()(const From& from) const 
        { 
	    	return from; // rely on compiler's default conversions 
        } 
    }; 
    // HACK: allow conversion from "unspecified" to/from any pointer type
    template <typename FromPtr> 
    struct value_convert<FromPtr*, unspecified_wordsize_type, 0> 
    { 
        unspecified_wordsize_type operator ()(FromPtr* from) const // NOT a reference 
        { 
        	unspecified_wordsize_type ret;
            ret = reinterpret_cast<unspecified_wordsize_type>(from);  
            return ret;
        } 
	}; 
    template <typename ToPtr> 
    struct value_convert<unspecified_wordsize_type, ToPtr*, 0> 
    { 
        ToPtr* operator ()(const unspecified_wordsize_type& from) const 
        { 
    	    return reinterpret_cast<ToPtr*>(from/*.data*/);
        } 
	}; 
    template <> 
    struct value_convert<wordsize_integer_type, unspecified_wordsize_type, 0> 
    { 
        unspecified_wordsize_type operator ()(const wordsize_integer_type& from) const 
        { 
			assert(sizeof (wordsize_integer_type) == sizeof (unspecified_wordsize_type));
        	unspecified_wordsize_type ret 
             = reinterpret_cast<unspecified_wordsize_type>(from);  
            return ret;
        } 
	}; 
#if defined (X86_64) || (defined (__x86_64__))
    template <> 
    struct value_convert<int, unspecified_wordsize_type, 0> 
    { 
        unspecified_wordsize_type operator ()(const int& from) const 
        { 
        	unspecified_wordsize_type ret 
             = reinterpret_cast<unspecified_wordsize_type>(static_cast<long>(from));  
            return ret;
        } 
	}; 
#endif
    template <> 
    struct value_convert<unspecified_wordsize_type, wordsize_integer_type, 0> 
    { 
        wordsize_integer_type operator ()(const unspecified_wordsize_type& from) const 
        {
			assert(sizeof (wordsize_integer_type) == sizeof (unspecified_wordsize_type));
    	    return reinterpret_cast<wordsize_integer_type>(from);
        } 
	}; 
#if defined (X86_64) || (defined (__x86_64__))
    template <> 
    struct value_convert<unspecified_wordsize_type, int, 0> 
    { 
        int operator ()(const unspecified_wordsize_type& from) const 
        {
    	    return static_cast<int>(reinterpret_cast<long>(from));
        } 
	}; 
#endif
   // handle those pesky zero-length array
    template <typename FromZeroArray, typename T> 
    struct value_convert<FromZeroArray[0], T, 0> 
    { 
        T operator ()(FromZeroArray (&from)[0]) const // NOT a reference 
        { 
        	T ret;
            ret = *reinterpret_cast<T*>(&from);  
            return ret;
        } 
	}; 
    template <typename T, typename ToZeroArray> 
    struct value_convert<T, ToZeroArray[0], 0> 
    { 
        void operator ()(const T& from) const 
        { 
    	    //return *reinterpret_cast<ToZeroArray*>(&from/*.data*/);
        } 
	}; 

	/* Q: When do we specialise this *struct template*? 
     *    (Note that we can't specialise the static functions.)
     *    (Note also that both functions are identical!)
     * A: Once per pair of components?
     * (Is the purpose of this class to select RuleTags?)
     * Yes, i.e. it's a mapping from per-component-pair 
     * (Does that mean it doesn't have RuleTags of its own?)
     * -- No, just that its default rule (0) might not map 
     * to value_convert<> rule 0.
     *  */
    template <typename FirstComponentTag, typename SecondComponentTag>
    struct component_pair {
        template <
		    typename To /*= typename cake::corresponding_cxx_type<FirstComponentTag, SecondComponentTag, Arg, 0>::t*/,
            typename From = ::cake::unspecified_wordsize_type, 
            int RuleTag = 0
        >
        static 
        To
        value_convert_from_first_to_second(From arg)
        {
    	    return value_convert<From, 
                To,
                RuleTag
                >().operator()(arg);
        }
        template <
            typename To /*= typename cake::corresponding_cxx_type<FirstComponentTag, SecondComponentTag, Arg, 0>::t*/,
            typename From = ::cake::unspecified_wordsize_type, 
            int RuleTag = 0
        >
        static 
        To
        value_convert_from_second_to_first(From arg)
        {
    	    return value_convert<From, 
                To,
                RuleTag
                >().operator()(arg);
        }	
    };



}
