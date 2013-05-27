#ifndef context_hh_INCLUDED
#define context_hh_INCLUDED

#include "dynamic_selection_list.hh"

namespace Kakoune
{

class Editor;
class Window;
class Buffer;
class InputHandler;
class UserInterface;
class DisplayLine;

// A Context is used to access non singleton objects for various services
// in commands.
//
// The Context object links an InputHandler, an Editor (which may be a Window),
// and a UserInterface. It may represent an interactive user window, or
// a hook execution or a macro replay.
struct Context
{
    Context();
    explicit Context(Editor& editor);
    Context(InputHandler& input_handler, UserInterface& ui);
    ~Context();

    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;

    Buffer& buffer() const;
    bool has_buffer() const { return (bool)m_editor; }

    Editor& editor() const;
    bool has_editor() const { return (bool)m_editor; }

    Window& window() const;
    bool has_window() const;

    InputHandler& input_handler() const;
    bool has_input_handler() const { return (bool)m_input_handler; }

    UserInterface& ui() const;
    bool has_ui() const { return (bool)m_ui; }

    void change_editor(Editor& editor);

    OptionManager& options() const;
    HookManager& hooks() const;

    void print_status(const DisplayLine& status) const;

    void push_jump();
    const DynamicSelectionList& jump_forward();
    const DynamicSelectionList& jump_backward();
    void forget_jumps_to_buffer(Buffer& buffer);

    int& numeric_param() { return m_numeric_param; }
private:
    safe_ptr<Editor>        m_editor;
    safe_ptr<InputHandler>  m_input_handler;
    safe_ptr<UserInterface> m_ui;

    int m_numeric_param = 0;

    using JumpList = std::vector<DynamicSelectionList>;
    JumpList           m_jump_list;
    JumpList::iterator m_current_jump = m_jump_list.begin();
};

}
#endif // context_hh_INCLUDED
