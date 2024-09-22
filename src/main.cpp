/* 
Author: Varad Chaskar

Description: This program integrates an LVGL-based graphical interface on a TFT display and a fingerprint sensor using ESP32. 
The system allows users to scan or enroll fingerprints, manage a touch-based UI, and calibrate the screen. It utilizes a 
fingerprint sensor to enroll and scan fingerprints and updates the display with the current status of each operation. 
The project involves various event-driven tasks, such as fingerprint scanning, enrolling, and displaying results on the screen.
*/

#include <Arduino.h>               // Core library for Arduino framework
#include <FS.h>                    // Filesystem support for ESP32
#include <SPI.h>                   // Serial Peripheral Interface (SPI) library
#include <lvgl.h>                  // LittlevGL graphics library for the display
#include <TFT_eSPI.h>              // TFT display library for eSPI interface
#include <Adafruit_Fingerprint.h>  // Library for interfacing with the fingerprint sensor

// Pins for Fingerprint Sensor and LVGL Display
#define RX_PIN 25   // RX pin for fingerprint sensor communication
#define TX_PIN 33   // TX pin for fingerprint sensor communication
#define TOUCH_CS 21 // Chip select pin for touch functionality

// TFT and Fingerprint configurations
TFT_eSPI tft = TFT_eSPI();         // Creating an instance of the TFT display
HardwareSerial mySerial(2);        // Defining a serial object for fingerprint sensor (uses Serial2)
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial); // Initializing fingerprint sensor with serial communication

// Screen resolution constants
static const uint32_t screenWidth = 320;  // Screen width in pixels
static const uint32_t screenHeight = 240; // Screen height in pixels

static lv_disp_draw_buf_t draw_buf; // LVGL draw buffer for display updates
static lv_color_t buf[screenWidth * 10]; // Color buffer for drawing display content

// Global objects for UI elements
lv_obj_t *fingerLabel;     // Label to display fingerprint status messages
lv_obj_t *scanButton;      // Button to initiate fingerprint scanning
lv_obj_t *enrollButton;    // Button to start fingerprint enrollment
lv_obj_t *inputTextArea;   // Text area for entering fingerprint ID during enrollment
lv_obj_t *keyboard;        // Virtual keyboard for ID input
lv_obj_t *idLabel;         // Label to display entered ID
lv_obj_t *returnButton;    // Button to return to the main menu

uint8_t id = 0;  // Fingerprint ID to be enrolled

// Flags for modes
bool enrollingMode = false; // True when enrollment is active
bool scanningMode = false;  // True when scanning is active

/* Touch calibration function */
void touch_calibrate() {
  uint16_t calData[5];  // Calibration data array
  uint8_t calDataOK = 0; // Flag to check if calibration data exists

  if (!SPIFFS.begin()) {   // Start the SPI file system
    Serial.println("Formatting file system"); // Log formatting operation
    SPIFFS.format();        // Format the SPIFFS if unavailable
    SPIFFS.begin();         // Restart SPIFFS
  }

  // Check if calibration data exists in the file system
  if (SPIFFS.exists("/TouchCalData3")) {
    File f = SPIFFS.open("/TouchCalData3", "r"); // Open calibration file
    if (f) {
      if (f.readBytes((char *)calData, 14) == 14) calDataOK = 1; // Check if file is valid
      f.close(); // Close the file
    }
  }

  if (calDataOK) {
    tft.setTouch(calData);  // Apply calibration data to the touch sensor
  } else {
    // Perform touch calibration if no data exists
    tft.fillScreen(TFT_BLACK); // Fill screen with black color
    tft.setCursor(20, 0);      // Set cursor position for text
    tft.setTextFont(2);        // Set font size
    tft.setTextSize(1);        // Set text size
    tft.setTextColor(TFT_WHITE, TFT_BLACK); // Set text color (white on black)
    tft.println("Touch corners as indicated"); // Prompt user to calibrate

    tft.calibrateTouch(calData, TFT_MAGENTA, TFT_BLACK, 15); // Start calibration process
    File f = SPIFFS.open("/TouchCalData3", "w"); // Open a file to store calibration data
    if (f) {
      f.write((const unsigned char *)calData, 14); // Write calibration data to the file
      f.close(); // Close the file
    }
  }
}

