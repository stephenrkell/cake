exists elf_reloc("a.o") a;
exists elf_reloc("b.o") b;
derive elf_reloc("out.o") output = link [a, b]
{
	a <--> b
	{
		myRuleName: values foo <--> bar;
		
		values
		{
			myOtherRuleName: baz <--> bum;
			myFinalRuleName: xyzzy <--> plugh;
		}
	}
};
