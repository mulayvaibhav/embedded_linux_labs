SUMMARY = "STM32Car Linux gateway application"
DESCRIPTION = "Gateway application running on STM32MP257 for communication with the STM32 motor controller"
LICENSE = "CLOSED"

DEPENDS = "glib-2.0"

inherit externalsrc systemd pkgconfig

#
# Application source is kept outside the Yocto layer.
#
EXTERNALSRC = "${AUTONOMOUS_CAR_SRC_ROOT}/linux-apps/motor-control-gateway"

#
# Current Makefile builds inside the source directory.
#
EXTERNALSRC_BUILD = "${EXTERNALSRC}"

EXTERNALSRC_SYMLINKS = ""

S = "${EXTERNALSRC}"
B = "${EXTERNALSRC_BUILD}"

EXTRA_OEMAKE += "UART_COM_ENABLED=1"

#
# Compile application using Yocto cross compiler.
#
do_compile() {
    oe_runmake \
        CC="${CC}" \
        CPPFLAGS="${CPPFLAGS}" \
        CFLAGS="${CFLAGS}" \
        LDFLAGS="${LDFLAGS}"
}

#
# Install application and systemd service into target root filesystem.
#
do_install() {
    # Install application binary
    install -d ${D}${bindir}

    install -m 0755 \
        ${B}/build/motor-control-gateway \
        ${D}${bindir}/motor-control-gateway

    # Install systemd service
    install -d ${D}${systemd_system_unitdir}

    install -m 0644 \
        ${S}/systemd/motor-control-gateway.service \
        ${D}${systemd_system_unitdir}/motor-control-gateway.service
}

SYSTEMD_SERVICE:${PN} = "motor-control-gateway.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"