namespace cake
{
	template <typename From, typename To, int RuleTag = 0> 
    class value_convert
    {
    	To operator()(const From& from) const
        {
	    	return from; // rely on compiler's default conversions
        }
    };
    
    template <typename FromPtr>
    class value_convert<FromPtr*, int, 0>
    {
        int operator ()(const FromPtr*& from) const
        {
    	    return reinterpret_cast<int>(from); // HACK: allow any pointer-to-int conversion
        }
	};
}
