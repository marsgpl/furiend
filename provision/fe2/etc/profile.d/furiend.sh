[[ $- == *i* ]] || return

alias ll="ls -Alh --group-directories-first --color=always"

export PS1="\[\033[0;35m\]\h\[\033[0m\] \w \[\033[0;35m\]#\[\033[0m\] "
export PATH="$PATH:/furiend/bin"
