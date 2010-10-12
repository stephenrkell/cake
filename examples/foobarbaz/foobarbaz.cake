exists elf_reloc("foo.o") foo;
exists elf_reloc("bar.o") bar;
exists elf_reloc("baz.o") baz;
derive elf_reloc("foobarbaz.o") foobarbaz = link [foo, bar, baz];
