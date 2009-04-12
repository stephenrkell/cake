GCJFLAGS := -fPIC -g

JAVA_SRC := $(wildcard cake/*.java)
CLASSFILES := $(patsubst %.java,%.class,$(JAVA_SRC))
OBJS := $(patsubst %.java,%.o,$(JAVA_SRC))

default:  $(CLASSFILES) $(OBJS)

$(CLASSFILES): $(JAVA_SRC)
	gcj -MD -C -Wno-unused $(JAVA_SRC)

%.o: %.class
	gcj $(GCJFLAGS) -MD -Wno-unused -c -o "$@" "$<"
# HACK: also make nested classes' object files, because we can't enumerate these statically for Makefile purposes
	for classfile in "$*"\$$*.class; do \
		if [[ $$classfile != "$*"\$$"*".class ]]; then \
			gcj $(GCJFLAGS) -o $$( echo "$$classfile" | sed 's/class$$/o/') -MD -Wno-unused -c "$$classfile"; \
		fi; \
	done

