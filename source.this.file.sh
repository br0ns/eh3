if [ "$0" = "$BASH_SOURCE" ] ; then
    echo You must source this file.  Like this:
    echo "\$ . $0"
    exit 1
fi

DIR="$(realpath "$(dirname "$BASH_SOURCE")")"
export PATH="$PATH:$DIR/bin"
export PYTHONPATH="$PYTHONPATH:$DIR/src/py"
