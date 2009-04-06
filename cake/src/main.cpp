#include <iostream>
#include <cstdlib>
#include <cstring>
#include <cassert>

#include <java/lang/System.h>
#include <java/io/PrintStream.h>
#include <org/antlr/runtime/tree/CommonTree.h>
#undef EOF
#include <org/antlr/runtime/Token.h>

#include "cake.hpp"
#include "main.hpp"

const struct option opts[] = {
	{ "help", false, NULL, 'h' }
};

int main(int argc, char **argv)
{
	int getopt_retval = 0;
	int longindex;
	
	while (getopt_retval != -1)
	{
		getopt_retval = getopt_long(argc, argv, "h", opts, &longindex);
		switch (getopt_retval)
		{
			case 'h':
				usage();
				return 0;
				
			case -1: /* no more options */
				break;

			case '?':
			default:
				usage();
				break;
		}
	}
	
	/* FIXME: this should automatically be added to the ELF .inits, rather than
	 * embedded here. */
	JvCreateJavaVM(NULL);
	JvAttachCurrentThread(NULL, NULL);
	JvInitClass(&java::lang::System::class$);
	
	const char *cakefile = argv[optind];
	if (cakefile == NULL) 
	{
		usage();
		return 1;
	}
	else
	{		
		try
		{
			cake::request req(cakefile);
			return req.process();
		}
		catch (cake::SemanticError *e)
		{
			/* Let's hope the node is from a CommonTree */
			org::antlr::runtime::tree::CommonTree *ct = 
				jcast<org::antlr::runtime::tree::CommonTree *>(e->t);
			std::cout 	<< "Semantic error";
			if (ct != 0)
			{
				org::antlr::runtime::Token *token = ct->getToken();				
				std::cout	<< " at line ";
				std::cout	<< (int) token->getLine();
				std::cout	<< ":";
				std::cout	<< (int) token->getCharPositionInLine();
			}
			std::cout	<< ": " << jtocstring_safe(e->msg);
			std::cout	<< std::endl;
			return 1;
		}
		catch (java::lang::Exception *e)
		{
			java::lang::System::out->print(e->toString());
			return 1;
		}				
	}
}

void usage()
{
	std::cout << "Usage: cake <file>\n";
}

