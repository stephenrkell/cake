#$(warning CLASSPATH is $(CLASSPATH))

GCJFLAGS ?= -fPIC -findirect-dispatch -fno-indirect-classes -g -Wno-unused -Wno-dead-code --classpath=$(CLASSPATH)

ARCHIVE_NAME ?= $(shell basename $(shell readlink -f $(shell pwd)))
#$(warning ARCHIVE_NAME is $(ARCHIVE_NAME))

JAVA_SRC ?= $(shell find -name '*.java')
#$(warning JAVA_SRC is $(JAVA_SRC))
CLASSES ?= 
CLASSFILES := $(patsubst %.java,%.class,$(JAVA_SRC))
#$(warning CLASSFILES is $(CLASSFILES))
JAVA_OBJS := $(patsubst %.java,%.o,$(JAVA_SRC))
#$(warning JAVA_OBJS is $(JAVA_OBJS))

.PHONY: java-all
java-all: $(CLASSFILES) $(JAVA_OBJS) include/stamp $(ARCHIVE_NAME).a stamp

stamp:
	touch stamp

#$(CLASSFILES): $(JAVA_SRC)
%.class: %.java
	gcj $(GCJFLAGS) -C "$<"

dollar := $$
star := *

$(ARCHIVE_NAME).a: $(JAVA_OBJS)
	files="$$( ls $^ $(patsubst %.o,%\$(dollar)$(star).o,$(JAVA_OBJS)) )"; \
             ar r "$@" $^ $$files

%.o: %.class
	gcj $(GCJFLAGS) -o "$@" -c "$<"
# HACK: also make nested classes' object files, because we can't enumerate these statically for Makefile purposes
	for classfile in "$*"\$$*.class; do \
		if [[ $$classfile != "$*"\$$"*".class ]]; then \
			outfile=$$( echo "$$classfile" | sed 's/class$$/o/' ); \
			gcj $(GCJFLAGS) -o "$$outfile" -c "$$classfile"; \
		fi; \
	done

cni-deps: # HACK until proper deps sorted out
	cat *.cpp | \
          grep '#include[[:space:]]*["<]\([a-zA-Z0-9]\+/\)\{2\}' | \
          grep -v '^[[:space:]]*//' | \
          sed 's/.*<\(.*\)\.h>.*/\1/' | \
          sort | uniq | tr '/' '.' > "$@"

%.jar.so: %-*.jar
	gcj -shared $(GCJFLAGS) -o "$@" "$<"

%.jar.so: %.jar
	gcj -shared $(GCJFLAGS) -o "$@" "$<"

%.jar.o: %.jar
	gcj -c $(GCJFLAGS) -o "$@" "$<"


include/stamp: include
	touch include/stamp
include:: $(CLASSFILES)
# HACK: search classdir in order make nested classes' headers also
# 1. Make headers for local classes
	if [[ -n "$(CLASSFILES)" ]]; then gcjh -force -d include --classpath=$(CLASSPATH) \
           $$( find $(CLASSDIR) -name '*.class' ); fi
# 2. Make headers for system classes
	if [[ -n "$(CLASSES)" ]]; then gcjh -force -d include --classpath=$(CLASSPATH) $(CLASSES); \
           fi && touch include

.PHONY: clean
clean::
	rm -f $(JAVA_OBJS) $(CLASSFILES)
	find -name '*$$*.o' -o -name '*$$*.class' | xargs rm -f # HACK
	rm -rf include/*
	rm -f $(ARCHIVE_NAME).a
	rm -f stamp
