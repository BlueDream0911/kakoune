## Line comments
declare-option -docstring "characters inserted at the beginning of a commented line" \
    str comment_line '#'

## Block comments
declare-option -docstring "colon separated tuple containing the characters inserted before/after a commented line" \
    str-list comment_block

## Default comments for all languages
hook global BufSetOption filetype=asciidoc %{
    set-option buffer comment_block '///:///'
}

hook global BufSetOption filetype=(c|cpp|go|java|javascript|objc|php|sass|scala|scss|swift) %{
    set-option buffer comment_line '//'
    set-option buffer comment_block '/*:*/'
}

hook global BufSetOption filetype=(cabal|haskell|moon) %{
    set-option buffer comment_line '--'
    set-option buffer comment_block '{-:-}'
}

hook global BufSetOption filetype=clojure %{
    set-option buffer comment_line '#_ '
    set-option buffer comment_block '(comment :)'
}

hook global BufSetOption filetype=coffee %{
    set-option buffer comment_block '###:###'
}

hook global BufSetOption filetype=css %{
    set-option buffer comment_line ''
    set-option buffer comment_block '/*:*/'
}

hook global BufSetOption filetype=d %{
    set-option buffer comment_line '//'
    set-option buffer comment_block '/+:+/'
}

hook global BufSetOption filetype=(gas|ini) %{
    set-option buffer comment_line ';'
}

hook global BufSetOption filetype=haml %{
    set-option buffer comment_line '-#'
}

hook global BufSetOption filetype=html %{
    set-option buffer comment_line ''
    set-option buffer comment_block '<!--:-->'
}

hook global BufSetOption filetype=latex %{
    set-option buffer comment_line '%'
}

hook global BufSetOption filetype=lisp %{
    set-option buffer comment_line ';'
    set-option buffer comment_block '#|:|#'
}

hook global BufSetOption filetype=lua %{
    set-option buffer comment_line '--'
    set-option buffer comment_block '--[[:]]'
}

hook global BufSetOption filetype=markdown %{
    set-option buffer comment_line ''
    set-option buffer comment_block '[//]: # (:)'
}

hook global BufSetOption filetype=perl %{
    set-option buffer comment_block '#[:]'
}

hook global BufSetOption filetype=(pug|rust) %{
    set-option buffer comment_line '//'
}

hook global BufSetOption filetype=python %{
    set-option buffer comment_block "''':'''"
}

hook global BufSetOption filetype=ragel %{
    set-option buffer comment_line '%%'
    set-option buffer comment_block '%%{:}%%'
}

hook global BufSetOption filetype=ruby %{
    set-option buffer comment_block '^begin=:^=end'
}

define-command comment-block -docstring '(un)comment selected lines using block comments' %{
    %sh{
        exec_proof() {
            ## Replace the '<' sign that is interpreted differently in `execute-keys`
            printf %s\\n "$@" | sed 's,<,<lt>,g'
        }

        readonly opening=$(exec_proof "${kak_opt_comment_block%:*}")
        readonly closing=$(exec_proof "${kak_opt_comment_block##*:}")

        if [ -z "${opening}" ] || [ -z "${closing}" ]; then
            echo "echo -debug 'The \`comment_block\` variable is empty, could not comment the selection'"
            exit
        fi

        printf %s\\n "evaluate-commands -draft %{ try %{
            ## The selection is empty
            execute-keys <a-K>\\A[\\h\\v\\n]*\\z<ret>

            try %{
                ## The selection has already been commented
                execute-keys %{<a-K>\\A\\Q${opening}\\E.*\\Q${closing}\\E\\n*\\z<ret>}

                ## Comment the selection
                execute-keys -draft %{a${closing}<esc>i${opening}}
            } catch %{
                ## Uncomment the commented selection
                execute-keys -draft %{s(\\A\\Q${opening}\\E)|(\\Q${closing}\\E\\n*\\z)<ret>d}
            }
        } }"
    }
}

define-command comment-line -docstring '(un)comment selected lines using line comments' %{
    %sh{
        readonly opening="${kak_opt_comment_line}"
        readonly opening_escaped="\\Q${opening}\\E"

        if [ -z "${opening}" ]; then
            echo "echo -debug 'The \`comment_line\` variable is empty, could not comment the line'"
            exit
        fi

        printf %s\\n "evaluate-commands -draft %{
            ## Select the content of the lines, without indentation
            execute-keys <a-s>I<esc><a-l>

            try %{
                ## There’s no text on the line
                execute-keys <a-K>\\A[\\h\\v\\n]*\\z<ret>

                try %{
                    ## The line has already been commented
                    execute-keys %{<a-K>\\A${opening_escaped}<ret>}

                    ## Comment the line
                    execute-keys -draft %{i${opening}}
                } catch %{
                    ## Uncomment the line
                    execute-keys -draft %{s\\A${opening_escaped}\\h*<ret>d}
                }
            }
        }"
    }
}
