idf_component_register(
    SRCS
        "main.c"
        "signal_gen.c"
        "dsp_pipeline.c"
        "usb_cdc.c"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_usb_serial_jtag esp_timer
)
