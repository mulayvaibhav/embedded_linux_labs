#!/bin/bash

# =============================================================================
# File        : setup-wifi.sh
# Description : Interactive Wi-Fi setup for OpenSTLinux
#
# Usage:
#   sudo ./setup-wifi.sh
#
# Optional:
#   sudo ./setup-wifi.sh wlan0
#
# The script:
#   1. Detects the Wi-Fi interface
#   2. Scans available Wi-Fi networks
#   3. Lets the user select an SSID
#   4. Requests credentials
#   5. Creates persistent wpa_supplicant configuration
#   6. Configures DHCP through systemd-networkd
#   7. Enables automatic Wi-Fi connection after reboot
#   8. Tests the connection
# =============================================================================

set -euo pipefail

WIFI_IF="${1:-wlan0}"

WPA_DIR="/etc/wpa_supplicant"
WPA_CONF="${WPA_DIR}/wpa_supplicant-${WIFI_IF}.conf"

NETWORKD_DIR="/etc/systemd/network"
NETWORK_CONF="${NETWORKD_DIR}/25-${WIFI_IF}.network"

COUNTRY_DEFAULT="DE"

# -----------------------------------------------------------------------------
# Helper functions
# -----------------------------------------------------------------------------

print_info()
{
    echo
    echo "INFO: $*"
}

print_error()
{
    echo
    echo "ERROR: $*" >&2
}

cleanup_password()
{
    unset WIFI_PASSWORD 2>/dev/null || true
}

trap cleanup_password EXIT


# -----------------------------------------------------------------------------
# Check root privileges
# -----------------------------------------------------------------------------

if [ "$(id -u)" -ne 0 ]; then
    print_error "This script must be run as root."
    echo "Run:"
    echo "  sudo $0"
    exit 1
fi


echo "============================================================"
echo " STM32 Autonomous Car - Wi-Fi Setup"
echo "============================================================"
echo


# -----------------------------------------------------------------------------
# Check required tools
# -----------------------------------------------------------------------------

REQUIRED_COMMANDS=(
    iw
    ip
    systemctl
    wpa_supplicant
)

for command in "${REQUIRED_COMMANDS[@]}"; do
    if ! command -v "${command}" >/dev/null 2>&1; then
        print_error "Required command '${command}' is not installed."
        exit 1
    fi
done


# -----------------------------------------------------------------------------
# Check Wi-Fi interface
# -----------------------------------------------------------------------------

if ! ip link show "${WIFI_IF}" >/dev/null 2>&1; then

    print_error "Wi-Fi interface '${WIFI_IF}' was not found."

    echo
    echo "Available interfaces:"
    ip -brief link

    exit 1
fi


print_info "Using Wi-Fi interface: ${WIFI_IF}"


# -----------------------------------------------------------------------------
# Check for NetworkManager conflict
# -----------------------------------------------------------------------------

if systemctl is-active --quiet NetworkManager.service 2>/dev/null; then

    print_error "NetworkManager is currently active."

    echo "This script is designed for:"
    echo "  wpa_supplicant + systemd-networkd"
    echo
    echo "Disable NetworkManager first or modify the script to use nmcli."

    exit 1
fi


# -----------------------------------------------------------------------------
# Enable the Wi-Fi interface
# -----------------------------------------------------------------------------

if command -v rfkill >/dev/null 2>&1; then
    rfkill unblock wifi || true
fi

ip link set "${WIFI_IF}" up

sleep 2


# -----------------------------------------------------------------------------
# Scan Wi-Fi networks
# -----------------------------------------------------------------------------

print_info "Scanning available Wi-Fi networks..."

SCAN_OUTPUT="$(iw dev "${WIFI_IF}" scan 2>/dev/null || true)"

mapfile -t SSIDS < <(
    printf '%s\n' "${SCAN_OUTPUT}" |
        sed -n 's/^[[:space:]]*SSID: //p' |
        sed '/^[[:space:]]*$/d' |
        sort -u
)

if [ "${#SSIDS[@]}" -eq 0 ]; then

    print_error "No Wi-Fi networks were detected."

    echo
    echo "Check:"
    echo "  ip link show ${WIFI_IF}"
    echo "  rfkill list"
    echo "  iw dev ${WIFI_IF} scan"

    exit 1
