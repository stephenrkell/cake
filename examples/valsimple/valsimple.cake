exists elf_reloc("client.o") client;
exists elf_reloc("lib.o") lib;
derive elf_reloc("valsimple.o") app = link [client, lib]
{
    client <--> lib
    {
        values gadget <--> gizmo;
		pass(a) --> pass(a);
    }
};
