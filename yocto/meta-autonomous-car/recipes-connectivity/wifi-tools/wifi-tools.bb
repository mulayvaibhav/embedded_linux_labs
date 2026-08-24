SUMMARY = "Wi-Fi provisioning utility for the autonomous car"
DESCRIPTION = "Interactive Wi-Fi setup script for first-boot provisioning"
LICENSE = "CLOSED"

SRC_URI = "file://setup-wifi.sh"

S = "${WORKDIR}"

RDEPENDS:${PN} += " \
    bash \
    iw \
    iproute2 \
    wpa-supplicant \
"

do_install() {
    install -d ${D}${sbindir}

    install -m 0755 \
        ${WORKDIR}/setup-wifi.sh \
        ${D}${sbindir}/setup-wifi
}

FILES:${PN} += " \
    ${sbindir}/setup-wifi \
"