fi


echo
echo "Available Wi-Fi networks:"
echo "------------------------------------------------------------"

for i in "${!SSIDS[@]}"; do
    printf "  %2d) %s\n" "$((i + 1))" "${SSIDS[$i]}"
done

echo "------------------------------------------------------------"


# -----------------------------------------------------------------------------
# Select network
# -----------------------------------------------------------------------------

while true; do

    echo
    read -r -p "Select Wi-Fi network [1-${#SSIDS[@]}]: " SELECTION

    if [[ "${SELECTION}" =~ ^[0-9]+$ ]] &&
       [ "${SELECTION}" -ge 1 ] &&
       [ "${SELECTION}" -le "${#SSIDS[@]}" ]; then
        break
    fi

    echo "Invalid selection."
done


SSID="${SSIDS[$((SELECTION - 1))]}"

echo
echo "Selected network: ${SSID}"


# -----------------------------------------------------------------------------
# Country code
# -----------------------------------------------------------------------------

echo
read -r -p "Wi-Fi country code [${COUNTRY_DEFAULT}]: " COUNTRY

COUNTRY="${COUNTRY:-${COUNTRY_DEFAULT}}"


# -----------------------------------------------------------------------------
# Select security mode
# -----------------------------------------------------------------------------

echo
echo "Security type:"
echo
echo "  1) WPA/WPA2 Personal (normal home/office Wi-Fi)"
echo "  2) WPA2 Enterprise - PEAP/MSCHAPv2"
echo "  3) Open network"
echo

read -r -p "Select security type [1]: " SECURITY_TYPE

SECURITY_TYPE="${SECURITY_TYPE:-1}"


# -----------------------------------------------------------------------------
# Backup existing configuration
# -----------------------------------------------------------------------------

mkdir -p "${WPA_DIR}"

if [ -f "${WPA_CONF}" ]; then

    BACKUP="${WPA_CONF}.backup.$(date +%Y%m%d-%H%M%S)"

    print_info "Backing up existing configuration to:"
    echo "${BACKUP}"

    cp "${WPA_CONF}" "${BACKUP}"
fi


# -----------------------------------------------------------------------------
# Generate wpa_supplicant configuration
# -----------------------------------------------------------------------------

TEMP_WPA_CONF="$(mktemp)"

chmod 600 "${TEMP_WPA_CONF}"


case "${SECURITY_TYPE}" in

    1)
        # -------------------------------------------------------------
        # WPA/WPA2 Personal
        # -------------------------------------------------------------

        if ! command -v wpa_passphrase >/dev/null 2>&1; then
            print_error "wpa_passphrase is not installed."
            exit 1
        fi

        while true; do

            echo
            read -r -s -p "Wi-Fi password: " WIFI_PASSWORD
            echo

            if [ "${#WIFI_PASSWORD}" -ge 8 ] &&
               [ "${#WIFI_PASSWORD}" -le 63 ]; then
                break
            fi

            echo "WPA password must normally be between 8 and 63 characters."
        done


        {
            echo "ctrl_interface=/run/wpa_supplicant"
            echo "update_config=1"
            echo "country=${COUNTRY}"
            echo

            # wpa_passphrase prints a commented plaintext password.
            # Remove that line before saving the configuration.
            printf '%s\n' "${WIFI_PASSWORD}" |
                wpa_passphrase "${SSID}" |
                sed '/^[[:space:]]*#psk=/d'

        } > "${TEMP_WPA_CONF}"
        ;;


    2)
        # -------------------------------------------------------------
        # WPA2 Enterprise - PEAP/MSCHAPv2
        # -------------------------------------------------------------

        echo
        read -r -p "Username / identity: " WIFI_USERNAME

        echo
        read -r -s -p "Password: " WIFI_PASSWORD
        echo

        cat > "${TEMP_WPA_CONF}" <<EOF
ctrl_interface=/run/wpa_supplicant
update_config=1
country=${COUNTRY}

