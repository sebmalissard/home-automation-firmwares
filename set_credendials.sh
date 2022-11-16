#!/bin/bash

# Installation requirement:
#     sudo apt install keepassxc
# For WSLv1, to fix error:  "libQt5Core.so.5: cannot open shared object file: No such file or directory":
#     sudo strip --remove-section=.note.ABI-tag /usr/lib/x86_64-linux-gnu/libQt5Core.so.5

USER_CMD="${0} ${*}"

KEEPASSXC_DATABASE_FILE=""
SOURCE_FILE=""

source tools/bash-tools/messages.sh
source tools/bash-tools/keepassxc.sh

print_help()
{
    echo -e ""
    echo -e "This script will remplace all the @KPXC@ with the associate secret from an input file to a new file ."
    echo -e ""
    echo -e "Usage:"
    echo -e "   $(basename "${0}") -d <keepassxc_database> -f <stub_file>"
    echo -e ""
    echo -e "Arguments:"
    echo -e "    -d : Path to the KeepassXC database to use"
    echo -e "    -f : Input file to set secret, should finish by STUB, for example Credendials_STUB.h"
    echo -e ""
}

while getopts "hd:f:" opt; do
  case "$opt" in
    h)
        print_help
        exit 0
        ;;
    d)
        KEEPASSXC_DATABASE_FILE="${OPTARG}"
        ;;
    f)
        SOURCE_FILE="${OPTARG}"
        ;;
    *)  
        error "Incorrect argument"
        print_help
        exit 1
        ;;
    
  esac
done

if [ ! -f "${KEEPASSXC_DATABASE_FILE}" ]; then
    fatal "Cannot found database '${KEEPASSXC_DATABASE_FILE}'"
fi

if [ ! -f "${SOURCE_FILE}" ]; then
    fatal "Cannot found file '${SOURCE_FILE}'"
fi

if ! echo  "${SOURCE_FILE}" | grep -q "_STUB"; then
    fatal "Incorrect input STUB file"
fi

GEN_FILE="${SOURCE_FILE//_STUB/}"

if ! cp "${SOURCE_FILE}" "${GEN_FILE}"; then
    fatal "Fail to create file: ${GEN_FILE}"
fi

sed -i "1s|^|\n|" "${GEN_FILE}"
sed -i "1s|^|//    ${USER_CMD}\n|" "${GEN_FILE}"
sed -i "1s|^|// File generated automatically with the following command:\n|" "${GEN_FILE}"
sed -i "1s|^|// DO NOT MODIFY\n|" "${GEN_FILE}"

keepassxc_set_error_fatal
keepassxc_set_database_path "${KEEPASSXC_DATABASE_FILE}"
keepassxc_set_password_from_user_with_verify

KPXC_LIST=$(grep -n @KPXC "${GEN_FILE}")

while read -r IN; do
    debug "IN=${IN}"
    
    LINE=$(echo "${IN}" | awk -F: '{print $1}')
    debug "LINE=${LINE}"

    KPXC=$(echo "${IN}" | awk -F '@KPXC|@' '{print $2}')
    debug "KPXC=${KPXC}"

    TYPE=$(echo "${KPXC}" | awk -F '/|/' '{print $2}')
    debug "TYPE=${TYPE}"

    if [ "${TYPE}" == "USER" ]; then
        ENTRY=$(echo "${KPXC}" | awk -F '/|/' '{print $3}')
        debug "ENTRY=${ENTRY}"
        SECRET=$(keepassxc_get_username "${ENTRY}")
        debug "SECRET=${SECRET}"
        sed -i "${LINE}s|@KPXC.*@|${SECRET}|" "${GEN_FILE}"
    elif [ "${TYPE}" == "PASSWORD" ]; then
        ENTRY=$(echo "${KPXC}" | awk -F '/|/' '{print $3}')
        debug "ENTRY=${ENTRY}"
        SECRET=$(keepassxc_get_password "${ENTRY}")
        debug "SECRET=${SECRET}"
        sed -i "${LINE}s|@KPXC.*@|${SECRET}|" "${GEN_FILE}"
    elif [ "${TYPE}" == "ATTRIBUTE" ]; then
        ENTRY=$(echo "${KPXC}" | awk -F '/|/' '{print $3}')
        debug "ENTRY=${ENTRY}"
        ATTRIBUTE=$(echo "${KPXC}" | awk -F '/|/' '{print $4}')
        debug "ATTRIBUTE=${ATTRIBUTE}"
        SECRET=$(keepassxc_get_attribute "${ENTRY}" "${ATTRIBUTE}")
        debug "SECRET=${SECRET}"
        sed -i "${LINE}s|@KPXC.*@|${SECRET}|" "${GEN_FILE}"
    else
        warning "Unsupported format: ${TYPE}"
    fi
done <<< "${KPXC_LIST}"

keepassxc_clear_password
