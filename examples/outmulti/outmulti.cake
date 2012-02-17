exists elf_reloc("client.o") client;
exists elf_reloc("lib.o") lib
{
	declare 
	{
		getme: (_, out _, out _) => _;
	}
}
derive elf_reloc("outmulti.o") outmulti = link [client, lib] 
{
	client <--> lib
	{
		get(s) --> { let (s1, s2, s3) = getme(1000) }--
			<--
		{ cb(s, s1, s2, s3) };
	}
};
