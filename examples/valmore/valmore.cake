exists elf_reloc("client.o") client;
exists elf_reloc("lib.o") lib;
derive elf_reloc("valmore.o") valmore = link [client, lib]
{
    client <--> lib
    {
		/* The point of these rules is to test 
		 * - initialization rules
		 * - bidirectional flow of object [references] 
		 *   i.e. firing of init rules
		 * - ...? */
        values gadget <--> gizmo
		{
			// this rule will fill the unused field on *return*
			unused <-- 42;
			
			// this rule will not be used, since client inits gadget
			unused <--? 999;
			
			// this rule will be fired on the first time, but not second
			69105 -->? unuzed;
		};
		pass(a) --> pass(a);
    }
};
