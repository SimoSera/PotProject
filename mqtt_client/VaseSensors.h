#ifndef POT_SENSORS_H
#define	POT_SENSORS_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_AHTX0.h> //AHT sensor
class ENS160;
#include <inttypes.h>

#define AHT_INTERVAL 30000 //   time interval (in milliseconds) after which the aht sensor sould actually update values
                            // I chose to define this as the temperature/humidity shouldn't change fast and reading the sensor uses resources


class PotSensors
{
private:
    uint8_t LIGHT_PIN;                  // analog pin for the light sensor
    uint8_t SOIL_MOISTURE_PIN;          // analog pin for the soil moisture capacitive sensor
    unsigned int ENS160_I2C_ADDRESS;    // I2C address for the ENS160 sensor
    bool verbose;
    ENS160*  ens160;                     // ENS160 Object from the dedicated library (air quality)
    Adafruit_AHTX0 aht;                 // AHT20 object from it's library (humidity and temperature)
    sensors_event_t humidity, temp;    // Used by the AHT20 sensor to get values
    uint8_t AQI;
    time_t last_aht;                    // Reading the aht with frequency higher than 5 seconds doesn't make sense
    
public:
    PotSensors(uint8_t LIGHT_PIN,          // PIN of the Analog light sensor
        uint8_t SOIL_MOISTURE_PIN,          // PIN of the Analog soil moisture sensor
        unsigned int ENS160_I2C_ADDRESS = 0x53 /* 0x53 or 0x52*/,  // I2C address of the ENS160 sensor
        bool verbose=true);               // True = print to serial

public:
    void init();            // Initialization of all the sensors
    int getSoilMoisture()const;     // Returns the Soil Moisture reading from the analog Capacitive soil moisture sensor
    int getLightLevel()const;   // Returns the Light level reading from the analog light sensor
    int getTemperature();      // Returns the Temperature reding from the AHT20 sensor
    int getHumidity();      // Returns the relative humidity read from the AHT20 sensor
    
    uint8_t getAQI();    //ENS160 Air Quality Index (technically UBA but not actualy UBA as per specifications (UBA is for outdoor and uses different factors))
                        // ATTENTION, returns 255 if the value cannot be read (most likely the sensor is in warmup phase or the sensor can't get a valid reading)
    
    void setENS160DeepSleep();  // Sets ENS160 OPMODE to DEEPSLEEP (<1mA), to use the sensor again you first need to reset it
    
    void setENS160Reset(); // Resets the EnS160, ATTENTIONS THIS is like a new startup, so the sensor NEEDS A WARMUP PHASE OF 3 MINUTES BEFORE READING


};



#endif // !POT_SENSORS_H

