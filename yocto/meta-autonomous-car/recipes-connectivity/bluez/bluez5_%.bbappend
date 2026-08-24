do_install:append() {
    BLUEZ_CONF="${D}${sysconfdir}/bluetooth/main.conf"

    if [ -f "${BLUEZ_CONF}" ]; then

        # BlueZ normally already contains:
        # #AutoEnable=false
        #
        # Replace either commented or existing setting.
        if grep -q '^[[:space:]#]*AutoEnable=' "${BLUEZ_CONF}"; then
            sed -i \
                's/^[[:space:]#]*AutoEnable=.*/AutoEnable=true/' \
                "${BLUEZ_CONF}"

        # Fallback: [Policy] exists but AutoEnable is missing.
        elif grep -q '^\[Policy\]' "${BLUEZ_CONF}"; then
            sed -i \
                '/^\[Policy\]/a AutoEnable=true' \
                "${BLUEZ_CONF}"

        # Last fallback: add the Policy section.
        else
            printf '\n[Policy]\nAutoEnable=true\n' >> "${BLUEZ_CONF}"
        fi

    else
        install -d ${D}${sysconfdir}/bluetooth

        printf '[Policy]\nAutoEnable=true\n' > "${BLUEZ_CONF}"
    fi
}