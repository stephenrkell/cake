handle SIGPWR pass
handle SIGPWR nostop
handle SIGXCPU pass
handle SIGXCPU nostop
break 'cake::elf_module::eval_claim_depthfirst(org::antlr::runtime::tree::Tree*, bool (cake::module::*)(org::antlr::runtime::tree::Tree*, unsigned long long), unsigned long long)'
disable 1
break module.hpp:39
