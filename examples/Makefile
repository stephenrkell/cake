examples := $(filter-out makefile p2k run-all-stdout.log run-all-stderr.log,$(wildcard [a-z]*))
$(warning running examples $(examples))
run-all:
	for ex in $(examples); do \
            saveddir=$$(pwd); cd $$ex && make || break; \
            LD_PRELOAD=$(PMIRROR_LIB)/libheap_index_fast_hooks.so ./$$ex | \
                diff -u /dev/fd/3 /dev/fd/0 3<expected.out; \
            cd "$$saveddir"; \
        done 2>run-all-stderr.log | tee run-all-stdout.log
