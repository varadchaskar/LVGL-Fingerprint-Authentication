/*
 * Author: Varad Chaskar
 * Purpose: This code demonstrates how to interface an Adafruit Fingerprint Sensor with an ESP32.
 * It initializes the sensor, reads its parameters, and attempts to capture and identify fingerprints.
 * The sensor's functionality is tested by printing status messages to the Serial Monitor.
 * Required Libraries:
 * - Adafruit_Fingerprint
 */

#include <Adafruit_Fingerprint.h>  // Include the Adafruit Fingerprint library for sensor functionality

// Define RX and TX pins for the fingerprint sensor
#define RX_PIN 25   // Set RX (Receive) pin to GPIO 25 (adjust based on wiring)
#define TX_PIN 33   // Set TX (Transmit) pin to GPIO 33 (adjust based on wiring)

// Create a HardwareSerial object for Serial2
HardwareSerial mySerial(2);  // Use Serial2 (UART2) on ESP32

// Create an instance of the Adafruit_Fingerprint class, passing in the Serial2 object
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

void setup() {
  Serial.begin(115200);                   // Initialize serial communication with baud rate 115200
  while (!Serial);                        // Wait until the Serial Monitor is connected
  delay(100);                             // Short delay to ensure the Serial Monitor is fully initialized
  
  Serial.println("\n\nAdafruit Fingerprint sensor test");  // Print a message indicating the start of the sensor test

  // Initialize Serial2 with the specified baud rate and pins
  mySerial.begin(57600, SERIAL_8N1, RX_PIN, TX_PIN);  // Begin communication with the fingerprint sensor at 57600 baud rate

  // Initialize the fingerprint sensor
  finger.begin(57600);                    // Begin communication with the fingerprint sensor at 57600 baud rate
  delay(5);                               // Small delay to ensure the sensor is ready
  
  if (finger.verifyPassword()) {          // Verify the sensor's password
    Serial.println("Found fingerprint sensor!");  // Print message if sensor is detected
  } 
  else {
    Serial.println("Did not find fingerprint sensor :(");  // Print message if sensor is not detected
    while (1) { delay(1); }               // Enter an infinite loop if the sensor is not found
  }

  // Read and print fingerprint sensor parameters
  Serial.println(F("Reading sensor parameters"));  // Print a message indicating parameter reading
  finger.getParameters();  // Retrieve the fingerprint sensor parameters

  Serial.print(F("Status: 0x")); 
  Serial.println(finger.status_reg, HEX);  // Print the sensor status register in hexadecimal
  Serial.print(F("Sys ID: 0x")); 
  Serial.println(finger.system_id, HEX);    // Print the system ID in hexadecimal
  Serial.print(F("Capacity: ")); 
  Serial.println(finger.capacity);          // Print the sensor capacity
  Serial.print(F("Security level: ")); 
  Serial.println(finger.security_level);    // Print the security level
  Serial.print(F("Device address: ")); 
  Serial.println(finger.device_addr, HEX);  // Print the device address in hexadecimal
  Serial.print(F("Packet len: ")); 
  Serial.println(finger.packet_len);       // Print the packet length
  Serial.print(F("Baud rate: ")); 
  Serial.println(finger.baud_rate);         // Print the baud rate

  // Check and print the number of fingerprint templates stored in the sensor
  finger.getTemplateCount();  // Retrieve the number of templates stored in the sensor

  if (finger.templateCount == 0) {  // Check if no templates are stored
    Serial.println("Sensor doesn't contain any fingerprint data. Please run the 'enroll' example.");  // Prompt to run the enrollment example if no data is present
  } 
  else {
    Serial.print("Sensor contains "); 
    Serial.print(finger.templateCount); 
    Serial.println(" templates");  // Print the number of templates stored
  }
}

void loop() {
  getFingerprintID();  // Call function to get and identify fingerprint
  delay(1000);         // Wait for 1 second before the next loop iteration
}

uint8_t getFingerprintID() {
  uint8_t p = finger.getImage();  // Capture an image from the fingerprint sensor

  switch (p) {  // Check the result of the image capture
    case FINGERPRINT_OK:
      Serial.println("Image taken");  // Print success message
      break;

    case FINGERPRINT_NOFINGER:
      Serial.println("No finger detected");  // Print message indicating no finger detected
      return p;  // Return error code

    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");  // Print communication error message
      return p;  // Return error code

    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");  // Print imaging error message
      return p;  // Return error code

    default:
      Serial.println("Unknown error");  // Print unknown error message
      return p;  // Return error code
  }

  p = finger.image2Tz();  // Convert the captured image to a template

  switch (p) {  // Check the result of image conversion
    case FINGERPRINT_OK:
      Serial.println("Image converted");  // Print success message
      break;

    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");  // Print message indicating the image is too messy
      return p;  // Return error code

    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");  // Print communication error message
      return p;  // Return error code

    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");  // Print message indicating features were not found
      return p;  // Return error code

    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");  // Print message indicating invalid image
      return p;  // Return error code

    default:
      Serial.println("Unknown error");  // Print unknown error message
      return p;  // Return error code
  }

  p = finger.fingerSearch();  // Search for a fingerprint match

  if (p == FINGERPRINT_OK) {
    Serial.println("Found a print match!");  // Print success message
  } 
  else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");  // Print communication error message
    return p;  // Return error code
  } 
  else if (p == FINGERPRINT_NOTFOUND) {
    Serial.println("Did not find a match");  // Print message indicating no match found
    return p;  // Return error code
  } 
  else {
    Serial.println("Unknown error");  // Print unknown error message
    return p;  // Return error code
  }

  // Print the fingerprint ID and confidence level if a match is found
  Serial.print("Found ID #"); 
  Serial.print(finger.fingerID);  // Print the ID of the matched fingerprint
  Serial.print(" with confidence of "); 
  Serial.println(finger.confidence);  // Print the confidence level of the match

  return finger.fingerID;  // Return the fingerprint ID of the match
}
