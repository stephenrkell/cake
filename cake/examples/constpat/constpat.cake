exists elf_reloc("client.o") client;
exists elf_reloc("lib.o") lib;
derive elf_reloc("constpat.o") app = link [client, lib]
{
	client <--> lib
    {
		wide(a, b) --> narrow(a);
        wider_still(a, b, 0) --> narrowish(a, b);
        sometimes(a) --> not_always(42);
        stringlit("hello") --> take_string("greetings");
    }
};
