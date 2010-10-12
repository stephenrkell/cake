exists elf_reloc("switch.o") switch12 {
        override {
                .gtk_dialog_new : GtkDialog ptr; // override static type
                        /* NOTE: we might also need a syntax for cases where the downcast target type is
                         * determined at run-time -- maybe a union + discriminant expression above */
        }
        declare {
                .gtk_dialog_new : opaque ptr;
        }
        
}
