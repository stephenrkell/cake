#include <vector>
#include <map>
#include <string>

namespace cake
{
	struct conv_table_key
	{
		std::vector<std::string> first;
		std::vector<std::string> second;
		bool from_first_to_second;
		int rule_tag;
	};
	typedef std::map<conv_table_key, conv_func_t> conv_table_t;

}
