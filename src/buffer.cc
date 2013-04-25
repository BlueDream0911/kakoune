#include "buffer.hh"

#include "assert.hh"
#include "buffer_manager.hh"
#include "context.hh"
#include "file.hh"
#include "utils.hh"
#include "window.hh"

#include <algorithm>

namespace Kakoune
{

Buffer::Buffer(String name, Flags flags, std::vector<String> lines)
    : m_name(std::move(name)), m_flags(flags | Flags::NoUndo),
      m_history(), m_history_cursor(m_history.begin()),
      m_last_save_undo_index(0),
      m_timestamp(0),
      m_hooks(GlobalHooks::instance()),
      m_options(GlobalOptions::instance())
{
    BufferManager::instance().register_buffer(*this);

    if (flags & Flags::File)
        m_name = real_path(m_name);

    if (lines.empty())
        lines.emplace_back("\n");

    ByteCount pos = 0;
    m_lines.reserve(lines.size());
    for (auto& line : lines)
    {
        kak_assert(not line.empty() and line.back() == '\n');
        m_lines.emplace_back(Line{ pos, std::move(line) });
        pos += m_lines.back().length();
    }

    Editor editor_for_hooks(*this);
    Context context(editor_for_hooks);
    if (flags & Flags::File)
    {
        if (flags & Flags::New)
            m_hooks.run_hook("BufNew", m_name, context);
        else
            m_hooks.run_hook("BufOpen", m_name, context);
    }

    m_hooks.run_hook("BufCreate", m_name, context);

    // now we may begin to record undo data
    m_flags = flags;
}

Buffer::~Buffer()
{
    {
        Editor hook_editor{*this};
        Context hook_context{hook_editor};
        m_hooks.run_hook("BufClose", m_name, hook_context);
    }

    BufferManager::instance().unregister_buffer(*this);
    kak_assert(m_change_listeners.empty());
}

String Buffer::display_name() const
{
    if (m_flags & Flags::File)
        return compact_path(m_name);
    return m_name;
}

bool Buffer::set_name(String name)
{
    Buffer* other = BufferManager::instance().get_buffer_ifp(name);
    if (other == nullptr or other == this)
    {
        if (m_flags & Flags::File)
            m_name = real_path(name);
        else
            m_name = std::move(name);
        return true;
    }
    return false;
}

BufferIterator Buffer::iterator_at(const BufferCoord& line_and_column,
                                   bool avoid_eol) const
{
    return BufferIterator(*this, clamp(line_and_column, avoid_eol));
}

ByteCount Buffer::line_length(LineCount line) const
{
    kak_assert(line < line_count());
    ByteCount end = (line < line_count() - 1) ?
                    m_lines[line + 1].start : byte_count();
    return end - m_lines[line].start;
}

BufferCoord Buffer::clamp(const BufferCoord& line_and_column,
                          bool avoid_eol) const
{
    if (m_lines.empty())
        return BufferCoord();

    BufferCoord result(line_and_column.line, line_and_column.column);
    result.line = Kakoune::clamp(result.line, 0_line, line_count() - 1);
    ByteCount max_col = std::max(0_byte, line_length(result.line) - (avoid_eol ? 2 : 1));
    result.column = Kakoune::clamp(result.column, 0_byte, max_col);
    return result;
}

BufferIterator Buffer::iterator_at_line_begin(LineCount line) const
{
    line = Kakoune::clamp(line, 0_line, line_count()-1);
    kak_assert(line_length(line) > 0);
    return BufferIterator(*this, { line, 0 });
}

BufferIterator Buffer::iterator_at_line_begin(const BufferIterator& iterator) const
{
    return iterator_at_line_begin(iterator.line());
}

BufferIterator Buffer::iterator_at_line_end(LineCount line) const
{
    line = Kakoune::clamp(line, 0_line, line_count()-1);
    kak_assert(line_length(line) > 0);
    return ++BufferIterator(*this, { line, line_length(line) - 1 });
}

BufferIterator Buffer::iterator_at_line_end(const BufferIterator& iterator) const
{
    return iterator_at_line_end(iterator.line());
}

BufferIterator Buffer::begin() const
{
    return BufferIterator(*this, { 0_line, 0 });
}

BufferIterator Buffer::end() const
{
    if (m_lines.empty())
        return BufferIterator(*this, { 0_line, 0 });
    return BufferIterator(*this, { line_count()-1, m_lines.back().length() });
}

ByteCount Buffer::byte_count() const
{
    if (m_lines.empty())
        return 0;
    return m_lines.back().start + m_lines.back().length();
}

LineCount Buffer::line_count() const
{
    return LineCount(m_lines.size());
}

String Buffer::string(const BufferIterator& begin, const BufferIterator& end) const
{
    String res;
    for (LineCount line = begin.line(); line <= end.line(); ++line)
    {
       ByteCount start = 0;
       if (line == begin.line())
           start = begin.column();
       ByteCount count = -1;
       if (line == end.line())
           count = end.column() - start;
       res += m_lines[line].content.substr(start, count);
    }
    return res;
}

// A Modification holds a single atomic modification to Buffer
struct Buffer::Modification
{
    enum Type { Insert, Erase };

