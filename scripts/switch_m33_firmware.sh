#!/usr/bin/env bash

set -Eeuo pipefail

FIRMWARE_DIR="/lib/firmware"

usage() {
    echo "Usage:"
    echo "  sudo $0 <remoteproc-number> <firmware-file>"
    echo
    echo "Examples:"
    echo "  sudo $0 0 stm32mp257_m33.elf"
    echo "  sudo $0 1 zephyr/stm32mp257_m33.elf"
    echo
    echo "You can also run the script without arguments:"
    echo "  sudo $0"
}

die() {
    echo "Error: $*" >&2
    exit 1
}

wait_for_state() {
    local state_file="$1"
    local expected_state="$2"
    local timeout_seconds="${3:-10}"
    local elapsed=0
    local current_state

    while (( elapsed < timeout_seconds )); do
        current_state="$(cat "${state_file}")"

        if [[ "${current_state}" == "${expected_state}" ]]; then
            return 0
        fi

        sleep 1
        ((elapsed += 1))
    done

    echo "Timed out waiting for state '${expected_state}'." >&2
    echo "Current state: $(cat "${state_file}")" >&2
    return 1
}

find_firmware() {
    local requested_firmware="$1"
    local requested_path
    local filename
    local relative_path
    local -a matches=()

    # Prevent paths outside /lib/firmware.
    if [[ "${requested_firmware}" == /* ]] ||
       [[ "${requested_firmware}" == *"../"* ]] ||
       [[ "${requested_firmware}" == ".." ]]; then
        die "Firmware must be specified as a filename or a path relative to ${FIRMWARE_DIR}."
    fi

    requested_path="${FIRMWARE_DIR}/${requested_firmware}"

    # First check the exact relative path supplied by the user.
    if [[ -f "${requested_path}" ]]; then
        realpath --relative-to="${FIRMWARE_DIR}" "${requested_path}"
        return 0
    fi

    # Otherwise search recursively using the basename.
    filename="$(basename "${requested_firmware}")"

    mapfile -d '' matches < <(
        find "${FIRMWARE_DIR}" \
            -type f \
            -name "${filename}" \
            -print0
    )

    if (( ${#matches[@]} == 0 )); then
        die "Firmware '${requested_firmware}' was not found under ${FIRMWARE_DIR}."
    fi

    if (( ${#matches[@]} > 1 )); then
        echo "Multiple firmware files named '${filename}' were found:" >&2

        for match in "${matches[@]}"; do
            relative_path="$(
                realpath --relative-to="${FIRMWARE_DIR}" "${match}"
            )"
            echo "  ${relative_path}" >&2
        done

        die "Specify the relative firmware path to select the correct file."
    fi

    realpath --relative-to="${FIRMWARE_DIR}" "${matches[0]}"
}

main() {
    local remoteproc_number="${1:-}"
    local requested_firmware="${2:-}"

    local remoteproc_dir
    local name_file
    local state_file
    local firmware_file

    local selected_firmware
    local current_state
    local current_firmware

    if (( $# > 2 )); then
        usage
        exit 1
    fi

    if (( EUID != 0 )); then
        die "This script must be run as root. Use sudo."
    fi

    # Ask interactively when the remoteproc number is not provided.
    if [[ -z "${remoteproc_number}" ]]; then
        read -r -p "Enter remoteproc number, for example 0 or 1: " \
            remoteproc_number
    fi

    # Only allow numeric remoteproc indexes.
    if [[ ! "${remoteproc_number}" =~ ^[0-9]+$ ]]; then
        die "Invalid remoteproc number '${remoteproc_number}'. It must be numeric."
    fi

    remoteproc_dir="/sys/class/remoteproc/remoteproc${remoteproc_number}"
    name_file="${remoteproc_dir}/name"
    state_file="${remoteproc_dir}/state"
    firmware_file="${remoteproc_dir}/firmware"

    if [[ ! -d "${remoteproc_dir}" ]]; then
        echo "Available remote processors:" >&2

        for path in /sys/class/remoteproc/remoteproc*; do
            [[ -d "${path}" ]] || continue

            if [[ -r "${path}/name" ]]; then
                echo "  $(basename "${path}"): $(cat "${path}/name")" >&2
            else
                echo "  $(basename "${path}")" >&2
            fi
        done

        die "Remote processor '${remoteproc_dir}' does not exist."
    fi

    # Ask interactively when the firmware name is not provided.
    if [[ -z "${requested_firmware}" ]]; then
        read -r -p "Enter firmware filename: " requested_firmware
    fi

    if [[ -z "${requested_firmware}" ]]; then
        die "Firmware filename cannot be empty."
    fi

    [[ -r "${name_file}" ]] ||
        die "Cannot read '${name_file}'."

    [[ -r "${state_file}" && -w "${state_file}" ]] ||
        die "Cannot read or write '${state_file}'."

    [[ -r "${firmware_file}" && -w "${firmware_file}" ]] ||
        die "Cannot read or write '${firmware_file}'."

    selected_firmware="$(find_firmware "${requested_firmware}")"

    current_state="$(cat "${state_file}")"
    current_firmware="$(cat "${firmware_file}")"

    echo
    echo "Remote processor : remoteproc${remoteproc_number}"
    echo "Processor name   : $(cat "${name_file}")"
    echo "Current state    : ${current_state}"
    echo "Current firmware : ${current_firmware}"
    echo "New firmware     : ${selected_firmware}"
    echo

    case "${current_state}" in
        running | attached)
            echo "Stopping remoteproc${remoteproc_number}..."
            printf '%s\n' "stop" > "${state_file}"
            wait_for_state "${state_file}" "offline" 10
            ;;

        offline)
            echo "remoteproc${remoteproc_number} is already offline."
            ;;

        *)
            echo "Current state is '${current_state}'."
            echo "Attempting to stop remoteproc${remoteproc_number}..."

            printf '%s\n' "stop" > "${state_file}"
            wait_for_state "${state_file}" "offline" 10
            ;;
    esac

    echo "Selecting firmware '${selected_firmware}'..."
    printf '%s\n' "${selected_firmware}" > "${firmware_file}"

    echo "Starting remoteproc${remoteproc_number}..."
    printf '%s\n' "start" > "${state_file}"

    wait_for_state "${state_file}" "running" 10

    echo
    echo "Firmware started successfully."
    echo "Remote processor : remoteproc${remoteproc_number}"
    echo "Processor name   : $(cat "${name_file}")"
    echo "State            : $(cat "${state_file}")"
    echo "Firmware         : $(cat "${firmware_file}")"
}

main "$@"