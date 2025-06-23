#!/usr/bin/env bash

# default values
OPTION_PLAYER_BRANCH="dev_sprint_25_1"
OPTION_BUILD_DIR=""
OPTION_BUILD_ARGS=""
OPTION_CLEAN=false
OPTION_COVERAGE=false
OPTION_PROTOBUF_REFERENCE="3.11.x"
OPTION_QUICK=false
OPTION_RIALTO_REFERENCE="v0.2.2"
OPTION_RIALTO_BUILD=false
OPTION_SUBTEC_SKIP=false
OPTION_SUBTEC_BUILD=true
OPTION_SUBTEC_CLEAN=false
OPTION_GOOGLETEST_REFERENCE="tags/release-1.11.0"



function install_options_fn()
{
  # Parse optional command line parameters
  while getopts ":d:b:cf:p:r:g:qs" OPT; do
    case ${OPT} in
      d ) # process option d install base directory name
        OPTION_BUILD_DIR=${OPTARG}
        echo "${OPTARG}"
        ;;
      b ) # process option b code branch name
        OPTION_PLAYER_BRANCH=${OPTARG}
        ;;
      c ) # process option c coverage
        OPTION_COVERAGE=ON
        echo coverage "${OPTION_COVERAGE}"
        ;;
      f )# process option f to get compiler flags
         # add flags for cmake build by splitting buildargs with separator ','
        OPTION_BUILD_ARGS=${OPTARG}
        echo "Additional build flags specified '${OPTARG}'"
        ;;
      g )# process option f to get googletest revision to build
        OPTION_GOOGLETEST_REFERENCE=${OPTARG}
        echo "${OPTARG}"
        ;;
      q )# quick option, skips installed (not built) dependency checks
        OPTION_QUICK=true
        echo "Skip : ${QUICK}"
        ;;
      r ) 
        OPTION_RIALTO_REFERENCE=${OPTARG}
        echo "rialto tag : ${RIALTO_REFERENCE}"
        ;;
      s ) 
        OPTION_SUBTEC_SKIP=true
        # overrides any subtec or subtec clean setting
        echo "Skip subtec: ${OPTION_SKIP_SUBTEC}"
        ;;
      p )     
        OPTION_PROTOBUF_REFERENCE=${OPTARG}
        echo "protobuf branch : ${PROTOBUF_REFERENCE}"
        ;;  
      * )
        echo "Usage: $0 [-b branch name] [-d local setup directory name] [-c] [-f compiler flags] [-g google release tag] [-n] [-q] [-s] [subtec [clean]]"
        echo "          [-r rialto tag] [-p protobuf branch name] [rialto] (Linux only)"
        echo
        echo "Note:  Subtec is built by default but can be rebuilt separately with the subtec"
        echo "       option 'clean' will delete the subtec source and reinstall before"
        echo "       building"
        echo
        echo "Note:  Rialto is built with the 'rialto' option. Use '-r' to set the rialto tag, "
        echo "       '-p' to set the Protobuf branch used for Rialto (Linux only)."
        return 1
        ;;
    esac
  done

  # Parse project clean first, allows for subtec [clean]
  if [[ ${@:$OPTIND:1} = "clean" ]]; then
      OPTION_CLEAN=true
      shift
  fi

  # Parse subtec options
  if  [[  ${@:$OPTIND:1} = "subtec" ]]; then
    OPTION_SUBTEC_BUILD=true 
    shift
    if  [[  ${@:$OPTIND:1} = "clean" ]]; then
        OPTION_SUBTEC_CLEAN=true
        shift
    fi
  fi

  if  [[ ${@:$OPTIND:1} = "rialto" ]]; then
    OPTION_RIALTO_BUILD=true
    shift
  fi

}

