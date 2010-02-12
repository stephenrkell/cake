exists elf_reloc("client.o") client;
exists elf_reloc("lib.o") lib;
derive elf_reloc("remap.o") app = link [client, lib]
{
	client <--> lib
    {
		div(a, b) --> div_into(b, a);
    }
};