    Type        type;
    BufferCoord coord;
    String      content;

    Modification(Type type, BufferCoord coord, String content)
        : type(type), coord(coord), content(std::move(content)) {}

    Modification inverse() const
    {
        Type inverse_type = Insert;
        switch (type)
        {
        case Insert: inverse_type = Erase;  break;
        case Erase:  inverse_type = Insert; break;
        default: kak_assert(false);
        }
        return {inverse_type, coord, content};
    }
};

class UndoGroupOptimizer
{
    static constexpr auto Insert = Buffer::Modification::Type::Insert;
    static constexpr auto Erase = Buffer::Modification::Type::Erase;

    static BufferCoord advance(BufferCoord coord, const String& str)
    {
        for (auto c : str)
        {
            if (c == '\n')
            {
                ++coord.line;
                coord.column = 0;
            }
            else
                ++coord.column;
        }
        return coord;
    }

    static ByteCount count_byte_to(BufferCoord pos, const BufferCoord& endpos, const String& str)
    {
        ByteCount count = 0;
        for (auto it = str.begin(); it != str.end() and pos != endpos; ++it)
        {
            if (*it == '\n')
            {
                ++pos.line;
                pos.column = 0;
            }
            else
                ++pos.column;
            ++count;
        }
        assert(pos == endpos);
        return count;
    }

    static const ByteCount overlaps(const String& lhs, const String& rhs)
    {
        if (lhs.empty() or rhs.empty())
            return -1;

        char c = rhs.front();
        ByteCount pos = 0;
        while ((pos = (int)lhs.find_first_of(c, (int)pos)) != -1)
        {
            ByteCount i = pos, j = 0;
            while (i != lhs.length() and j != rhs.length() and lhs[i] == rhs[j])
                 ++i, ++j;
            if (i == lhs.length())
                break;
            ++pos;
        }
        return pos;
    }

    static bool merge_contiguous(Buffer::UndoGroup& undo_group)
    {
        bool progress = false;
        auto it = undo_group.begin();
        auto it_next = it+1;
        while (it_next != undo_group.end())
        {
            ByteCount pos;
            auto& coord = it->coord;
            auto& next_coord = it_next->coord;

            // reorders modification doing a kind of custom bubble sort
            // so we have a O(n²) worst case complexity of the undo group optimization
            if (next_coord < coord)
            {
                BufferCoord next_end = advance(next_coord, it_next->content);
                if (it_next->type == Insert)
                {
                     if (coord.line == next_coord.line)
                         coord.column += next_end.column - next_coord.column;
                     coord.line += next_end.line - next_coord.line;
                }
                else if (it->type == Insert)
                {
                    if (next_end > coord)
                    {
                        ByteCount start = count_byte_to(next_coord, coord, it_next->content);
                        ByteCount len = std::min(it->content.length(), it_next->content.length() - start);
                        it->coord = it_next->coord;
                        it->content = it->content.substr(len);
                        it_next->content = it_next->content.substr(0,start) + it_next->content.substr(start + len);
                    }
                    else
                    {
                        if (next_end.line == coord.line)
                        {
                            coord.line = next_coord.line;
                            coord.column = next_coord.column + coord.column - next_end.column;
                        }
                        else
                            coord.line -= next_end.line - next_coord.line;
                    }
                }
                else if (it->type == Erase and next_end > coord)
                {
                    ByteCount start = count_byte_to(next_coord, coord, it_next->content);
                    it_next->content = it_next->content.substr(0, start) + it->content + it_next->content.substr(start);
                    it->coord = it_next->coord;
                    it->content.clear();
                }
                std::swap(*it, *it_next);
                progress = true;
            }

            if (it->type == Erase and it_next->type == Erase and coord == next_coord)
            {
                it->content += it_next->content;
                it_next = undo_group.erase(it_next);
                progress = true;
            }
            else if (it->type == Insert and it_next->type == Insert and
                     is_in_range(next_coord, coord, advance(coord, it->content)))
            {
                ByteCount prefix_len = count_byte_to(coord, next_coord, it->content);
                it->content = it->content.substr(0, prefix_len) + it_next->content
                            + it->content.substr(prefix_len);
                it_next = undo_group.erase(it_next);
                progress = true;
            }
            else if (it->type == Insert and it_next->type == Erase and
                     coord <= next_coord and next_coord < advance(coord, it->content))
            {
                ByteCount insert_len = it->content.length();
                ByteCount erase_len  = it_next->content.length();
                ByteCount prefix_len = count_byte_to(coord, next_coord, it->content);

                it->content = it->content.substr(0, prefix_len) +
                              ((prefix_len + erase_len < insert_len) ?
                               it->content.substr(prefix_len + erase_len) : "");
                it_next->content = (insert_len - prefix_len < erase_len) ?
                                   it_next->content.substr(insert_len - prefix_len) : "";
                ++it, ++it_next;
                progress = true;
            }
            else if (it->type == Erase and it_next->type == Insert and coord == next_coord and
                     (pos = overlaps(it->content, it_next->content)) != -1)
            {
                ByteCount overlaps_len = it->content.length() - pos;
                it->content = it->content.substr(0, pos);
                it_next->content = it_next->content.substr(overlaps_len);
                ++it, ++it_next;
                progress = true;
            }
            else
                ++it, ++it_next;
        }
        return progress;
    }

