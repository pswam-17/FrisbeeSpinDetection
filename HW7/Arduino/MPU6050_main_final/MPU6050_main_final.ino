#include <math.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include "U8x8lib.h" 
#include "Wire.h"

U8X8_SSD1306_128X32_UNIVISION_HW_I2C oled(U8X8_PIN_NONE);

// Clear the display no more than once per second
const int MAX_REFRESH = 1000;
unsigned long lastClear = 0;

Adafruit_MPU6050 mpu;

const int MPU_addr=0x68;
int16_t AcX,AcY,AcZ,Tmp,GyX,GyY,GyZ;
 
int minVal=265;
int maxVal=402;
 
double angle_x;
double angle_y;
double angle_z;

// parameters for calculating spin
double abs_spin = 0;
double max_spin = 0;
double rad_rpmconvert = 9.549297; // conversion from rad/sec to rpm

// parameters for calculating angle
int angle_state = 0;

double sqrt_mag = 0;
double tilt_angle = 0;
double tilt_angle_degrees = 0;
double ax = 0;
double ay = 0;
double az = 0;

//countdown and sampling timer
unsigned long timer = 6; //counter
unsigned long countdown = 6;
unsigned long sampling = 2; //set the sampling time

unsigned long cd; //countdown timer
unsigned long cd1; //countdown timer

unsigned long cdt;
unsigned long cdt1;

//final results variables
double max_spin_final = 0;
double angle_display = 0; //sample numbers
int i = 0; //used to loop through the array that stores the angle values
int j = 0;
int state = 0; //used to stop the average angle counting function

// variables for calculating wind up
int sampleInterval = 10;  // in milliseconds 
int sampleRate = 1000 / sampleInterval;  // can edit the sample rate (1 second / delay time per sample (sampleInterval))
int motionDetect = 0;
int windDetect = 0;

double deltatime = 10 / (double) sampleRate; // adjust with our sample rate

double windMag = 0;
double previous_windMag = 0;
double derivative_mag = 0;
double previous_derivative = 0;

unsigned long before = millis();

// variables for sensor sampling function
unsigned long sampleDelay = 1e6/sampleRate; //Time(μs) between samples
unsigned long timeStart = 0;              //Start time timing variable
unsigned long timeEnd = 0;                  //End time timing variable
int sampleTime = 0; // Time of last sample

void setup() {
  // put your setup code here, to run once:
  setupDisplay();
  Serial.begin(9600);

  Wire.begin();
  Wire.beginTransmission(MPU_addr);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);

	if (!mpu.begin()) {
		Serial.println("Failed to find MPU6050 chip");
		while (1) {
		  delay(10);
		}
	}
	Serial.println("MPU6050 Found!");

	// set accelerometer range to +-8G
	mpu.setAccelerometerRange(MPU6050_RANGE_8_G);

	// set gyro range to +- 2000 deg/s
	mpu.setGyroRange(MPU6050_RANGE_2000_DEG);

	// set filter bandwidth to 21 Hz
	mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  //initialize the timers
  cd1 = millis();
  cdt1 = millis();
}

