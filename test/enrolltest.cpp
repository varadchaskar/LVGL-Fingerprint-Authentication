/*
 * Author: Varad Chaskar
 * Purpose: This code demonstrates how to use an Adafruit Fingerprint Sensor with an ESP32. It allows enrolling new fingerprints, providing feedback on the enrollment process, and storing the fingerprint templates in the sensor's memory.
 * Required Libraries:
 * - Adafruit_Fingerprint
 * - FS.h
 * - SPI.h
 * - lvgl.h
 * - TFT_eSPI.h
 */

#include <Arduino.h>                // Include the core Arduino library
#include <Adafruit_Fingerprint.h>    // Include the Adafruit Fingerprint Sensor library
#include <FS.h>                     // Include the File System library (not used in this code but commonly used with ESP32)
#include <SPI.h>                    // Include the SPI library (not used directly here but included for completeness)
#include <lvgl.h>                  // Include the LVGL library for graphics (not used in this code but included for completeness)
#include <TFT_eSPI.h>              // Include the TFT_eSPI library for the TFT display (not used directly here but included for completeness)

// Use HardwareSerial for ESP32
HardwareSerial mySerial(1);       // Create a HardwareSerial object using UART1 (1 is the ID for UART1)

// Create an instance of the Adafruit_Fingerprint class
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial); // Pass the HardwareSerial object to the fingerprint sensor

uint8_t id;                       // Variable to store the fingerprint ID

void setup() {
  Serial.begin(9600);            // Initialize serial communication at 9600 baud rate
  while (!Serial);               // Wait for the serial port to connect (necessary for some environments)
  delay(100);                    // Delay to ensure serial port is ready
  Serial.println("\n\nAdafruit Fingerprint sensor enrollment"); // Print initial message to serial monitor

  // Set up hardware serial for fingerprint sensor on ESP32
  mySerial.begin(57600, SERIAL_8N1, 25, 33); // Initialize UART1 with 57600 baud rate, 8 data bits, 1 stop bit, and no parity. TX pin = 25, RX pin = 33

  // Initialize the fingerprint sensor
  finger.begin(57600);            // Begin communication with the fingerprint sensor at 57600 baud rate

  if (finger.verifyPassword()) {  // Verify if the fingerprint sensor is responding correctly
    Serial.println("Found fingerprint sensor!"); // Print message if sensor is found
  } 
  else {
    Serial.println("Did not find fingerprint sensor :("); // Print message if sensor is not found
    while (1) { delay(1); }     // Infinite loop if sensor is not found (prevent further execution)
  }

  // Read and print sensor parameters
  Serial.println(F("Reading sensor parameters")); // Print message indicating parameter reading
  finger.getParameters();           // Retrieve and display fingerprint sensor parameters
  Serial.print(F("Status: 0x")); 
  Serial.println(finger.status_reg, HEX); // Print status register in hexadecimal format
  Serial.print(F("Sys ID: 0x")); 
  Serial.println(finger.system_id, HEX);   // Print system ID in hexadecimal format
  Serial.print(F("Capacity: ")); 
  Serial.println(finger.capacity);         // Print sensor capacity
  Serial.print(F("Security level: ")); 
  Serial.println(finger.security_level); // Print security level
  Serial.print(F("Device address: ")); 
  Serial.println(finger.device_addr, HEX); // Print device address in hexadecimal format
  Serial.print(F("Packet len: ")); 
  Serial.println(finger.packet_len);     // Print packet length
  Serial.print(F("Baud rate: ")); 
  Serial.println(finger.baud_rate);       // Print baud rate
}

uint8_t readnumber(void) {
  uint8_t num = 0;                // Initialize num variable to 0

  while (num == 0) {             // Loop until a valid number is read
    while (!Serial.available()); // Wait until data is available in the serial buffer
    num = Serial.parseInt();     // Parse an integer from the serial buffer
  }
  return num;                    // Return the parsed number
}

void loop() {
  Serial.println("Ready to enroll a fingerprint!"); // Print message indicating readiness for fingerprint enrollment
  Serial.println("Please type in the ID # (from 1 to 127) you want to save this finger as..."); // Prompt user for ID
  id = readnumber();            // Read the ID from the serial monitor
  if (id == 0) {               // Check if ID is 0 (invalid)
    return;                    // Exit the function if ID is invalid
  }
  Serial.print("Enrolling ID #"); 
  Serial.println(id);         // Print the ID being enrolled

  while (!getFingerprintEnroll()); // Call the function to start fingerprint enrollment and wait until successful
}

