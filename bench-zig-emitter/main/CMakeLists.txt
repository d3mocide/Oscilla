idf_component_register(
    SRCS "zig_emitter.c"
    INCLUDE_DIRS "."
    REQUIRES ieee802154 nvs_flash driver
)
