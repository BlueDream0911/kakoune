# http://elm-lang.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](elm) %{
    set-option buffer filetype elm
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/ regions -default code elm \
    string   '"'     (?<!\\)(\\\\)*"      '' \
    comment  (--) $                       '' \
    comment \{-   -\}                    \{- \

add-highlighter shared/elm/string  fill string
add-highlighter shared/elm/comment fill comment

add-highlighter shared/elm/code regex \b(import|exposing|as|module|where)\b 0:meta
add-highlighter shared/elm/code regex \b(True|False)\b 0:value
add-highlighter shared/elm/code regex \b(if|then|else|case|of|let|in|type|port|alias)\b 0:keyword
add-highlighter shared/elm/code regex \b(Array|Bool|Char|Float|Int|String)\b 0:type

# Commands
# ‾‾‾‾‾‾‾‾

# http://elm-lang.org/docs/style-guide

define-command -hidden elm-filter-around-selections %{
    # remove trailing white spaces
    try %{ exec -draft -itersel <a-x> s \h+$ <ret> d }
}

define-command -hidden elm-indent-after "
 exec -draft \\; k x <a-k> ^\\h*(if)|(case\\h+[\\w']+\\h+of|let|in|\\{\\h+\\w+|\\w+\\h+->|[=(])$ <ret> j <a-gt>
"

define-command -hidden elm-indent-on-new-line %{
    eval -draft -itersel %{
        # copy -- comments prefix and following white spaces
        try %{ exec -draft k <a-x> s ^\h*\K--\h* <ret> y gh j P }
        # preserve previous line indent
        try %{ exec -draft \; K <a-&> }
        # align to first clause
        try %{ exec -draft \; k x X s ^\h*(if|then|else)?\h*(([\w']+\h+)+=)?\h*(case\h+[\w']+\h+of|let)\h+\K.* <ret> s \A|.\z <ret> & }
        # filter previous line
        try %{ exec -draft k : elm-filter-around-selections <ret> }
        # indent after lines beginning with condition or ending with expression or =(
        try %{ elm-indent-after }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group elm-highlight global WinSetOption filetype=elm %{ add-highlighter window ref elm }

hook global WinSetOption filetype=elm %{
    hook window InsertEnd  .* -group elm-hooks  elm-filter-around-selections
    hook window InsertChar \n -group elm-indent elm-indent-on-new-line
}

hook -group elm-highlight global WinSetOption filetype=(?!elm).* %{ remove-highlighter window/elm }

hook global WinSetOption filetype=(?!elm).* %{
    remove-hooks window elm-indent
    remove-hooks window elm-hooks
}
