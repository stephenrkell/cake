exists elf_reloc("client.o") client;
exists elf_reloc("lib.o") lib;
derive elf_reloc("xinout.o") xinout = link [client, lib]
{
	client <--> lib
	{
		/* bit of boilerplate while we don't have require func info */
		send(0, a as safe_obj ptr) --> atomic(a);
		
		send(1, a)                 --> nonatomic(a as fragile_obj);
		
		send(2, a)                 --> locked_on_return(a out_as unlockme_obj);
		
		values 
		{
			safe_obj    <-->                       lockable_obj;
			safe_obj     -->({lock(this); that})   fragile_obj;
			safe_obj    <-- ({unlock(this); this}) fragile_obj;
			safe_obj    <-- ({unlock(this); this}) unlockme_obj;
		}
	}
};