void loop() {
  // put your main code here, to run repeatedly:

  cd = millis(); //start the countdown timer
  cdt = millis();

  if(timer>0){
    if(cd - cd1 >= 1000){ //check if a second has passed
      timer = timer - 1;
      cd1 = cd; //reset the timing
    }
    if(cdt - cdt1 >= 1000){
      if(countdown > 0){
        countdown = countdown - 1;
        Serial.print("Ready in: ");
        Serial.println(countdown);
        String countdown_s = String(countdown);
        String countdown_statement = "Ready in: " + countdown_s;
        writeDisplay(countdown_statement.c_str(), 0, true);
      }
      cdt1 = cdt; //reset the countdown display time
    }
  }
  else{
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);

    // gets angle from accelerometer
    Wire.beginTransmission(MPU_addr);
    Wire.write(0x3B);
    Wire.endTransmission(false);
    Wire.requestFrom(MPU_addr,14,true);
    AcX=Wire.read()<<8|Wire.read();
    AcY=Wire.read()<<8|Wire.read();
    AcZ=Wire.read()<<8|Wire.read();
    int xAng = map(AcX,minVal,maxVal,-90,90);
    int yAng = map(AcY,minVal,maxVal,-90,90);
    int zAng = map(AcZ,minVal,maxVal,-90,90);
 
    angle_x = RAD_TO_DEG * (atan2(-yAng, -zAng)+PI);  // changes when rotating the frisbee
    angle_y = RAD_TO_DEG * (atan2(-xAng, -zAng)+PI);  // changes when rotating the frisbee
    angle_z = RAD_TO_DEG * (atan2(-yAng, -xAng)+PI);  // changes when spinning

    double spin_rpm_z = rad_rpmconvert * g.gyro.z;

    // main code for detection of wind up
    if(windDetect == 0){
      unsigned long now = millis();
      if(sampleSensors() && Serial.availableForWrite()){
        unsigned long now = millis();

        if(now - before >= 10 / deltatime){ // adjusted to help with motion calculation 
          // calculates the derivative process
          windMag = sqrt(sq(ax) + sq(ay) + sq(az));
          derivative_mag = (windMag - previous_windMag) / deltatime;
          Serial.println(derivative_mag);

          previous_windMag = windMag;
          before = now;
        }
        if(derivative_mag > 30 || derivative_mag < -30){  // detects winding up motion 
          motionDetect = 1;
          windDetect = 1;
          before = millis();
        }
      }
    }
    else if(windDetect == 1){
      // main code for detecting maximum spin
      if(spin_rpm_z < 0){
        abs_spin = -1 * spin_rpm_z;
      }
      else{
        abs_spin = spin_rpm_z;
      }

      if(max_spin <= abs_spin){
        max_spin = abs_spin;
      }

      // main code for detecting angle
      ax = a.acceleration.x;
      ay = a.acceleration.y;
      az = a.acceleration.z; 

      if(angle_state == 0){ // to calculate the instant angle of throw (using tilt angle calculation)
        unsigned long now = millis();
        if(now - before >= 2000){
          sqrt_mag = sqrt(sqrt(sq(ax) + sq(ay))+ sq(az)); // calculates the vector magitude on all three axises
          tilt_angle = asin(az / sqrt_mag); 
          tilt_angle_degrees = tilt_angle * (180 / M_PI);
          angle_display = tilt_angle_degrees + 90;
          angle_state = 1;
          delay(1500);
        }
      }
      else{
        if(sampling > 0){
          max_spin_final = max_spin; //update the max spin final value
          if(cd - cd1 >= 3000){//check sampling time
            sampling = sampling - 1;
            cd1 = cd;
            //Print to serial monitior for testing
            /*
            Serial.print("max spin:");
            Serial.print(max_spin);
	          Serial.println("rpm"); 
            */
            Serial.print("angle:");
            Serial.print(tilt_angle_degrees);
            Serial.println("degrees");
          }
        // Code for OLED Display
          String maxspin_string = String(max_spin);
          String spin_statement = "max spin: " + maxspin_string;
          writeDisplay(spin_statement.c_str(), 0, true);
        }
        else if(sampling == 0){ //sampling time finishes
          if(state == 0){
            //Print to serial monitior for testing
            Serial.print("max spin: ");
            Serial.print(max_spin);
	          Serial.println("rpm");
            Serial.print("Angle: ");
            Serial.print(angle_display);
            Serial.println("degrees");
          }
          state = 1;
      
          //show the final results on the OLED
          String maxspin_string = String(max_spin_final);
          String spin_statement = "max spin: " + maxspin_string;
          writeDisplay(spin_statement.c_str(), 0, true);
          String angle_string = String(angle_display); 
          String angle_statement = "Angle: " + angle_string;
          writeDisplay(angle_statement.c_str(), 1, false);
        }
      }
    }
  }
}


// code below is for display 

void setupDisplay() {   
    oled.begin(); // Initializes u8x8 object
    oled.setPowerSave(0); // Makes sure OLED doesn't go to sleep
    oled.setFont(u8x8_font_amstrad_cpc_extended_r); // Set the font
    oled.setCursor(0, 0); //Sets the cursor at the top left corner
}

/*
 * A function to write a message on the OLED display.
 * The “row” argument specifies which row to print on: [0,1,2,3].
 * The “erase” argument specifies if the display should be cleared.
 */
void writeDisplay(const char * message, int row, bool erase) {
    unsigned long now = millis();
    //if erase is true and it's been longer than MAX_REFRESH (1s)
    if(erase && (millis() - lastClear >= MAX_REFRESH)) {
        oled.clearDisplay();
        lastClear = now;
    }
    oled.setCursor(0, row);
    oled.print(message);
}

void writeDisplayCSV(String message, int commaCount) {
     int startIndex = 0;
     for(int i=0; i<=commaCount; i++) {
          // find the index of the comma and store it in startIndex
          int index = message.indexOf(',', startIndex);           

          // take everything in the string up until the comma
          String subMessage = message.substring(startIndex, index); 

          startIndex = index + 1; // skip over the comma

          // Write the substring onto the OLED!
          writeDisplay(subMessage.c_str(), i, false);
     }
}

// code below is for sampling (functions specifically used for wind up detection)
bool sampleSensors() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  timeEnd = micros();
  if(timeEnd - timeStart >= sampleDelay) {
    timeStart = timeEnd;

    // Read the sensors and store their outputs in global variables
    sampleTime = millis();
    ax = a.acceleration.x;
    ay = a.acceleration.y;
    az = a.acceleration.z;
         // values stored in "ax", "ay", and "az"
    return true;
  }

  return false;
}
