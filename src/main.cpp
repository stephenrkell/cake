#include <iostream>
#include <cstdlib>
#include <cstring>
#include <cassert>

#include "request.hpp"
#include "util.hpp"
#include "main.hpp"
// 
// #include <java/lang/System.h>
// #include <java/io/PrintStream.h>
// #include <org/antlr/runtime/tree/CommonTree.h>
// #undef EOF
// #include <org/antlr/runtime/Token.h>

const struct option opts[] = {
	{ "help", false, NULL, 'h' },
	{ "output", true, NULL, 'o' }
	
};

static const char *makefile;

int main(int argc, char **argv)
{
	using namespace cake;
	int getopt_retval = 0;
	int longindex;
	
	while (getopt_retval != -1)
	{
		getopt_retval = getopt_long(argc, argv, "ho:", opts, &longindex);
		switch (getopt_retval)
		{
			case 'h':
				usage();
				return 0;
				
			case 'o':
				makefile = optarg;
				break;
				
				
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
//	JvCreateJavaVM(NULL);
//	JvAttachCurrentThread(NULL, NULL);
//	JvInitClass(&java::lang::System::class$);
	
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
			cake::request req(cakefile, makefile);
			return req.process();
		}
        catch (cake::SemanticError e)
        {
        	std::cerr << e.message() << std::endl;
        }
		catch (cake::TreewalkError e)
		{
// 			/* Let's hope the node is from a CommonTree */
// 			antlr::tree::CommonTree *ct = 
// 				jcast<org::antlr::tree::CommonTree *>(e->t);
			//std::cerr 	<< jtocstring_safe(e->toString()) << std::endl;
            std::cerr << e.message() << std::endl;
// 			//"Semantic error";
// 			if (ct != 0)
// 			{
// 				antlr::Token *token = ct->getToken();				
// 				std::cerr	<< " at line ";
// 				std::cerr	<< (int) token->getLine();
// 				std::cerr	<< ":";
// 				std::cerr	<< (int) token->getCharPositionInLine();
// 			}
// 			std::cerr	<< ": " << jtocstring_safe(e->msg);
// 			std::cerr	<< std::endl;
// 			return 1;
		}
		//catch (java::lang::Exception *e)
        catch (std::string s)
		{
			//java::lang::System::err->print(e->toString());
            std::cerr << s;
			return 1;
		}				
	}
}

void usage()
{
	std::cerr << "Usage: cake <file>\n";
}

