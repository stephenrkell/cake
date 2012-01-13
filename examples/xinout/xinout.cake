exists elf_reloc("client.o") client;
exists elf_reloc("lib.o") lib;
derive elf_reloc("xinout.o") xinout = link [client, lib]
{
	client <--> lib
	{
		/* bit of boilerplate while we don't have require func info */
		send(a as safe_obj ptr) --> send(a);
		
		values 
		{
			safe_obj  -->({lock(this); that})   lockable_obj;
			safe_obj <-- ({unlock(this); this}) lockable_obj;
		}
	}
};
