idf_component_register(
    SRCS "main.c" "libraries/LT_SPI/LT_SPI.c"
    	 "libraries/LTC681x/LTC681x.c" "libraries/LTC6813/LTC6813.c"
    	 "libraries/UserInterface/UserInterface.c"
    INCLUDE_DIRS "." "libraries/Linduino/" "libraries/LT_SPI/"
    	 "libraries/LTC681x/" "libraries/LTC6813/" "libraries/UserInterface/"
    PRIV_INCLUDE_DIRS "../../../../src"
    REQUIRES esp_driver_twai esp_timer)
