USE_MALLOC_HOOKS ?= fast
PMIRROR_LIB ?= $(realpath ../../libpmirror.hg/lib)

PRELOAD_LIB := $(PMIRROR_LIB)/libheap_index_$(USE_MALLOC_HOOKS)_hooks.so

examples := $(filter-out makefile p2k run-all-stdout.log run-all-stderr.log,$(wildcard [a-z]*))
$(warning Available examples: $(examples))
run-all:
	mkdir -p Results
	for ex in $(examples); do $(MAKE) run-$$ex; done 2>Results/run-all-stderr.log | \
	tee Results/run-all-stdout.log; cat Results/*.diff
	
run-%:
	ex="$*"; saveddir=$$(pwd); \
	mkdir -p $$saveddir/Results; \
	cd $$ex && ( $(MAKE) || (echo "Build error."; false) ) && \
	pwd && \
	(LD_PRELOAD="$(PRELOAD_LIB) $${LD_PRELOAD}" ./$$ex; \
	echo "Exited with status $$?") | \
	diff -u `readlink -f expected.out` /dev/fd/0 \
	> $$saveddir/Results/$$ex.diff; \
	cd "$$saveddir"

debug-%:
	ex="$*"; saveddir=$$(pwd); cd $$ex; \
	gdb --eval-command "set environment LD_PRELOAD=$(PMIRROR_LIB)/libheap_index_fast_hooks.so" --args ./$$ex; \
	cd "$$saveddir"
