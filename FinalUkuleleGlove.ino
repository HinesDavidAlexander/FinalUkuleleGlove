// Written by: Alex Hines
// 'Ukulele Synth Glove' project
// Date: 5/26/2025

// ====================== Finger settings ======================
// Define the pins for the fingers
const int fingerPin1 = 10;  // Pin for G (Index Finger)
const int fingerPin2 = 9;  // Pin for C (Middle Finger)
const int fingerPin3 = 6; // Pin for E (Ring Finger)
const int fingerPin4 = 12; // Pin for A (Pinkie Finger)
const int fingerPins[] = {fingerPin1, fingerPin2, fingerPin3, fingerPin4}; // Array of finger pins

const int numFingers = 4;

// Variable to track which finger's note is currently playing
// -1 means no note is playing for that finger. This system plays one note at a time. [Area of potential future improvement - multiple notes at one time]
int currentlyPlayingFinger = -1;

// ======================= Note settings =======================
// Define the pin for the buzzer (SDA pin, usable as general I/O)
const int buzzerPin = 2;

// Ukulele base note frequencies (G4, C4, E4, A4)
const float NOTE_G4 = 392.00;
const float NOTE_C4 = 261.63;
const float NOTE_E4 = 329.63;
const float NOTE_A4 = 440.00;

// Array of base notes
const float baseNotes[] = {NOTE_G4, NOTE_C4, NOTE_E4, NOTE_A4};

// ====================== Octave settings ======================
// Define the pin for the octave button (SCL Pin, but works for standard IO)
const int octaveButtonPin = 3;

// Index for the octaveMultipliers array
int currentOctave = 0;
 // Total number of octave settings to cycle through
const int numOctaveSettings = 3;

// Octave multipliers: 0 = Middle, 1 = Up one octave, 2 = Up two octaves
float octaveMultipliers[] = {1.0, 2.0, 4.0};
// Alternative for Low, Middle, High with different octaves in-range:
// float octaveMultipliers[] = {0.5, 1.0, 2.0};

// ====================== Button settings ======================
// Variables to track button state for debouncing and single press detection
int lastButtonState = HIGH; // Assuming button pulls to LOW when pressed (internal pull-up)
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50; // 50 milliseconds for debounce
int debouncedButtonState = HIGH;

// ====================== Debouncing Vars ======================
// Finger Debouncing Variables
int fingerDebouncedState[numFingers];    // Stores the current debounced state (LOW = pressed)
int lastFingerRawState[numFingers];      // Stores the last raw input state read
unsigned long lastFingerDebounceTime[numFingers]; // Stores time of last raw state change for each finger

// =================== Setup & Loop Methods ====================

void setup() {
  Serial.begin(9600); // Initialize serial communication for debugging

  for (int i = 0; i < numFingers; i++) {
    pinMode(fingerPins[i], INPUT_PULLUP);
    // Initialize finger debouncing variables
    lastFingerRawState[i] = HIGH;
    fingerDebouncedState[i] = HIGH;
    lastFingerDebounceTime[i] = 0;
  }

  // Set octave button pin as input
  pinMode(octaveButtonPin, INPUT_PULLUP);
  lastButtonState = digitalRead(octaveButtonPin);
  debouncedButtonState = lastButtonState;

  // Set buzzer pin as output
  pinMode(buzzerPin, OUTPUT);

  Serial.println("Ukulele Glove Synthesizer ready");
  Serial.print("Octave Setting: ");
  Serial.print(currentOctave);
  printOctaveName();
}

void loop() {
  handleOctaveButton();
  handleFingerPresses();
}

// ====================== Helper Methods =======================
void printOctaveName() {
  if (octaveMultipliers[currentOctave] == 0.5) Serial.println(" (Low)");
  else if (octaveMultipliers[currentOctave] == 1.0) Serial.println(" (Middle)");
  else if (octaveMultipliers[currentOctave] == 2.0) Serial.println(" (High)");
  else if (octaveMultipliers[currentOctave] == 4.0) Serial.println(" (Very High)");
  else Serial.println("");
}

