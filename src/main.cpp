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

/* New command-line options thing:
 * 1.  no options will write makerules that reflect the entire structure
 *     of Cake derivations. 
 * 1a. These are the only makerules written by Cake.
 * 2.  hpp files are written by Cake using --make-module-headers. 
 * 3.  cpp files are written by Cake using --derive-module <name>
 * 4.  That's it! */

const struct option opts[] = {
	{ "help", false, NULL, 'h' },
	{ "output", true, NULL, 'o' },
	{ "make-module-headers", true, NULL, 'm' },
	{ "derive-module", true, NULL, 'd' }
};

static const char *makefile;

static const char *outfile;
static const char *headers_modulename;
static const char *derived_modulename;

int main(int argc, char **argv)
{
	using namespace cake;
	int getopt_retval = 0;
	int longindex;
	
	while (getopt_retval != -1)
	{
		getopt_retval = getopt_long(argc, argv, "ho:m:d:", opts, &longindex);
		switch (getopt_retval)
		{
			case 'h':
				usage();
				return 0;
				
			case 'o':
				outfile = optarg;
				break;
				
			case 'm':
				headers_modulename = optarg;
				break;
				
			case 'd':
				derived_modulename = optarg;
				break;
			
			case -1: /* no more options */
				break;

			case '?':
			default:
				usage();
				break;
		}
	}
	
	/* Now fix up "makefile". */
	makefile = outfile; // FIXME: more flexibility
	
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
			cake::request req(cakefile, makefile, 0);
			if (!derived_modulename && !headers_modulename)
			{
				req.write_makerules();
			}
			else if (derived_modulename)
			{
				req.do_derivation(derived_modulename);
			}
			else if (headers_modulename)
			{
				req.write_headers(headers_modulename);
			}
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
	std::cerr << "Usage: cake [-o <outfile>]\n"
	             "            [ < --make-module-headers | --derive-module > <modulename> ]\n"
	             "            <file.cake>\n";
}

