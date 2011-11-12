#ifndef buffer_manager_hh_INCLUDED
#define buffer_manager_hh_INCLUDED

#include "buffer.hh"
#include "completion.hh"
#include "utils.hh"

#include <unordered_map>
#include <memory>

namespace Kakoune
{

class BufferManager : public Singleton<BufferManager>
{
public:
    typedef std::unordered_map<std::string, std::unique_ptr<Buffer>> BufferMap;

    struct iterator : public BufferMap::const_iterator
    {
        typedef BufferMap::const_iterator parent_type;

        iterator() {}
        iterator(const parent_type& other) : parent_type(other) {}
        Buffer& operator*()  const { return *(parent_type::operator*().second); }
        Buffer* operator->() const { return parent_type::operator*().second.get(); }
    };

    void register_buffer(Buffer* buffer);
    void delete_buffer(Buffer* buffer);

    iterator begin() const { return iterator(m_buffers.begin()); }
    iterator end() const { return iterator(m_buffers.end()); }

    Buffer* get_buffer(const std::string& name);

    CandidateList complete_buffername(const std::string& prefix,
                                      size_t cursor_pos = std::string::npos);

private:
    BufferMap m_buffers;
};

}

#endif // buffer_manager_hh_INCLUDED
