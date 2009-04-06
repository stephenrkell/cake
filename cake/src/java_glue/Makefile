GCJFLAGS := -fPIC -g

default: cake/SemanticError.o

%.class: %.java
	gcj -MD -C -Wno-unused "$<"

%.o: %.class
	gcj $(GCJFLAGS) -MD -Wno-unused -c -o "$@" "$<"
# HACK: also make nested classes' object files, because we can't enumerate these statically for Makefile purposes
	for classfile in "$*"\$$*.class; do \
		if [[ $$classfile != "$*"\$$"*".class ]]; then \
			gcj $(GCJFLAGS) -o $$( echo "$$classfile" | sed 's/class$$/o/') -MD -Wno-unused -c "$$classfile"; \
		fi; \
	done

