style rpc_to_api
	(servicename, // string
	 grammar in pattern ...)
{
	// for each subprogram in the grammar, create an entry point
	[x: _(...), x in grammar] 
	{ send(handle, va_pack(args...)) ;&
	  out va_unpack(recv(handle, va_unpack)) } <-- x(args...);
	
};

exists rpc_to_api( /* what goes here? */ ) rpc_as_api;
/* Here we slice the fread(stdin, ...)
                 and fwrite(stdout, ...) interface
   out of the C library
   -- we mux a bunch of entry points over the fwrite()
      and demux response-parsers over the fread() */