/* Function to flush display content to the screen */
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1); // Calculate width of the area to update
  uint32_t h = (area->y2 - area->y1 + 1); // Calculate height of the area to update

  tft.startWrite(); // Start writing to the TFT display
  tft.setAddrWindow(area->x1, area->y1, w, h); // Set the area of the screen to update
  tft.pushColors((uint16_t *)&color_p->full, w * h, true); // Push the color data to the screen
  tft.endWrite(); // End the writing process

  lv_disp_flush_ready(disp); // Inform LVGL that flushing is done
}

/* Touchpad input handler for LVGL */
void lvgl_port_tp_read(lv_indev_drv_t *indev, lv_indev_data_t *data) {
  uint16_t touchX, touchY;   // Variables to store touch coordinates
  bool touched = tft.getTouch(&touchX, &touchY); // Get touch status and coordinates

  if (!touched) {
    data->state = LV_INDEV_STATE_REL; // Set state to released if no touch detected
  } else {
    data->state = LV_INDEV_STATE_PR;  // Set state to pressed if touch detected
    data->point.x = touchX;           // Set X coordinate of touch point
    data->point.y = touchY;           // Set Y coordinate of touch point
  }
}

/* Function to handle fingerprint scanning */
void scanFingerprint() {
  uint8_t fingerprintID = getFingerprintID(); // Get the scanned fingerprint ID
  switch (fingerprintID) {
    case FINGERPRINT_NOFINGER: // No finger detected
      lv_label_set_text(fingerLabel, "No Finger Detected"); // Update display label
      Serial.println("No Finger Detected"); // Print message to serial monitor
      break;
    case FINGERPRINT_NOTFOUND: // Fingerprint not found
      lv_label_set_text(fingerLabel, "No Match Found"); // Update display label
      Serial.println("No Match Found"); // Print message to serial monitor
      break;
    default: // Fingerprint matched with an ID
      if (fingerprintID >= 0) {
        String msg = "Fingerprint ID: " + String(fingerprintID); // Construct message
        lv_label_set_text(fingerLabel, msg.c_str()); // Update label with ID
        Serial.println(msg); // Print ID message to serial monitor
      }
      break;
  }
}

/* Event handler for the Return button */
void return_button_event_handler(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e); // Get the event code

  if (code == LV_EVENT_CLICKED) { // If return button is clicked
    Serial.println("Return button clicked."); // Print message to serial monitor

    // Show the main menu buttons (Enroll and Scan)
    lv_obj_clear_flag(enrollButton, LV_OBJ_FLAG_HIDDEN); // Show enroll button
    lv_obj_clear_flag(scanButton, LV_OBJ_FLAG_HIDDEN);   // Show scan button
    lv_obj_add_flag(returnButton, LV_OBJ_FLAG_HIDDEN);   // Hide return button
    lv_label_set_text(fingerLabel, "Select Enroll or Scan."); // Update label text

    // Reset modes
    enrollingMode = false; // Disable enrolling mode
    scanningMode = false;  // Disable scanning mode
  }
}

// Event handler for the Scan/Return button
void scan_button_event_handler(lv_event_t *e) {
  // Get the event code (e.g., button click)
  lv_event_code_t code = lv_event_get_code(e);

  // Check if the Scan/Return button was clicked
  if (code == LV_EVENT_CLICKED) {
    // If scanning mode is off, start scanning
    if (!scanningMode) {
      scanningMode = true;  // Set scanning mode to true
      lv_label_set_text(fingerLabel, "Scanning...");  // Update label to show scanning status
      lv_label_set_text(lv_obj_get_child(scanButton, NULL), "Return");  // Change button text to "Return"

      // Center the Return button on the screen
      lv_obj_align(scanButton, LV_ALIGN_CENTER, 0, 40);
      
      // Hide the Enroll button during scanning
      lv_obj_add_flag(enrollButton, LV_OBJ_FLAG_HIDDEN);

      // Scanning process can start here
    } else {  // If already scanning, stop and return to the main menu
      scanningMode = false;  // Disable scanning mode
      lv_label_set_text(fingerLabel, "Returning to main menu...");  // Update label to show returning status
      lv_label_set_text(lv_obj_get_child(scanButton, NULL), "Scan");  // Change button text back to "Scan"

      // Restore the Scan button's position
      lv_obj_align(scanButton, LV_ALIGN_CENTER, -80, 40);
      
      // Show the Enroll button again
      lv_obj_clear_flag(enrollButton, LV_OBJ_FLAG_HIDDEN);
    }
  }
}

