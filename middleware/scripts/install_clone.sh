#!/usr/bin/env bash

# do_clone <params>
# Pass all params to 'git clone'
# If the clone fails then exit the script
function do_clone_fn()
{
    ARGLIST=""
    while [ "$1" ]; do
        ARGLIST+=" $1"
        shift
    done

    echo && echo "Executing: 'git clone $ARGLIST'"
    git clone $ARGLIST

    if [ $? != 0 ]; then
        echo "'git clone $ARGLIST' FAILED"
        return 1
    fi
}

# do_clone_rdk_repo <branch> <repo>
# Clone a generic RDK repo
# If the destination (repo) dir already exists then skip the clone
function do_clone_rdk_repo_fn() {
    if [ -d $2 ]; then
        echo "Repo '$2' already exists"
        pushd $2
        git fetch
        git checkout $1
        git pull
        popd
    else
        do_clone_fn -b $1 https://code.rdkcentral.com/r/rdk/components/generic/$2
    fi
}

# do_clone_github_repo <repo> <dir> [...]
# Clone a repo from github into a directory
# If the destination <dir> already exists then skip the clone
function do_clone_github_repo_fn() {
    if [ -d $2 ]; then
        echo "Repo in '$2' already exists"
        return 1
    else
        do_clone_fn "$@"
    fi
}
