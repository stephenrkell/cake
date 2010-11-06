style tool_to_api
{
	/* We want this style to generate stubs that
	 * call the tool, exercising the various variety of options.
	 *
	 * There are many ways to do this. One is a stateful
	 * object where we imperatively assign to options.
	 * Another is where the options are a data type that can be assigned as a whole.
	 * Another is where the options are all function arguments.
	 *
	 * We might want to interpret option arguments in various 
	 * ways (as integers, floats, booleans, filenames, ...) ---
	 * easiest way for now is to parameterise these using patterns
	 * for matching each kind). 
	 *
	 * Might we compare with a hand-generated API? Is this useful? 
	 * Problem is that there is often an abstraction gap, e.g.
	 * libtar versus tar.
	 *
	 * Another problem:
	 * many command-line tools are single-function, many-option, stateless.
	 * most APIs are multi-function, few-option, stateful.
	 *
	 * What's a good way to factor options into a natural API?
	 * What we really want to know is which options conflict and
	 * which are mandatory,
	 * i.e. to come up with a disjunction of operations.  */
};

exists tool_to_api(elf_reloc("main.o")) tool_as_api;
