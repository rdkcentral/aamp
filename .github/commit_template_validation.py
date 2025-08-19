import subprocess
import sys
import argparse
import re

COMMIT_MESSAGE_FORMAT = '''
###########         ERROR : INVALID COMMIT MESSAGE!          ############
-------------- Please follow the commit message format as below --------------

RDKDEV-1234 : Fixes code download failure to set-top via IP download

Reason for change: Enable capabilities for IPV6 connections .
Test Procedure: Refer ticket
--------------------------------------------------------------------------------
'''


# Execute a command and return the output and error.
#
# Args:
#     cmd (str): The command to execute.
#     ignore (bool): Whether to ignore the error if the command fails. Default is True.
#     cwd (str): The current working directory for the command. Default is None.
#
# Returns:
#     tuple: A tuple containing the output (stdout) and error (stderr) of the command.
def executeCmd(cmd, ignore=True, cwd=None):

    proc = subprocess.Popen("%s" % cmd, shell=True, stderr=subprocess.STDOUT, stdout=subprocess.PIPE, cwd=cwd)
    out, err = proc.communicate()
    if proc.returncode != 0:
        out = proc.returncode
        if ignore is False:
            raise IOError('Failed to execute: %s, error %s' % (cmd, err))
        err = str('Failed to execute: %s, error %d' % (cmd, proc.returncode))
    return out, err

# Validate the commits in a project.
#
# Args:
#     newrev (str): The new revision.
#
# Returns:
#     dict: A dictionary containing the commits and their validation errors.
def validate_commits(newrev, message):

    commits = {}
    #commit_message, _ = executeCmd('git log --format=%s -n 1 {0} --no-merges'.format(newrev), ignore=False)
    errors = []
    errors += validate_message(message)
    if errors:
        commits[newrev] = errors
        print (COMMIT_MESSAGE_FORMAT)
        print('--------------    Commit message in PR is:    --------------')
        print(message)
        print('--------------------------------------------------------------------------------')

    return commits


# Validate the commit message has a JIRA ticket reference and is not a revert.
#  In addition, the commit message should have :
#  1] Synopsis with a Jira ticket reference in first line 
#  2] Title should not exceed 80 characters
#  3] Commit message should not contain new line character
#
# Args:
#     message (str): The commit message.
#
# Returns:
#     list: A list of validation errors.
def validate_message(message):
    errors = []
    JIRA_PROJ_REGEX = r"\s*[A-Z0-9]+-[0-9]+"

    if message.startswith('Revert'):
        # Do not validate revert messages.
        return errors
    
    # check if message contains new line character
    if '\n' in message:
        errors.append('Commit message should not contain new line character')
    
    if len(message) > 80:
        errors.append('Summary line must not exceed 80 characters.')

    if not re.match(JIRA_PROJ_REGEX, message):
        errors.append('Summary line must have at least one JIRA ticket reference.')
   
    return errors
    




# Validate pushed refs.

# Usage:
#     python commit_template_validation.py --newrev <new_revision> --message <commit_message>

# Arguments:
#     --newrev: The sha revision in Pull Request.
#     --message: The commit message title of the PR.

# Returns:
#     int: 0 for success, 1 for failure.

def main():

    parser = argparse.ArgumentParser(description='Validate pushed refs:', formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument('--newrev', required=True)
    parser.add_argument('--message', required=True)

    params = parser.parse_args()

    commits = validate_commits(params.newrev, params.message)
    error_messages = set()
    commits_sha = set()

    for commit, errors in commits.items():
        commits_sha.add(commit)
        for error_message in errors:
            error_messages.add(error_message)

    is_invalid = any(error_messages)
    if is_invalid:
        print('ERRORS:\n - ' + '\n - '.join(error_messages))
        print('COMMIT ID:\n - ' + '\n - '.join(commits_sha))

        print('\n########################################################')

    return 1 if is_invalid else 0

if __name__ == '__main__':
    sys.exit(main())




