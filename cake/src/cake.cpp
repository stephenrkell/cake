#include <string>
#include "cake.hpp"
#include "parser.hpp"

namespace cake
{
	request::request(jstring filename)
		: in_filename(filename), in_fileobj(in_filename), in_file(in_fileobj)
	{
		
	}
	int request::process() { return 0; }	
}
