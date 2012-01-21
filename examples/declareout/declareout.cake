/* Here we 
 * - test the "out" annotation;
 * - test the initialization of co-objects passed by out ptr.
 */

/* We declare the *same* signature both times, to avoid the 
 * creation of an argument mismatch. The mismatch is just in
 * the object layout. */
exists elf_reloc("client.o") client
{
	declare 
	{
		fillme: (_, out p_buf) => void;
	}
}
exists elf_reloc("lib.o") lib
{
	declare 
	{
		fillme: (_, out _) => void;
	}
}
derive elf_reloc("declareout.o") declareout = link [client, lib] 
{
	client <--> lib
	{
		// still avoiding implicit corresps for now...
		fillme(a, b) --> fillme(a, b);
	}
};
