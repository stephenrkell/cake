exists elf_reloc("foo.o") foo;
exists elf_reloc("bar.o") bar;
exists elf_reloc("baz.o") baz;
derive elf_reloc("foobarbaz.o") foobarbaz = link [foo, bar, baz] 
{ 
	// HACK: this linkRefinement is only added temporarily!
	// ... until I get the gcc patch working
	foo <--> bar
	{
		bar(a) --> bar(a);
	}
	
	bar <--> baz
	{
		baz(a) --> baz(a);
	}
	
	baz <--> foo
	{
		foo(a) --> foo(a);
	}
};