network={
    ssid="${SSID}"
    key_mgmt=WPA-EAP
    eap=PEAP
    identity="${WIFI_USERNAME}"
    password="${WIFI_PASSWORD}"
    phase2="auth=MSCHAPV2"
}
EOF
        ;;


    3)
        # -------------------------------------------------------------
        # Open Wi-Fi
        # -------------------------------------------------------------

        cat > "${TEMP_WPA_CONF}" <<EOF
ctrl_interface=/run/wpa_supplicant
update_config=1
country=${COUNTRY}

network={
    ssid="${SSID}"
    key_mgmt=NONE
}
EOF
        ;;


    *)
        print_error "Unsupported security selection."
        rm -f "${TEMP_WPA_CONF}"
        exit 1
        ;;
esac


install -m 0600 "${TEMP_WPA_CONF}" "${WPA_CONF}"
rm -f "${TEMP_WPA_CONF}"

cleanup_password


print_info "Created:"
echo "  ${WPA_CONF}"


# -----------------------------------------------------------------------------
# Configure systemd-networkd DHCP
# -----------------------------------------------------------------------------

mkdir -p "${NETWORKD_DIR}"

cat > "${NETWORK_CONF}" <<EOF
[Match]
Name=${WIFI_IF}

[Network]
DHCP=yes
EOF

chmod 644 "${NETWORK_CONF}"


print_info "Created:"
echo "  ${NETWORK_CONF}"


# -----------------------------------------------------------------------------
# Check wpa_supplicant template service
# -----------------------------------------------------------------------------

if ! systemctl cat wpa_supplicant@.service >/dev/null 2>&1; then

    print_error "wpa_supplicant@.service does not exist."

    echo
    echo "The configuration files have been created, but the"
    echo "wpa_supplicant systemd service could not be enabled."

    exit 1
fi


# -----------------------------------------------------------------------------
# Enable persistent services
# -----------------------------------------------------------------------------

print_info "Enabling Wi-Fi services..."

systemctl daemon-reload

systemctl enable systemd-networkd.service

systemctl enable "wpa_supplicant@${WIFI_IF}.service"


# -----------------------------------------------------------------------------
# Restart networking
# -----------------------------------------------------------------------------

print_info "Connecting to '${SSID}'..."

systemctl restart systemd-networkd.service

systemctl restart "wpa_supplicant@${WIFI_IF}.service"


# -----------------------------------------------------------------------------
# Wait for association
# -----------------------------------------------------------------------------

CONNECTED=0

for _ in $(seq 1 30); do

    if iw dev "${WIFI_IF}" link |
       grep -q "^Connected to"; then

        CONNECTED=1
        break
    fi

    sleep 1
done


if [ "${CONNECTED}" -ne 1 ]; then

    print_error "Wi-Fi association failed."

    echo
    echo "Service status:"
    systemctl --no-pager --full status \
        "wpa_supplicant@${WIFI_IF}.service" || true

    echo
    echo "Recent logs:"
    journalctl \
        -u "wpa_supplicant@${WIFI_IF}.service" \
        --no-pager \
        -n 20 || true

    exit 1
fi


# -----------------------------------------------------------------------------
# Wait for DHCP address
# -----------------------------------------------------------------------------

print_info "Wi-Fi associated. Waiting for DHCP address..."

IP_ADDRESS=""

for _ in $(seq 1 30); do

    IP_ADDRESS="$(
        ip -4 -o addr show dev "${WIFI_IF}" |
            awk '{print $4}' |
            head -n 1
    )"

    if [ -n "${IP_ADDRESS}" ]; then
        break
    fi

    sleep 1
done


# -----------------------------------------------------------------------------
# Final status
# -----------------------------------------------------------------------------

echo
echo "============================================================"
echo " Wi-Fi setup completed"
echo "============================================================"
echo

iw dev "${WIFI_IF}" link

echo

if [ -n "${IP_ADDRESS}" ]; then

    echo "Interface : ${WIFI_IF}"
    echo "SSID      : ${SSID}"
    echo "IP address: ${IP_ADDRESS}"
    echo

    echo "Wi-Fi will now connect automatically after reboot."

else

    echo "Wi-Fi is associated with '${SSID}',"
    echo "but no IPv4 address was received yet."
    echo
    echo "Check:"
    echo "  networkctl status ${WIFI_IF}"
    echo "  journalctl -u systemd-networkd"
fi

echo