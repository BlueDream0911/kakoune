decl line-flag-list git_diff_flags

def git-diff-update-buffer %{ %sh{
    added_lines=""
    removed_lines=""
    git diff -U0 $kak_bufname | {
        line=0
        flags="0:red:."
        while read; do
            if [[ $REPLY =~ ^---.* ]]; then
                continue
            elif [[ $REPLY =~ ^@@.-[0-9]+(,[0-9]+)?.\+([0-9]+)(,[0-9]+)?.@@.* ]]; then
                line=${BASH_REMATCH[2]}
            elif [[ $REPLY =~ ^\+ ]]; then
                flags="$flags,$line:green:+"
                ((line++))
            elif [[ $REPLY =~ ^\- ]]; then
                flags="$flags,$line:red:-"
            fi
        done
        echo "setb git_diff_flags '$flags'"
    }
}}

def git-diff-show %{ addhl flag_lines black git_diff_flags; git-diff-update-buffer }

decl line-flag-list git_blame_flags

def git-blame %{
    try %{ addhl flag_lines magenta git_blame_flags } catch %{}
    setb git_blame_flags ''
    %sh{ (
            declare -A authors
            send_flags() {
                if [[ -z "$line" ]]; then return; fi
                text="${sha:0:8} ${authors[$sha]}"
                flag="$line:black:$text"
                for (( i=1; $i < $count; i++ )); do
                    flag="$flag;$(($line+$i))|black|$text"
                done
                echo "setb -add -buffer $kak_bufname git_blame_flags %{${flag}}" | socat -u stdin UNIX-CONNECT:${kak_socket}
            }
            git blame --incremental $kak_bufname | ( while read blame_line; do
                if [[ $blame_line =~ ([0-9a-f]{40}).([0-9]+).([0-9]+).([0-9]+) ]]; then
                    send_flags
                    sha=${BASH_REMATCH[1]}
                    line=${BASH_REMATCH[3]}
                    count=${BASH_REMATCH[4]}
                elif [[ $blame_line =~ author[^-](.*) ]]; then
                    authors[$sha]=${BASH_REMATCH[1]}
                fi
            done; send_flags )
       ) >& /dev/null < /dev/null & }
}
