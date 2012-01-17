style cli_to_api
	(grammar in pattern ...)
{
	/* This style amounts to a pretty-printer.
	 * The converse would amount to a parser.
	 *
	 * Can we argue that Cake has sufficient power
	 * to act as a parser? 
	 *
	 * We can't do lookahead. But we can do delayed action
	 * and context-specificity. Let's assume we just want
	 * to output a parenthesised version of the input (or error).
	 * Can we do enough for an LL(1) grammar? */
	 
	// for each subprogram in the grammar, create an entry point
	[x: _(...) in ] fwrite(stdin, ) <--
	// GAH: we need coroutines here I think---at least,
	// the CLI and API-caller need to run in parallel
	
	// another PROBLEM: fwrite() doesn't feed into fread()!
	// stdin and stdout are different streams and not connected to a pipe
	// --- unless we set them up that way by pipe() and freopen()...?
	// --- this would break too much of the rest of the program
	// --- can we use a pair of fresh fds and not stdin/stdout? maybe, but...
	// it's trickier to slice on them (need to remember the fds we are initialized)
	// i.e.
	
	fread(stdin, ...)[0] --> { pipes = pipe(); 
	                           new_file = fdopen(pipes[0]); 
							   fread(new_file 
	
	// can we instead assume that we're a master process
	// whose stdout is routed to stdin of the CLI
	// and the stdin is sourced from the stdout of the CLI?
	// Yes, but where & how to start this process?
	// We could just fork() ourselves but somehow override 
	// the entry point before we exec()...
	// .. or actually we don't want to exec() because
	//    we already

};

exists cli_to_api(elf_reloc("main.o")) cli_as_api;
/* Here we slice the fread(stdin, ...)
                 and fwrite(stdout, ...) interface
   out of the C library
   -- we mux a bunch of entry points over the fwrite()
      and demux response-parsers over the fread() */
