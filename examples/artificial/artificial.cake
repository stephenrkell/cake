exists elf_reloc("client.o") client;
exists elf_reloc("lib.o") lib;
derive elf_reloc("artificial.o") artificial = link [client, lib]
{
	client <--> lib
	{
		/* bit of boilerplate while we don't have require func info */
		frob(a as length) --> frob(a);
		
		/* Artificial data types allow typedefs having different treatment. */
		values 
		{
			length          -->(that*4) perimeter;
			length (that/4)<--          perimeter;
			
			hundredths_t            -->(that*10) thousandths_t;
			hundredths_t  (that/10)<--           thousandths_t;
		}
		
		/* We can also introduce them ourselves using "as". */
		
		/* We can use "as" at up to one level of indirection. */
		
		/* We can use in_as and out_as for finer-grained control. */
	
	}
};
