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
		fillme: (_, out p_buf) => _;
	}
}
exists elf_reloc("lib.o") lib
{
	declare 
	{
		fillme: (_, out p_buf) => _;
	}
}
derive elf_reloc("declareout.o") declareout = link [client, lib] {};
