idf_component_register(SRCS "w25qxx_cli.c"
                    INCLUDE_DIRS "."
                    REQUIRES w25qxx_driver fatfs)