// Event handler for the Enroll button
void enroll_button_event_handler(lv_event_t *e) {
  // Get the event code (e.g., button click)
  lv_event_code_t code = lv_event_get_code(e);

  // If the Enroll button was clicked
  if (code == LV_EVENT_CLICKED) {
    lv_label_set_text(fingerLabel, "Enrolling, please enter the ID:");  // Update label to show enrollment process
    Serial.println("Enroll button clicked.");  // Print message to Serial monitor for debugging

    // Hide the main menu buttons (Enroll and Scan)
    lv_obj_add_flag(enrollButton, LV_OBJ_FLAG_HIDDEN);  // Hide Enroll button
    lv_obj_add_flag(scanButton, LV_OBJ_FLAG_HIDDEN);  // Hide Scan button
    lv_obj_clear_flag(inputTextArea, LV_OBJ_FLAG_HIDDEN);  // Show text input area for ID entry
    lv_obj_clear_flag(keyboard, LV_OBJ_FLAG_HIDDEN);  // Show on-screen keyboard
  }
}

// Event handler for the on-screen keyboard
void keyboard_event_handler(lv_event_t *e) {
  // Get the event code (e.g., input ready)
  lv_event_code_t code = lv_event_get_code(e);

  // If the user has finished entering text (e.g., ID input)
  if (code == LV_EVENT_READY) {
    // Get the text from the input text area
    const char* input = lv_textarea_get_text(inputTextArea);
    id = atoi(input);  // Convert the input string to an integer ID

    // Validate the ID (must be between 1 and 127)
    if (id > 0 && id <= 127) {
      lv_label_set_text_fmt(fingerLabel, "Enrolling ID #%d", id);  // Show the ID being enrolled
      lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);  // Hide the keyboard
      lv_obj_add_flag(inputTextArea, LV_OBJ_FLAG_HIDDEN);  // Hide the input text area
      lv_obj_clear_flag(returnButton, LV_OBJ_FLAG_HIDDEN);  // Show the Return button
      enrollingMode = true;  // Set enrolling mode to true
    } else {
      // If the ID is invalid, show an error message
      lv_label_set_text(fingerLabel, "Invalid ID, please try again.");
    }
  }
}

// Function to return to the main menu
void return_to_main_menu() {
  // Show the main menu buttons (Enroll and Scan)
  lv_obj_clear_flag(enrollButton, LV_OBJ_FLAG_HIDDEN);  // Show Enroll button
  lv_obj_clear_flag(scanButton, LV_OBJ_FLAG_HIDDEN);  // Show Scan button
  lv_obj_add_flag(returnButton, LV_OBJ_FLAG_HIDDEN);  // Hide the Return button
  lv_label_set_text(fingerLabel, "Select Enroll or Scan.");  // Update label to prompt user action

  // Reset enrollment and scanning modes
  enrollingMode = false;
  scanningMode = false;
}

