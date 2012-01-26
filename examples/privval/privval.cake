exists elf_reloc("client.o") client;
exists elf_reloc("lib.o") lib;
derive elf_reloc("privval.o") privval = link [client, lib] 
{
	client <--> lib
	{
		values val <--> direct
		{
			void (get_hidden(here))        --> d;
			void (set_hidden(here, that)) <--  d;
		}
		pass(a) --> pass(a);
	}
};
