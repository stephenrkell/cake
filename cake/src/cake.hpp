#include <gcj/cni.h>
#include <java/io/File.h>
#include <java/io/FileInputStream.h>
#include <java/lang/ClassCastException.h>

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

template<class T>
inline T *jcast (java::lang::Object *o)
{
        if (T::class$.isAssignableFrom (o->getClass ()))
                return reinterpret_cast<T*>(o);
        else
                throw new java::lang::ClassCastException;
}
