SUMMARY = "STM32 Autonoumous Car application package group"
DESCRIPTION = "Packages required by the STM32 autonomous car platform"
LICENSE = "MIT"

inherit packagegroup

PACKAGES = "${PN}"

RDEPENDS:${PN} = " \
    motor-control-gateway \
    wifi-tools \
"