// Function to handle fingerprint enrollment process
void handleFingerprintEnrollment() {
  // If not in enrolling mode or no valid ID, exit the function
  if (!enrollingMode || id == 0) return;

  // Prompt user to place finger for enrollment
  lv_label_set_text_fmt(fingerLabel, "Place finger to enroll as ID #%d", id);
  lv_timer_handler();  // Force display update to reflect the new message
  delay(200);  // Small delay for display update

  // Capture the fingerprint image
  int p = finger.getImage();
  if (p == FINGERPRINT_NOFINGER) {
    Serial.println("No finger detected.");  // Print debug message if no finger is detected
    return;
  }

  // If image was successfully taken
  if (p == FINGERPRINT_OK) {
    Serial.println("Image taken");  // Debug message for successful image capture
    lv_label_set_text(fingerLabel, "Image taken, processing...");  // Update label
    lv_timer_handler();  // Force display update
    delay(200);  // Small delay for display update

    p = finger.image2Tz(1);  // Convert image to a fingerprint template
    if (p == FINGERPRINT_OK) {
      Serial.println("Remove finger and place it again.");  // Prompt to place the same finger again
      lv_label_set_text(fingerLabel, "Remove finger and place it again.");
      lv_timer_handler();  // Force display update
      delay(2000);  // Allow user time to remove finger

      // Wait for user to remove the finger
      while (finger.getImage() != FINGERPRINT_NOFINGER) {
        delay(100);  // Prevent busy loop by adding a small delay
      }

      lv_label_set_text(fingerLabel, "Place the same finger again.");
      lv_timer_handler();  // Force display update
      delay(500);  // Allow display to show message

      // Wait for user to place the same finger again
      while (finger.getImage() != FINGERPRINT_OK) {
        delay(100);  // Small delay to avoid busy loop
      }

      // Convert the second fingerprint image to template
      p = finger.image2Tz(2);
      if (p == FINGERPRINT_OK) {
        // Merge the two templates and store the fingerprint
        p = finger.createModel();
        if (p == FINGERPRINT_OK) {
          p = finger.storeModel(id);  // Store the fingerprint with the provided ID
          if (p == FINGERPRINT_OK) {
            Serial.println("Fingerprint enrolled successfully.");  // Success message for enrollment
            lv_label_set_text_fmt(fingerLabel, "Fingerprint enrolled successfully as ID #%d", id);
            lv_timer_handler();  // Force display update
            delay(2000);  // Show success message for 2 seconds
            return_to_main_menu();  // Return to the main menu
          } else {
            lv_label_set_text(fingerLabel, "Failed to store fingerprint.");  // Error if storing fails
            lv_timer_handler();  // Force display update
          }
        } else {
          lv_label_set_text(fingerLabel, "Fingerprints did not match.");  // Error if fingerprints do not match
          lv_timer_handler();  // Force display update
        }
      } else {
        lv_label_set_text(fingerLabel, "Failed to capture second image.");  // Error if second image capture fails
        lv_timer_handler();  // Force display update
      }
    } else {
      lv_label_set_text(fingerLabel, "Failed to process image.");  // Error if image processing fails
      lv_timer_handler();  // Force display update
    }
  } else {
    lv_label_set_text(fingerLabel, "Error capturing image.");  // General error for image capture
    lv_timer_handler();  // Force display update
  }
}

// Function to enlarge a button (used for return button)
void enlarge_button(lv_obj_t *button) {
  lv_obj_set_size(button, 120, 60);  // Set button dimensions to 120x60 pixels
  lv_obj_set_style_pad_all(button, 10, 0);  // Add 10px padding to the button
}

