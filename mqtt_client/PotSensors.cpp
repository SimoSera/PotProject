/*
    For documentation on the ENS160 check https://www.sciosense.com/wp-content/uploads/2023/12/ENS160-Datasheet.pdf 
        and https://github.com/sciosense/ens16x-arduino/tree/main (the library, specifically check /src/ens16x.h)


    


*/



#include "PotSensors.h"
#include <ScioSense_ENS16x.h> // ENS160 library https://github.com/sciosense/ens16x-arduino/tree/main




PotSensors::PotSensors(uint8_t LIGHT_PIN, uint8_t SOIL_MOISTURE_PIN, unsigned int ENS160_I2C_ADDRESS, bool verbose ) {
    this->LIGHT_PIN = LIGHT_PIN;
    this->SOIL_MOISTURE_PIN = SOIL_MOISTURE_PIN;
    this->ENS160_I2C_ADDRESS = ENS160_I2C_ADDRESS;
    this->verbose = verbose;
    ens160=new ENS160();
}

void PotSensors::init() {
    if (verbose) {
        Serial.begin(9600);
        this->ens160->enableDebugging(Serial);            // Enables debugging for the esn160 on serial
    }


    Wire.begin();
    this->ens160->begin(&Wire, this->ENS160_I2C_ADDRESS);  // Starts the I2C communication with the ENS160

    while (this->ens160->init() != true)  // Initialization of the ENS160 sensor
    {
        if (verbose) {
            Serial.print(".");
        }
        delay(100);
    }
    if (verbose) {
        Serial.println("success");
    }


    this->ens160->enableAutoDataIntegrityValidation(true);   // Uses a checksum to check if the readings are correct
    this->ens160->startStandardMeasure();                // Starts the sensor

    if (!this->aht.begin()) {              // Initialization of the AHT20 sensor
        if (verbose) {
            Serial.println("Could not find AHT? Check wiring");
        }
        while (1) delay(10);
    }
    aht.getEvent(&this->humidity, &this->temp);   // Reads the first values from the AHT20 sensor
    this->last_aht = millis();                  // Initialize the timer
    if (verbose) {
        Serial.println("AHT20 found");
        Serial.println("All sensors found and setup, ready to start.");
    }
}

int PotSensors::getSoilMoisture() const{
    return 100-((analogRead(this->SOIL_MOISTURE_PIN)-SOIL_MOISTURE_MIN_VAL)*100/SOIL_MOISTURE_MAX_VAL);
}


int PotSensors::getLightLevel() const{
    return int((analogRead(this->LIGHT_PIN))*LIGHT_SENSOR_VOLTAGE/4095.0*200.0);
}


int PotSensors::getTemperature(){
    if (millis() - this->last_aht > AHT_INTERVAL) {   // Update the humidity / temperature only if AHT_INTERVAL time has elapsed
        aht.getEvent(&this->humidity, &this->temp);
        this->last_aht = millis();
    }
    return this->temp.temperature;
}


int PotSensors::getHumidity(){ 
    if (millis() - this->last_aht > AHT_INTERVAL) {  // Update the humidity / temperature only if AHT_INTERVAL time has elapsed
        aht.getEvent(&this->humidity, &this->temp);
        this->last_aht = millis();
    }
    return this->humidity.relative_humidity;
}



//Air Quality Index (technically UBA but not actualy UBA as per specifications (UBA is for outdoor and uses different factors))
uint8_t PotSensors::getAQI(){
    if (ens160->update() == RESULT_OK) {   // Checks if the sensor was able to read
        if (this->ens160->hasNewData()) {    // If the sensor has new data --> update the AQI
            this->AQI = (uint8_t)ens160->getAirQualityIndex_UBA();
        }
        return AQI;
    }
    else {
        return 255;  //255 for when the sensor has no reading (either warming up or some very unusual measurements)
    }
       
}



// Sets OPMODE to DEEPSLEEP (<1mA), to use the sensor again you first need to reset it
void PotSensors::setENS160DeepSleep() {
    this->ens160->setOperatingMode(ENS16X_OPERATING_MODE_DEEP_SLEEP);
}



// Resets the EnS160, NEEDS A WARMUP PHASE
void PotSensors::setENS160Reset() {
    while (this->ens160->init() != true)
    {
        delay(100);
    }
}
