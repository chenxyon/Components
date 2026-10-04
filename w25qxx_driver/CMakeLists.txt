idf_component_register(SRCS "w25qxx.c" "w25qxx_spi.c" "w25qxx_diskio.c"
                    INCLUDE_DIRS "."
                    REQUIRES driver fatfs)
