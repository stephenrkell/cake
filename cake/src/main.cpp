#include <iostream>
#include <cstdlib>
#include <cstring>

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
	
	const char *cakefile = argv[optind];
	if (cakefile == NULL) 
	{
		usage();
		return 1;
	}
	else
	{		
		cake::request req(JvNewStringUTF(cakefile));
		return req.process();
	}
}

void usage()
{
	std::cout << "Usage: cake <file>\n";
}
