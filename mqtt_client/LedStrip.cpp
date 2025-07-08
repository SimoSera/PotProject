#include "LedStrip.h"
#include <algorithm>

    void LedStrip::update(int light_level){
        // auto brightness is a removed functionality
        if(auto_brightness_){
            brightness_=min(int((light_level-light_threshold_)*brightness_gamma_)+50,220);   // to revise !!!!!!!!!!!!
            strip_.setBrightness(brightness_);
        }
        last_light_level_=light_level;
        if(on_){
            if(light_level<=light_threshold_ && light_on_tresh_){
                light_on_tresh_=false;
                strip_.fill(0U);
                strip_.show();
                return;
            }else if(light_level>light_threshold_ && !light_on_tresh_){
                light_on_tresh_=true;
            }
            if(light_on_tresh_){
                if(breathing_!=0){
                    breathing_step();
                }else if(circling_){
                    circling_step();
                } else{
                    strip_.setBrightness(brightness_);
                    strip_.show();
                }
            }
        }else{
            strip_.fill(0U);
            strip_.show();  
        }
    }
    void LedStrip::turn_on(){
        on_=true;
        update(last_light_level_);
    }

    void LedStrip::turn_off(){
        on_=false;
        strip_.fill(0U);
        strip_.show();
    }

    void LedStrip::set_still(color_t still_color){
        effect_color_=still_color;
        strip_.fill(effect_color_);
        strip_.setBrightness(brightness_);
        update(last_light_level_);
    }

    void LedStrip::set_circling(color_t effect_color,uint8_t effect_speed){
        breathing_=0;
        static_=false;
        circling_=true;
        effect_color_=effect_color;
        effect_timestep_=0;
        effect_update_period_=int(200/effect_speed);
        update(last_light_level_);
    }

    void LedStrip::set_breathing(color_t effect_color,uint8_t effect_speed){
        circling_=false;
        static_=false;
        breathing_=1;
        effect_timestep_=0;
        effect_color_=effect_color;
        effect_update_period_=int(200/effect_speed);
        update(last_light_level_);
    }

    void  LedStrip::breathing_step(){
        if(millis()-timer_ > (effect_update_period_)){
        
            if(effect_timestep_>=brightness_ && breathing_==1){
                breathing_=2;
            }else if(effect_timestep_<5 && breathing_==2){
                breathing_=1;
            }
            strip_.fill(effect_color_);
            strip_.setBrightness(effect_timestep_);
            
            if(breathing_== 1){
                ++effect_timestep_;
            }else if(breathing_== 2){
                --effect_timestep_;
            }
            strip_.show();
            timer_=millis();
        }
    }
    void  LedStrip::circling_step(){
        if(millis()-timer_ > effect_update_period_){
            if(effect_timestep_>num_pixels_){
                effect_timestep_=0;
            }
            strip_.fill(effect_color_,effect_timestep_,1);
            strip_.setBrightness(brightness_);
            ++effect_timestep_;
            strip_.show();
            timer_=millis();
        }
    }

    // removed functionality
    void LedStrip::set_auto_brightness(bool on){
        auto_brightness_=on;
    }
    // removed functionality
    void LedStrip::set_brightness_gamma(float gamma){
        brightness_gamma_=gamma;
    }
    void LedStrip::set_light_threshold(int threshold){
        light_threshold_=threshold;
    }


    void LedStrip::set_brightness(uint8_t brightness){
        brightness_ = brightness;
    }