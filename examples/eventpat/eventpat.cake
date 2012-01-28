exists elf_reloc("client.o") client;
exists elf_reloc("lib.o") lib;
derive elf_reloc("eventpat.o") eventpat = link [client, lib] 
{
    client <--> lib
    {
         // FIXME: the semantics of this v-- are *not* the same as (name: _)
         pattern  /foo_do_(.*)/ (frotz, blorb, ...)
            <--> bar_do_\\1 (blorb, frotz, ...);
         // HACK: should really be "-->" but this doesn't parse,
         // because of sink-as-stub grammar asymmetry ("..." kills it)
    }
};