    static bool erase_empty(Buffer::UndoGroup& undo_group)
    {
        auto it = std::remove_if(begin(undo_group), end(undo_group),
                                 [](Buffer::Modification& m) { return m.content.empty(); });
        if (it != end(undo_group))
        {
            undo_group.erase(it, end(undo_group));
            return true;
        }
        return false;
    }
public:
    static void optimize(Buffer::UndoGroup& undo_group)
    {
        while (undo_group.size() > 1)
        {
            bool progress = false;
            progress |= merge_contiguous(undo_group);
            progress |= erase_empty(undo_group);
            if (not progress)
                break;
        }
    }
};

void Buffer::commit_undo_group()
{
    if (m_flags & Flags::NoUndo)
        return;

    UndoGroupOptimizer::optimize(m_current_undo_group);

    if (m_current_undo_group.empty())
        return;

    m_history.erase(m_history_cursor, m_history.end());

    m_history.push_back(std::move(m_current_undo_group));
    m_current_undo_group.clear();
    m_history_cursor = m_history.end();

    if (m_history.size() < m_last_save_undo_index)
        m_last_save_undo_index = -1;
}

bool Buffer::undo()
{
    commit_undo_group();

    if (m_history_cursor == m_history.begin())
        return false;

    --m_history_cursor;

    for (const Modification& modification : reversed(*m_history_cursor))
        apply_modification(modification.inverse());
    return true;
}

bool Buffer::redo()
{
    if (m_history_cursor == m_history.end())
        return false;

    kak_assert(m_current_undo_group.empty());

    for (const Modification& modification : *m_history_cursor)
        apply_modification(modification);

    ++m_history_cursor;
    return true;
}

void Buffer::check_invariant() const
{
#ifdef KAK_DEBUG
    ByteCount start = 0;
    kak_assert(not m_lines.empty());
    for (auto& line : m_lines)
    {
        kak_assert(line.start == start);
        kak_assert(line.length() > 0);
        kak_assert(line.content.back() == '\n');
        start += line.length();
    }
#endif
}

void Buffer::do_insert(const BufferIterator& pos, const String& content)
{
    kak_assert(pos.is_valid());

    if (content.empty())
        return;

    ++m_timestamp;
    ByteCount offset = pos.offset();

    // all following lines advanced by length
    for (LineCount i = pos.line()+1; i < line_count(); ++i)
        m_lines[i].start += content.length();

    BufferIterator begin_it;
    BufferIterator end_it;
    // if we inserted at the end of the buffer, we have created a new
    // line without inserting a '\n'
    if (pos.is_end())
    {
        ByteCount start = 0;
        for (ByteCount i = 0; i < content.length(); ++i)
        {
            if (content[i] == '\n')
            {
                m_lines.push_back({ offset + start, content.substr(start, i + 1 - start) });
                start = i + 1;
            }
        }
        if (start != content.length())
            m_lines.push_back({ offset + start, content.substr(start) });

        begin_it = pos.column() == 0 ? pos : BufferIterator{*this, { pos.line() + 1, 0 }};
        end_it = end();
    }
    else
    {
        String prefix = m_lines[pos.line()].content.substr(0, pos.column());
        String suffix = m_lines[pos.line()].content.substr(pos.column());

        std::vector<Line> new_lines;

        ByteCount start = 0;
        for (ByteCount i = 0; i < content.length(); ++i)
        {
            if (content[i] == '\n')
            {
                String line_content = content.substr(start, i + 1 - start);
                if (start == 0)
                {
                    line_content = prefix + line_content;
                    new_lines.push_back({ offset + start - prefix.length(),
                                          std::move(line_content) });
                }
                else
                    new_lines.push_back({ offset + start, std::move(line_content) });
                start = i + 1;
            }
        }
        if (start == 0)
            new_lines.push_back({ offset + start - prefix.length(), prefix + content + suffix });
        else if (start != content.length() or not suffix.empty())
            new_lines.push_back({ offset + start, content.substr(start) + suffix });

        LineCount last_line = pos.line() + new_lines.size() - 1;

        auto line_it = m_lines.begin() + (int)pos.line();
        *line_it = std::move(*new_lines.begin());
        m_lines.insert(line_it+1, std::make_move_iterator(new_lines.begin() + 1),
                       std::make_move_iterator(new_lines.end()));

        begin_it = pos;
        end_it = BufferIterator{*this, { last_line, m_lines[last_line].length() - suffix.length() }};
    }

    for (auto listener : m_change_listeners)
        listener->on_insert(begin_it, end_it);
}

void Buffer::do_erase(const BufferIterator& begin, const BufferIterator& end)
{
    kak_assert(begin.is_valid());
    kak_assert(end.is_valid());
    ++m_timestamp;
    const ByteCount length = end - begin;
    String prefix = m_lines[begin.line()].content.substr(0, begin.column());
    String suffix = m_lines[end.line()].content.substr(end.column());
    Line new_line = { m_lines[begin.line()].start, prefix + suffix };

    if (new_line.length() != 0)
    {
        m_lines.erase(m_lines.begin() + (int)begin.line(), m_lines.begin() + (int)end.line());
        m_lines[begin.line()] = std::move(new_line);
    }
    else
        m_lines.erase(m_lines.begin() + (int)begin.line(), m_lines.begin() + (int)end.line() + 1);

    for (LineCount i = begin.line()+1; i < line_count(); ++i)
        m_lines[i].start -= length;

    for (auto listener : m_change_listeners)
        listener->on_erase(begin, end);
}

void Buffer::apply_modification(const Modification& modification)
{
    const String& content = modification.content;
    BufferCoord coord = modification.coord;

    // this may happen when a modification applied at the
    // end of the buffer has been inverted for an undo.
    if (coord.line < line_count()-1 and coord.column == m_lines[coord.line].length())
        coord = { coord.line + 1, 0 };

    BufferIterator pos{*this, coord};
    switch (modification.type)
    {
    case Modification::Insert:
    {
        do_insert(pos, content);
        break;
    }
    case Modification::Erase:
    {
        ByteCount count = content.length();
        BufferIterator end = pos + count;
        kak_assert(string(pos, end) == content);
        do_erase(pos, end);
        break;
    }
    default:
        kak_assert(false);
    }
}

void Buffer::insert(BufferIterator pos, String content)
{
    if (content.empty())
        return;

    if (pos.is_end() and content.back() != '\n')
        content += '\n';

    if (not (m_flags & Flags::NoUndo))
        m_current_undo_group.emplace_back(Modification::Insert, pos.coord(), content);
    do_insert(pos, content);
}

void Buffer::erase(BufferIterator begin, BufferIterator end)
{
    if (end.is_end() and (begin.column() != 0 or begin.is_begin()))
        --end;

    if (begin == end)
        return;

    if (not (m_flags & Flags::NoUndo))
        m_current_undo_group.emplace_back(Modification::Erase, begin.coord(),
                                          string(begin, end));
    do_erase(begin, end);
}

bool Buffer::is_modified() const
{
    size_t history_cursor_index = m_history_cursor - m_history.begin();
    return m_last_save_undo_index != history_cursor_index
           or not m_current_undo_group.empty();
}

void Buffer::notify_saved()
{
    if (not m_current_undo_group.empty())
        commit_undo_group();

    m_flags &= ~Flags::New;
    size_t history_cursor_index = m_history_cursor - m_history.begin();
    if (m_last_save_undo_index != history_cursor_index)
    {
        ++m_timestamp;
        m_last_save_undo_index = history_cursor_index;
    }
}

bool Buffer::is_valid(const BufferCoord& c) const
{
   return (c.line < line_count() and c.column < m_lines[c.line].length()) or
          (c.line == line_count() - 1 and c.column == m_lines.back().length()) or
          (c.line == line_count() and c.column == 0);
}

}
