idf_component_register(SRCS "ws_dsi_panel.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_lcd esp_driver_i2c esp_hw_support esp_lcd_touch
                       PRIV_REQUIRES esp_lcd_st7701 esp_lcd_ek79007 esp_lcd_touch_gt911)
