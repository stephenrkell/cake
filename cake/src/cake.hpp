#include <gcj/cni.h>
#include <java/io/File.h>
#include <java/io/FileInputStream.h>

namespace cake
{
	class request
	{
		jstring in_filename;
		java::io::File *in_fileobj;
		java::io::FileInputStream *in_file;
	public:
		request(jstring filename);
		int process();
	};
}
