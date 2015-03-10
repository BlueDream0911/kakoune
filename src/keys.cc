#include "keys.hh"

#include "containers.hh"
#include "exception.hh"
#include "string.hh"
#include "utils.hh"
#include "utf8_iterator.hh"

namespace Kakoune
{

Key canonicalize_ifn(Key key)
{
    if (key.key > 0 and key.key < 27)
    {
        kak_assert(key.modifiers == Key::Modifiers::None);
        key.modifiers = Key::Modifiers::Control;
        key.key = key.key - 1 + 'a';
    }
    return key;
}

struct KeyAndName { const char* name; Codepoint key; };
static constexpr KeyAndName keynamemap[] = {
    { "ret", '\r' },
    { "space", ' ' },
    { "tab", '\t' },
    { "lt", '<' },
    { "gt", '>' },
    { "backspace", Key::Backspace},
    { "esc", Key::Escape },
    { "up", Key::Up },
    { "down", Key::Down},
    { "left", Key::Left },
    { "right", Key::Right },
    { "pageup", Key::PageUp },
    { "pagedown", Key::PageDown },
    { "home", Key::Home },
    { "end", Key::End },
    { "backtab", Key::BackTab },
    { "del", Key::Delete },
};

KeyList parse_keys(StringView str)
{
    KeyList result;
    using Utf8It = utf8::iterator<const char*>;
    for (Utf8It it = str.begin(), str_end = str.end(); it < str_end; ++it)
    {
        if (*it != '<')
        {
            result.push_back({Key::Modifiers::None, *it});
            continue;
        }

        Utf8It end_it = it;
        skip_while(end_it, str_end, [](char c) { return c != '>'; });
        if (end_it == str_end)
        {
            result.push_back({Key::Modifiers::None, *it});
            continue;
        }

        Key::Modifiers modifier = Key::Modifiers::None;

        StringView desc{it.base()+1, end_it.base()};
        if (desc.length() > 2 and desc[1_byte] == '-')
        {
            switch(tolower(desc[0_byte]))
            {
                case 'c': modifier = Key::Modifiers::Control; break;
                case 'a': modifier = Key::Modifiers::Alt; break;
                default:
                    throw runtime_error("unable to parse modifier in " +
                                        StringView{it.base(), end_it.base()+1});
            }
            desc = desc.substr(2_byte);
        }
        auto name_it = find_if(keynamemap, [&desc](const KeyAndName& item)
                                           { return item.name == desc; });
        if (name_it != end(keynamemap))
            result.push_back(canonicalize_ifn({ modifier, name_it->key }));
        else if (desc.char_length() == 1)
            result.push_back(Key{ modifier, desc[0_char] });
        else if (tolower(desc[0_byte]) == 'f' and desc.length() <= 3)
        {
            int val = str_to_int(desc.substr(1_byte));
            if (val >= 1 and val <= 12)
                result.push_back(Key{ modifier, Key::F1 + (val - 1) });
            else
                throw runtime_error("Only F1 through F12 are supported");
        }
        else
            throw runtime_error("Failed to parse " +
                                 StringView{it.base(), end_it.base()+1});

        it = end_it;
    }
    return result;
}

String key_to_str(Key key)
{
    bool named = false;
    String res;
    auto it = find_if(keynamemap, [&key](const KeyAndName& item)
                                  { return item.key == key.key; });
    if (it != end(keynamemap))
    {
        named = true;
        res = it->name;
    }
    else if (key.key >= Key::F1 and key.key < Key::F12)
    {
        named = true;
        res = "F" + to_string((int)(key.key - Key::F1 + 1));
    }
    else
        res = codepoint_to_str(key.key);

    switch (key.modifiers)
    {
    case Key::Modifiers::Control: res = "c-" + res; named = true; break;
    case Key::Modifiers::Alt:     res = "a-" + res; named = true; break;
    default: break;
    }
    if (named)
        res = StringView{'<'} + res + StringView{'>'};
    return res;
}

}
