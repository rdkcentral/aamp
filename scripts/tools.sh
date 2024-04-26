#!/usr/bin/env bash

function tools_banner_fn ()
{
                                                                  
  echo ""
  echo "        #         #     ####   #### ######          ### # ####   #### "
  echo "       ###       ###     ###   ###   ##  ##        #   ##  ##     ##  "
  echo "       ###       ###     # ## # ##   ##  ##       ##    #  ##     ##  "
  echo "      #  ##     #  ##    # ## # ##   ##  ##       ##       ##     ##  "
  echo "      #  ##     #  ##    # ## # ##   #####  ##### ##       ##     ##  "
  echo "     #######   #######   #  ##  ##   ##           ##    #  ##  #  ##  "
  echo "     #    ##   #    ##   #  ##  ##   ##            #    #  ##  #  ##  "
  echo "    ###  #### ###  #### ### ## #### ####            ####  ###### #### "
  echo ""
}

function tools_arch_fn ()
{
    if [[ "${OSTYPE}" == "darwin"* ]]; then    
        ARCH=$(uname -m)
        if [[ ${ARCH} == "x86_64" ]]; then
            echo "${ARCH}"
        elif [[ ${ARCH} == "arm64" ]]; then
            echo "${ARCH}"
        else
            echo "Achitecture $ARCH is unsupported"
            return 1
        fi

    elif [[ "${OSTYPE}" == "linux"* ]]; then
        :
    else
       echo "OS ${OSTYPE} is unsupported"
       return 1
    fi
}

function tools_install_fn ()
{
    if [[ "${OSTYPE}" == "darwin"* ]]; then    

        echo "Checking Xcode..."

        #Install XCode Command Line Tools
        MIN_MACOS_VER=10.15

        CUR_MACOS_VER=$(sw_vers | grep ProductVersion | grep -Eo '[0-9]+\.[0-9]+')
        CUR_XTOOLS_VER=$(xcrun --sdk macosx --show-sdk-path | grep -Eo '[0-9]+\.[0-9]+')
        CUR_XCODE_VER=$(xcodebuild -version | grep Xcode | grep -Eo '[0-9]+\.[0-9]+')

        echo "MacOS ${CUR_MACOS_VER} / Xcode ${CUR_XCODE_VER} / Command Line Tools ${CUR_XTOOLS_VER}"

        #Check if Xcode is installed
        if XPATH=$( xcode-select --print-path ) &&
            test -d "${XPATH}" && test -x "${XPATH}" ; then
            echo "Xcode tools installed in '${XPATH}'"
        else    
            #... isn't correctly installed
            echo "Xcode tools installation not found."
            echo -e "
  Command Line Tools for XCode can be found here. 
  https://developer.apple.com/download/all/?q=command%20line%20tools%20for%20xcode
 
  Double-click the DMG file to make its content available (the name will show up in the Finder sidebar), 
  and a window generally opens also showing the content. Drag the application from the DMG window into 
  the Applications directory to install (you may need an administrator password). Wait for the copy process to finish.

  Then run the following:
  sudo installer -pkg /Library/Developer/CommandLineTools/Packages/macOS_SDK_headers_for_macOS_\${CUR_TOOLS_VER}.pkg -target /
  where \${CUR_TOOLS_VER} is the tools version number, e.g. 13.3 or 14.4
  sudo xcode-select -s /Applications/Xcode.app/Contents/Developer
"
            return 1
        fi 


    elif [[ "${OSTYPE}" == "linux"* ]]; then

        echo "Checking VSCode..."
        sudo snap install --classic code || RC=$?
	if [ "${RC}" != 0 ] ; then
	    INSTALL_STATUS_ARR+=("WARNING tools_install_fn: snap install VSCode failed, not halting the build.")
	    return 0
        fi

        echo "Installing VSCode Dependencies..."
        code --install-extension ms-vscode.cmake-tools
    fi
}

function tools_print_summary_fn()
{
echo ""
echo "********AAMP install summary start************"

for item in "${!INSTALL_STATUS_ARR[@]}";
    do
        echo "$item ${INSTALL_STATUS_ARR[$item]}"
    done
echo ""
echo "********AAMP install summary end*************"

echo -n "build completed $(date) elapsed time "
if [[ "${OSTYPE}" == "darwin"* ]]; then    
    date -ju -f "%s" ${SECONDS}  +%T
else
    date -u -d "@$SECONDS" +%T
fi
echo ""
}
