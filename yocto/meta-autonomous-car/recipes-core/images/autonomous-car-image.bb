SUMMARY = "STM32 Autonomous Car OpenSTLinux image"
DESCRIPTION = "OpenSTLinux image for the STM32MP257 autonomous vehicle platform"

require recipes-st/images/st-image-core.bb

CORE_IMAGE_EXTRA_INSTALL += " \
    packagegroup-autonomous-car \
"