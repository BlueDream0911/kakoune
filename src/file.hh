#ifndef file_hh_INCLUDED
#define file_hh_INCLUDED

#include "string.hh"

#include "exception.hh"

namespace Kakoune
{

struct file_access_error : runtime_error
{
public:
    file_access_error(const String& filename,
                      const String& error_desc)
        : runtime_error(filename + ": " + error_desc) {}
};

struct file_not_found : file_access_error
{
    file_not_found(const String& filename)
        : file_access_error(filename, "file not found") {}
};

class Buffer;

// parse ~/ and $env values in filename and returns the translated filename
String parse_filename(const String& filename);

String read_file(const String& filename);
Buffer* create_buffer_from_file(const String& filename);
void write_buffer_to_file(const Buffer& buffer, const String& filename);
String find_file(const String& filename, const memoryview<String>& paths);

}

#endif // file_hh_INCLUDED
