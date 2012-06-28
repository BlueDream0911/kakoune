#ifndef buffer_hh_INCLUDED
#define buffer_hh_INCLUDED

#include <vector>
#include <list>
#include <memory>

#include "line_and_column.hh"
#include "option_manager.hh"
#include "hook_manager.hh"
#include "string.hh"

namespace Kakoune
{

class Buffer;
class Modification;
class Window;

typedef int       BufferPos;
typedef int       BufferSize;

struct BufferCoord : LineAndColumn<BufferCoord>
{
    BufferCoord(int line = 0, int column = 0)
        : LineAndColumn(line, column) {}

    template<typename T>
    explicit BufferCoord(const LineAndColumn<T>& other)
        : LineAndColumn(other.line, other.column) {}
};

// A BufferIterator permits to iterate over the characters of a buffer
class BufferIterator
{
public:
    typedef Character  value_type;
    typedef BufferSize difference_type;
    typedef const value_type* pointer;
    typedef const value_type& reference;
    typedef std::bidirectional_iterator_tag iterator_category;

    BufferIterator() : m_buffer(nullptr) {}
    BufferIterator(const Buffer& buffer, BufferCoord coord);
    BufferIterator& operator=(const BufferIterator& iterator);

    bool operator== (const BufferIterator& iterator) const;
    bool operator!= (const BufferIterator& iterator) const;
    bool operator<  (const BufferIterator& iterator) const;
    bool operator<= (const BufferIterator& iterator) const;
    bool operator>  (const BufferIterator& iterator) const;
    bool operator>= (const BufferIterator& iterator) const;

    Character  operator* () const;
    BufferSize operator- (const BufferIterator& iterator) const;

    BufferIterator operator+ (BufferSize size) const;
    BufferIterator operator- (BufferSize size) const;

    BufferIterator& operator+= (BufferSize size);
    BufferIterator& operator-= (BufferSize size);

    BufferIterator& operator++ ();
    BufferIterator& operator-- ();

    bool is_begin() const;
    bool is_end() const;
    bool is_valid() const;

    void on_insert(const BufferCoord& begin, const BufferCoord& end);
    void on_erase(const BufferCoord& begin, const BufferCoord& end);

    const Buffer& buffer() const;
    BufferSize line() const { return m_coord.line; }
    BufferSize column() const { return m_coord.column; }

private:
    BufferSize offset() const;

    const Buffer* m_buffer;
    BufferCoord   m_coord;
    friend class Buffer;
};

// A Modification holds a single atomic modification to Buffer
struct Modification
{
    enum Type { Insert, Erase };

    Type           type;
    BufferIterator position;
    String         content;

    Modification(Type type, BufferIterator position, const String& content)
        : type(type), position(position), content(content) {}

    Modification inverse() const;

    static Modification make_erase(BufferIterator begin, BufferIterator end);
    static Modification make_insert(BufferIterator position,
                                    const String& content);
};

// A Buffer is a in-memory representation of a file
//
// The Buffer class permits to read and mutate this file
// representation. It also manage modifications undo/redo and
// provides tools to deal with the line/column nature of text.
class Buffer : public SafeCountable
{
public:
    enum class Type
    {
        File,
        NewFile,
        Scratch
    };

    Buffer(const String& name, Type type,
           const String& initial_content = "\n");
    Buffer(const Buffer&) = delete;
    Buffer(Buffer&&) = delete;
    Buffer& operator= (const Buffer&) = delete;
    ~Buffer();

    Type type() const { return m_type; }

    // apply given modification to buffer.
    void           modify(Modification&& modification);

    void           begin_undo_group();
    void           end_undo_group();
    bool           undo();
    bool           redo();

    String         string(const BufferIterator& begin,
                          const BufferIterator& end) const;

    BufferIterator begin() const;
    BufferIterator end() const;
    BufferSize     character_count() const;
    BufferSize     line_count() const;

    BufferIterator iterator_at(const BufferCoord& line_and_column) const;
    BufferCoord    line_and_column_at(const BufferIterator& iterator) const;

    // returns nearest valid coordinates from given ones
    BufferCoord    clamp(const BufferCoord& line_and_column) const;

    const String& name() const { return m_name; }

    Window* get_or_create_window();
    void delete_window(Window* window);

    // returns true if the buffer is in a different state than
    // the last time it was saved
    bool is_modified() const;

    // notify the buffer that it was saved in the current state
    void notify_saved();

    void add_iterator_to_update(BufferIterator& iterator);
    void remove_iterator_from_update(BufferIterator& iterator);

    // returns an iterator pointing to the first character of the line
    // iterator is on
    BufferIterator iterator_at_line_begin(const BufferIterator& iterator) const;

    // returns an iterator pointing to the character after the last of the
    // line iterator is on (which is the first of the next line if iterator is
    // not on the last one)
    BufferIterator iterator_at_line_end(const BufferIterator& iterator) const;

    const String& line_content(size_t l) const { return m_lines[l].content; }

    OptionManager& option_manager() { return m_option_manager; }
    HookManager&   hook_manager()   { return m_hook_manager; }

private:
    friend class BufferIterator;

    void check_invariant() const;

    struct Line
    {
        BufferPos start;
        String    content;

        size_t length() const { return content.length(); }
    };
    std::vector<Line> m_lines;

    void insert(const BufferIterator& pos, const String& content);
    void erase(const BufferIterator& pos, BufferSize length);

    BufferPos line_at(const BufferIterator& iterator) const;
    BufferSize line_length(BufferPos line) const;

    String  m_name;
    const Type   m_type;

    typedef std::vector<Modification> UndoGroup;

    std::vector<UndoGroup>           m_history;
    std::vector<UndoGroup>::iterator m_history_cursor;
    UndoGroup                        m_current_undo_group;

    void apply_modification(const Modification& modification);
    void revert_modification(const Modification& modification);

    std::list<std::unique_ptr<Window>> m_windows;

    size_t m_last_save_undo_index;

    std::vector<BufferIterator*> m_iterators_to_update;

    OptionManager m_option_manager;
    HookManager   m_hook_manager;
};

inline Modification Modification::make_erase(BufferIterator begin,
                                             BufferIterator end)
{
    return Modification(Erase, begin, begin.buffer().string(begin, end));
}

inline Modification Modification::make_insert(BufferIterator position,
                                              const String& content)
{
    return Modification(Insert, position, content);
}

}

#include "buffer_iterator.inl.hh"

#endif // buffer_hh_INCLUDED
