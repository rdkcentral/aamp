#!/bin/bash

# do_clone <params>
# Pass all params to 'git clone'
# If the clone fails then exit the script
function do_clone()
{
    arglist=""
    while [ "$1" ]; do
        arglist+=" $1"
        shift
    done

    echo && echo "Executing: 'git clone $arglist'"
    git clone $arglist

    if [ $? != 0 ]; then
        echo "'git clone $arglist' FAILED"
        exit 0
    fi
}

# do_clone_rdk_repo <branch> <repo>
# Clone a generic RDK repo
# If the destination (repo) dir already exists then skip the clone
function do_clone_rdk_repo() {
    if [ -d $2 ]; then
        echo "Repo '$2' already exists"
        return 1
    else
        do_clone -b $1 https://code.rdkcentral.com/r/rdk/components/generic/$2
    fi
    return 0
}



