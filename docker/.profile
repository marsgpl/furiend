alias ll="ls -Alh --group-directories-first --color=always"

export PS1="$(tput setaf 1)shell$(tput sgr0) \w $(tput setaf 1)#$(tput sgr0) "
export PATH="$PATH:/furiend/bin"

alias tests='lua /furiend/test/main.lua'
