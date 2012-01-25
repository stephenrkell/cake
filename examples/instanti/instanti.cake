/* Here the client is actually a "plugin acceptor": 
 * it doesn't have UND symbols for the calls it requires,
 * but instead expects to be given (by a "register" call
 * made on *it*) */

exists elf_reloc("client.o") client;
exists elf_reloc("lib.o") lib;
derive elf_reloc("client_plugged.o") client_plugged
 = instantiate(client, ops, my_ops, "plugged_");
/* instantiate args: (component, structure type, obj name, symbol prefix) */
derive elf_reloc("instanti.o") instanti = link [client_plugged, lib] 
{
	client_plugged <--> lib
	{
		// still avoiding implicit corresps for now...
		plugged_do_something(a) --> really_do_something(a);
	}
};
