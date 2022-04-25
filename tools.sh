#!/bin/bash

set -e

function callbison() {
    [[ -z "$1" ]] && echo "Usage: ./run.sh bison {file}" && exit 1
    file=$(readlink -f $1)
    outfile=$(echo "$(dirname $file)/out/$(basename $file)" | sed 's/\..*$/.output/')
    (cd out && bison $file --report=state) && \
        printf "\n>>>> tail $outfile <<<< \n\n" && \
        tail $outfile
}

function callmy() {
    [[ -z "$1" ]] && echo "Usage: ./run.sh lrparser {file}" && exit 2
    parser=./linux/lrparser
    type=lalr
    ${parser} -g $1 --no-test -t ${type} -o out --sep=':' --strict | \
        perl -0pe 's/.*(> Summary.*)/$1/s'
}

function convert() {
    [[ -z "$2" ]] && echo "Usage: ./run.sh convert {yaccfile} {myfile}" && exit 3
    cat $1 | perl -ne 'print if /%%/ ... /%%/' | \
        perl -pe 's/\x27;\x27/\x27@@\x27/g'    | \
        perl -pe 's/;//g'                      | \
        perl -pe 's/\x27@@\x27/\x27;\x27/g'    > $2
}

function release_win() {
    [[ $OS != Windows_NT ]] && echo 'release function is only for Windows.' && exit 4
    [[ -z "$1" ]] && echo "Usage: ./run.sh release {directoryName}" && exit 4
    mkdir -p "$1/bin"
    mkdir -p "$1/src"
    echo 'set PATH=bin;%PATH%'        > "$1/run.cmd"
    echo 'python3 .\src\gui\main.py' >> "$1/run.cmd"
    cp "$(dirname "$(which dot)")"/* "$1/bin/"
    cp "$(which flex)"               "$1/bin/"
    cp  build/lrparser.exe           "$1/bin/"
    rm -rf src/gui/__pycache__ && cp -r src/gui "$1/src/gui"
}

case $1 in
    bison)
        mkdir -p out && callbison $2
    ;;
    lrparser)
        mkdir -p out && callmy $2
    ;;
    convert)
        convert $2 $3
    ;;
    release)
        release_win $2
    ;;
    *)
    echo 'Error: Argument 1 should be one of those:'
    echo '1) bison    # Call bison'
    echo '2) lrparser # Call lrparser'
    echo '3) convert  # Convert yacc file to lrparser file'
    echo '4) release  # Generate release directory'
    ;;
esac