uint8_t getFingerprintEnroll() {
  int p = -1;                   // Initialize result variable to -1

  Serial.print("Waiting for valid finger to enroll as #"); 
  Serial.println(id); // Prompt user to place their finger
  while (p != FINGERPRINT_OK) { // Loop until a valid fingerprint image is obtained
    p = finger.getImage();      // Capture image from the fingerprint sensor
    switch (p) {                // Check the result of image capture
    case FINGERPRINT_OK:        // If image capture is successful
      Serial.println("Image taken"); // Print success message
      break;
    case FINGERPRINT_NOFINGER: // If no finger is detected
      Serial.print(".");      // Print a dot as a progress indicator
      break;
    case FINGERPRINT_PACKETRECIEVEERR: // If there is a communication error
      Serial.println("Communication error"); // Print error message
      break;
    case FINGERPRINT_IMAGEFAIL: // If there is an imaging error
      Serial.println("Imaging error"); // Print error message
      break;
    default:                     // For any other unknown errors
      Serial.println("Unknown error"); // Print unknown error message
      break;
    }
  }

  p = finger.image2Tz(1);     // Convert the captured image to template 1
  switch (p) {                // Check the result of image conversion
    case FINGERPRINT_OK:      // If conversion is successful
      Serial.println("Image converted"); // Print success message
      break;
    case FINGERPRINT_IMAGEMESS: // If the image is too messy
      Serial.println("Image too messy"); // Print error message
      return p;              // Return error code
    case FINGERPRINT_PACKETRECIEVEERR: // If there is a communication error
      Serial.println("Communication error"); // Print error message
      return p;              // Return error code
    case FINGERPRINT_FEATUREFAIL: // If fingerprint features are not found
      Serial.println("Could not find fingerprint features"); // Print error message
      return p;              // Return error code
    case FINGERPRINT_INVALIDIMAGE: // If the image is invalid
      Serial.println("Could not find fingerprint features"); // Print error message
      return p;              // Return error code
    default:                     // For any other unknown errors
      Serial.println("Unknown error"); // Print unknown error message
      return p;              // Return error code
  }

  Serial.println("Remove finger"); // Prompt user to remove their finger
  delay(2000);                    // Wait for 2 seconds
  p = 0;                         // Initialize result variable to 0
  while (p != FINGERPRINT_NOFINGER) { // Wait until the finger is removed
    p = finger.getImage();      // Capture image to check if the finger is still present
  }

  Serial.print("ID "); 
  Serial.println(id); // Print the ID being enrolled
  p = -1;                          // Initialize result variable to -1
  Serial.println("Place same finger again"); // Prompt user to place the same finger again
  while (p != FINGERPRINT_OK) { // Loop until a valid fingerprint image is obtained
    p = finger.getImage();      // Capture image from the fingerprint sensor
    switch (p) {                // Check the result of image capture
    case FINGERPRINT_OK:        // If image capture is successful
      Serial.println("Image taken"); // Print success message
      break;
    case FINGERPRINT_NOFINGER: // If no finger is detected
      Serial.print(".");      // Print a dot as a progress indicator
      break;
    case FINGERPRINT_PACKETRECIEVEERR: // If there is a communication error
      Serial.println("Communication error"); // Print error message
      break;
    case FINGERPRINT_IMAGEFAIL: // If there is an imaging error
      Serial.println("Imaging error"); // Print error message
      break;
    default:                     // For any other unknown errors
      Serial.println("Unknown error"); // Print unknown error message
      break;
    }
  }

  p = finger.image2Tz(2);     // Convert the captured image to template 2
  switch (p) {                // Check the result of image conversion
    case FINGERPRINT_OK:      // If conversion is successful
      Serial.println("Image converted"); // Print success message
      break;
    case FINGERPRINT_IMAGEMESS: // If the image is too messy
      Serial.println("Image too messy"); // Print error message
      return p;              // Return error code
    case FINGERPRINT_PACKETRECIEVEERR: // If there is a communication error
      Serial.println("Communication error"); // Print error message
      return p;              // Return error code
    case FINGERPRINT_FEATUREFAIL: // If fingerprint features are not found
      Serial.println("Could not find fingerprint features"); // Print error message
      return p;              // Return error code
    case FINGERPRINT_INVALIDIMAGE: // If the image is invalid
      Serial.println("Could not find fingerprint features"); // Print error message
      return p;              // Return error code
    default:                     // For any other unknown errors
      Serial.println("Unknown error"); // Print unknown error message
      return p;              // Return error code
  }

  Serial.println("Remove finger"); // Prompt user to remove their finger
  delay(2000);                    // Wait for 2 seconds
  p = 0;                         // Initialize result variable to 0
  while (p != FINGERPRINT_NOFINGER) { // Wait until the finger is removed
    p = finger.getImage();      // Capture image to check if the finger is still present
  }

  Serial.println("Enrolling..."); // Print message indicating enrollment process
  p = finger.createModel();       // Create a fingerprint model from the captured templates
  if (p == FINGERPRINT_OK) {     // Check if enrollment is successful
    Serial.println("Finger enrolled successfully"); // Print success message
    return true;               // Return true indicating successful enrollment
  } 
  else {                     // If enrollment fails
    Serial.print("Error code: "); 
    Serial.println(p);        // Print error code
    return false;              // Return false indicating failure
  }
}
