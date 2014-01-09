#ifndef highlighter_hh_INCLUDED
#define highlighter_hh_INCLUDED

#include "function_group.hh"
#include "function_registry.hh"
#include "memoryview.hh"
#include "string.hh"
#include "utils.hh"

#include <functional>

namespace Kakoune
{

class DisplayBuffer;
class Context;

// An Highlighter is a function which mutates a DisplayBuffer in order to
// change the visual representation of a file. It could be changing text
// color, adding information text (line numbering for example) or replacing
// buffer content (folding for example)

using HighlighterFunc = std::function<void (const Context& context, DisplayBuffer& display_buffer)>;
using HighlighterAndId = std::pair<String, HighlighterFunc>;
using HighlighterParameters = memoryview<String>;
using HighlighterFactory = std::function<HighlighterAndId (HighlighterParameters params)>;
using HighlighterGroup = FunctionGroup<const Context&, DisplayBuffer&>;

struct HighlighterRegistry : FunctionRegistry<HighlighterFactory>,
                             Singleton<HighlighterRegistry>
{};

struct DefinedHighlighters : public HighlighterGroup,
                             public Singleton<DefinedHighlighters>
{
};

}

#endif // highlighter_hh_INCLUDED