// Setup function to initialize the display, fingerprint sensor, and buttons
void setup() {
  // Initialize serial communication for debugging and fingerprint sensor
  Serial.begin(115200);
  mySerial.begin(57600, SERIAL_8N1, RX_PIN, TX_PIN);  // Initialize the fingerprint sensor's serial communication
  tft.begin();  // Initialize the display
  tft.setRotation(1);  // Set display rotation

  touch_calibrate();  // Calibrate the touch screen

  // Initialize LVGL (GUI library)
  lv_init();
  lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * 10);  // Initialize display buffer

  // Set up the display driver
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);  // Initialize display driver structure
  disp_drv.flush_cb = my_disp_flush;  // Set the display flush callback function
  disp_drv.draw_buf = &draw_buf;  // Set the display buffer
  disp_drv.hor_res = screenWidth;  // Set horizontal resolution
  disp_drv.ver_res = screenHeight;  // Set vertical resolution
  lv_disp_drv_register(&disp_drv);  // Register the display driver with LVGL

  // Set up the touchpad driver
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);  // Initialize input device driver structure
  indev_drv.type = LV_INDEV_TYPE_POINTER;  // Set input device type to pointer (touch)
  indev_drv.read_cb = lvgl_port_tp_read;  // Set the touchpad read callback function
  lv_indev_drv_register(&indev_drv);  // Register the input device driver with LVGL

  // Create a label to display messages (fingerLabel)
  fingerLabel = lv_label_create(lv_scr_act());  // Create a label on the active screen
  lv_obj_align(fingerLabel, LV_ALIGN_CENTER, 0, -40);  // Align label to the center
  lv_label_set_text(fingerLabel, "Select Enroll or Scan.");  // Set default text for the label

  // Create buttons for Scan and Enroll
  scanButton = lv_btn_create(lv_scr_act());  // Create a Scan button
  lv_obj_set_size(scanButton, 100, 50);  // Set button size to 100x50 pixels
  lv_obj_align(scanButton, LV_ALIGN_CENTER, -80, 40);  // Align Scan button to the center-left
  lv_obj_t *scanButtonLabel = lv_label_create(scanButton);  // Create a label for the Scan button
  lv_label_set_text(scanButtonLabel, "Scan");                               // Set the text for the Scan button label
  lv_obj_add_event_cb(scanButton, scan_button_event_handler, LV_EVENT_ALL, NULL);  // Add an event handler for the Scan button

  enrollButton = lv_btn_create(lv_scr_act());                               // Create an Enroll button
  lv_obj_set_size(enrollButton, 100, 50);                                   // Set button size to 100x50 pixels
  lv_obj_align(enrollButton, LV_ALIGN_CENTER, 80, 40);                      // Align Enroll button to the center-right
  lv_obj_t *enrollButtonLabel = lv_label_create(enrollButton);              // Create a label for the Enroll button
  lv_label_set_text(enrollButtonLabel, "Enroll");                           // Set the text for the Enroll button label
  lv_obj_add_event_cb(enrollButton, enroll_button_event_handler, LV_EVENT_ALL, NULL);  // Add an event handler for the Enroll button

  // Create a Return button (initially hidden)
  returnButton = lv_btn_create(lv_scr_act());                               // Create a Return button
  enlarge_button(returnButton);                                             // Enlarge the Return button
  lv_obj_align(returnButton, LV_ALIGN_CENTER, 0, 40);                       // Align the Return button to the center
  lv_obj_t *returnButtonLabel = lv_label_create(returnButton);              // Create a label for the Return button
  lv_label_set_text(returnButtonLabel, "Return");                               // Set the text for the Return button label
  lv_obj_add_event_cb(returnButton, return_button_event_handler, LV_EVENT_ALL, NULL);  // Add an event handler for the Return button
  lv_obj_add_flag(returnButton, LV_OBJ_FLAG_HIDDEN);                            // Hide the Return button initially

  // Create a text area for user input (initially hidden)
  inputTextArea = lv_textarea_create(lv_scr_act());                         // Create a text area
  lv_textarea_set_one_line(inputTextArea, true);                            // Set the text area to single-line mode
  lv_textarea_set_placeholder_text(inputTextArea, "Enter ID");              // Set placeholder text for the input text area
  lv_obj_align(inputTextArea, LV_ALIGN_CENTER, 0, -20);                     // Align the input text area to the center
  lv_obj_add_flag(inputTextArea, LV_OBJ_FLAG_HIDDEN);                       // Hide the input text area initially

  // Create a keyboard for user input (initially hidden)
  keyboard = lv_keyboard_create(lv_scr_act());                              // Create a keyboard
  lv_keyboard_set_textarea(keyboard, inputTextArea);                        // Link the keyboard to the input text area
  lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);                            // Hide the keyboard initially
  lv_obj_add_event_cb(keyboard, keyboard_event_handler, LV_EVENT_ALL, NULL);    // Add an event handler for the keyboard

  // Initialize the fingerprint sensor
  if (finger.verifyPassword()) {
    Serial.println("Fingerprint sensor initialized.");  // Debug message for successful fingerprint sensor initialization
  } else {
    Serial.println("Fingerprint sensor initialization failed.");  // Debug message for failed fingerprint sensor initialization
    while (1);  // Halt execution if fingerprint sensor initialization fails
  }
}

void loop() {
  lv_timer_handler();  // Keep the LVGL running and update the UI
  delay(5);  // Delay for LVGL to handle its tasks

  if (enrollingMode) {  // Check if in enrollment mode
    handleFingerprintEnrollment();  // Call enrollment function if active
  }
  
  if (scanningMode) {  // Check if in scanning mode
    scanFingerprint();  // Call the fingerprint scanning function if active
  }
}

// Function to handle fingerprint detection and matching
uint8_t getFingerprintID() {
  uint8_t p = finger.getImage();

  // No finger detected
  if (p == FINGERPRINT_NOFINGER) return FINGERPRINT_NOFINGER;

  // Check if the image can be converted to features
  if (p != FINGERPRINT_OK) return p;
  p = finger.image2Tz();

  // Search for a matching fingerprint
  if (p != FINGERPRINT_OK) return p;
  p = finger.fingerSearch();
  if (p != FINGERPRINT_OK) return p;

  return finger.fingerID;
}
