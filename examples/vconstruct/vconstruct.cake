exists elf_reloc("client.o") client
{
	/* Here "outbuf" means a buffer that we are using to
	 * *receive* data from a callee, cf. inbuf which would be a buffer
	 * we were using to transmit data to a callee.
	 * The callee will tell us how much data it wrote. */
	declare 
	{
// 		uio_outbuf: class_of object {
// 			buf: uint8_t ptr;
// 			len: size_t;
// 			off: off_t;
// 		};
		getstuff: (subst_buf : void ptr, 
		           off : long\ unsigned\ int, 
		           len : long\ unsigned\ int) => _;
	}
}
exists elf_reloc("lib.o") lib;
derive elf_reloc("vconstruct.o") vconstruct = link [client, lib] 
{
	client <--> lib
	{
		/* Value construction expressions let us construct a substitute
		 * value for one or more parameters, and use its correspondences
		 * to transfer data in and out of the target stub. */
		 
		
		getstuff(subst_buf as uio_outbuf(subst_buf, off, len), _, _)
		                                         --> readstuff(subst_buf);
		
		values
		{
			/* Here we provide correspondences specifying different
			 * treatments of our data types
			 * uio_outbuf (client) and
			 * uio        (library).
			 * Note that "uio_outbuf" is a "virtual data type":
			 * it exists only to group buf, off and len together
			 * for the purposes of writing a correspondence,
			 * and has no user-visible concrete data type
			 * (cf. artificial data types). */
			
			// Note that the pointer returned by uio_setup 
			// becomes the value bound to the LHS's ident
			// after crossover. It is never allocated a co-object,
			// and infact needn't be an object (i.e. it could be,
			// say, a file descriptor or some other signifier).
			uio_outbuf         --> (uio_setup(subst_buf, len, off) /* WHAT now? */
			                        ) uio;
			
			uio_outbuf <-- uio
			{ len <-- (uio_getresid(here)) void;
			  off <-- (uio_getoff(here)) void;
			  void <-- (uio_free(here)) void;
			}; // ^-- not quite "len" and "off" here -- "*len" and "*off"
			   // BUT we don't want assignment... use "out" somehow?
			   // MAYBE virtual data types automatically reflect the in/out
			   // direction of arguments? YES.
			
		}

	}
};
