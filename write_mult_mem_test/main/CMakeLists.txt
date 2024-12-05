idf_component_register(SRCS "write_mem_stm.c"
                    INCLUDE_DIRS "")
spiffs_create_partition_image(storage ../spiffs_test FLASH_IN_PROJECT)
