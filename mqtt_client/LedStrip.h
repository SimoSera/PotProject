#ifndef LEDSTRIP_H
#define LEDSTRIP_H


#include <Adafruit_NeoPixel.h>          // Docs: https://adafruit.github.io/Adafruit_NeoPixel/html/class_adafruit___neo_pixel.html
#include <inttypes.h>

typedef uint32_t color_t;

struct LedStrip{

    uint8_t num_pixels_;
    uint8_t pin_;

    Adafruit_NeoPixel strip_;


    bool on_=false;
    // brightness definition
    uint32_t light_threshold_=0;
    uint32_t last_light_level_=1;
    float brightness_gamma_=0.1;
    bool auto_brightness_=true;
    uint8_t brightness_=255;

    //effects
    uint8_t breathing_=0;       
    bool circling_=false;
    bool static_=false;
    bool light_on_=false;
    bool light_on_tresh_=false;

    // colors
    color_t effect_color_=0U;


    //timings
    unsigned long timer_=0;       // timer used for the leds effects
    size_t effect_timestep_=0;                 // used during effects to track current step
    uint32_t effect_update_period_=20;  //in milliseconds 

  



    LedStrip(uint8_t num_pixels, uint8_t pin, int light_threshold=0,float brightness_gamma=0.1,bool auto_brightness=true):
                            num_pixels_(num_pixels),
                            pin_(pin),
                            strip_(num_pixels_, pin_, NEO_GRBW + NEO_KHZ800), 
                            light_threshold_(light_threshold),
                            auto_brightness_(auto_brightness),
                            brightness_gamma_(brightness_gamma)
    {
      strip_.begin();
    }

    void update(int light_level);
    void turn_on();
    void turn_off();
    void set_base_color(color_t base);
    void set_basic();
    void set_still(color_t still_color);
    void set_circling(color_t effect_color,uint8_t effect_speed=0);
    void set_breathing(color_t effect_color,uint8_t effect_speed=0);
    void set_auto_brightness(bool on);
    void set_brightness_gamma(float gamma);
    void set_light_threshold(int threshold);
    void breathing_step();
    void circling_step();
};


#endif