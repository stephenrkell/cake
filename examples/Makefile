examples := $(filter-out makefile p2k run-all-stdout.log run-all-stderr.log,$(wildcard [a-z]*))
$(warning Available examples: $(examples))
run-all:
	mkdir -p Results
	for ex in $(examples); do $(MAKE) run-$$ex; done 2>Results/run-all-stderr.log | tee Results/run-all-stdout.log
	
run-%:
	ex="$*"; saveddir=$$(pwd); mkdir -p $$saveddir/Results; cd $$ex && make clean && make || exit 42; \
	LD_PRELOAD=$(PMIRROR_LIB)/libheap_index_fast_hooks.so ./$$ex | \
	diff -u `readlink -f expected.out` /dev/fd/0 > $$saveddir/Results/$$ex.diff; \
	cd "$$saveddir"