// Method to handle button interactions while not blocking loop (No delay() calls)
void handleOctaveButton() {
  int currentRawReading = digitalRead(octaveButtonPin);

  // DEBUG: Check if Button is registering properly
  // Serial.print("Octave RAW: "); Serial.print(currentRawReading);
  // Serial.print(" | LastRAW: "); Serial.print(lastButtonState); // 'lastButtonState' is our previous raw reading
  // Serial.print(" | DebouncedState: "); Serial.print(debouncedButtonState);
  // Serial.print(" | Debounce Timer: "); Serial.println(millis() - lastDebounceTime);
  // --- End of Debug Prints ---

  // Debounce logic for the button // Source for idea (Also used in handleFingerPresses()): https://dev.to/aneeqakhan/throttling-and-debouncing-explained-1ocb#
  // If the raw input has changed since last time, reset the debounce timer
  if (currentRawReading != lastButtonState) {
    lastDebounceTime = millis();
  }
  
  // If enough time has passed since the last raw input change
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (currentRawReading != debouncedButtonState) {
      debouncedButtonState = currentRawReading;

      // If debounced state is LOW - button was pressed
      if (debouncedButtonState == LOW) {
        Serial.println(">>> Octave Button PRESS Detected (Debounced) <<<");
        currentOctave = (currentOctave + 1) % numOctaveSettings;
        Serial.print("Octave Setting changed to: ");
        Serial.print(currentOctave);
        printOctaveName();

        if (currentlyPlayingFinger != -1) {
          noTone(buzzerPin);
          currentlyPlayingFinger = -1; 
          Serial.println("Note stopped for octave change, will restart if finger held.");
        }
      }
    }
  }
  // Store the current raw reading for the next loop
  lastButtonState = currentRawReading;
}

// Method to handle registering which finger is currently 'pressed' (in contact with the Thumb to complete circuit)
void handleFingerPresses() {
  // Read raw state and update debounced state for each finger
  for (int i = 0; i < numFingers; i++) {
    int rawState = digitalRead(fingerPins[i]);

    if (rawState != lastFingerRawState[i]) {
      // Reset debounce timer for this finger
      lastFingerDebounceTime[i] = millis(); 
    }

    if ((millis() - lastFingerDebounceTime[i]) > debounceDelay) {
      // If the signal has been stable longer than the debounce delay, update the debounced state
      if (rawState != fingerDebouncedState[i]) {
        fingerDebouncedState[i] = rawState;
      }
    }
    lastFingerRawState[i] = rawState;
  }

  int fingerToPlay = -1;
  // Find the first (lowest index) pressed finger
  for (int i = 0; i < numFingers; i++) {
    if (fingerDebouncedState[i] == LOW) { // LOW means pressed (after debouncing)
      fingerToPlay = i;
      break; 
    }
  }

  // Play or stop tone
  if (fingerToPlay != -1) { // A finger is pressed
    if (currentlyPlayingFinger != fingerToPlay) {
      // New finger press, or finger press after octave change (where currentlyPlayingFinger was reset)
      float frequency = baseNotes[fingerToPlay] * octaveMultipliers[currentOctave];
      tone(buzzerPin, frequency);
      currentlyPlayingFinger = fingerToPlay;
      
      Serial.print("Playing Finger ");
      Serial.print(fingerToPlay + 1); 
      Serial.print(" (Note: ");
      
      if(fingerToPlay == 0) Serial.print("G");
      else if(fingerToPlay == 1) Serial.print("C");
      else if(fingerToPlay == 2) Serial.print("E");
      else if(fingerToPlay == 3) Serial.print("A");
      Serial.print(") at Freq: ");
      Serial.println(frequency);
    }
  } else { // No finger is pressed
    if (currentlyPlayingFinger != -1) { // If a note was playing, stop it
      noTone(buzzerPin);
      Serial.println("Stopping tone");
      currentlyPlayingFinger = -1; 
    }
  }
}