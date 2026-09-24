/*
  MEMORY + NAVIGATION GAME
  ------------------------
  Hardware: Arduino Uno, raw 8x8 LED matrix (16 pins, no driver chip),
  standard analog joystick (VRx, VRy, SW), buzzer.

  HOW IT WORKS
  1. A random target cell lights up for ~4 seconds, with a beep. Then it
     disappears and the Arduino remembers it internally.
  2. A "ball" LED appears at (0,0). The joystick moves it one cell at a
     time (push and return to center = one move).
  3. Clicking the joystick submits the ball's position as your guess.
  4. Correct -> 3 beeps. Wrong -> the matrix shows a capital "L".
  5. After a short pause, a new round starts automatically.

  IMPORTANT: read the wiring notes and the ROW_ACTIVE_HIGH / COL_ACTIVE_LOW
  section before powering on — see the chat message for the full
  wiring table and pin-identification procedure.
*/

#include <Arduino.h>

// ================= PIN CONFIGURATION =================
// --- 8x8 raw LED matrix (no driver chip) ---
// Assumption: rows are anodes (driven HIGH to select a row) and columns
// are cathodes (driven LOW to light an LED in the active row). If your
// matrix is wired the opposite way, flip these two constants — see the
// troubleshooting notes in chat.
const bool ROW_ACTIVE_HIGH = true;
const bool COL_ACTIVE_LOW  = true;

// row 0 = top row ... row 7 = bottom row
const uint8_t rowPins[8] = {2, 3, 4, 5, 6, 7, 8, 9};
// col 0 = left column ... col 7 = right column
const uint8_t colPins[8] = {10, 11, 12, 13, A0, A1, A2, A3};

// --- Joystick ---
const uint8_t JOY_X_PIN  = A4;
const uint8_t JOY_Y_PIN  = A5;
const uint8_t JOY_SW_PIN = 0;   // D0 / RX — see wiring note in chat

// --- Buzzer ---
const uint8_t BUZZER_PIN = 1;   // D1 / TX — see wiring note in chat

// ================= GAME TUNING =================
const unsigned long TARGET_SHOW_TIME  = 4000; // ms target stays visible (3-5s range)
const unsigned long RESULT_HOLD_TIME  = 2500; // ms success/fail screen stays up
const int JOY_DEAD_ZONE     = 150;  // how far from center counts as a "push"
const int JOY_NEUTRAL_BAND  = 80;   // must return this close to center to re-arm
const unsigned long MOVE_COOLDOWN   = 180;  // ms minimum between moves
const unsigned long BUTTON_DEBOUNCE = 50;   // ms
const unsigned long ROW_REFRESH_MICROS = 2000; // ~2ms/row -> ~62Hz full refresh

// ================= GAME STATE =================
enum GameState { SHOW_TARGET, PLAYING, RESULT_SUCCESS, RESULT_FAILURE };
GameState state = SHOW_TARGET;

bool grid[8][8]; // grid[y][x] = true means that LED should be on

int targetX, targetY;
int playerX, playerY;

int joyCenterX = 512, joyCenterY = 512;
bool joyArmedX = true, joyArmedY = true;
unsigned long lastMoveTime = 0;

bool lastButtonReading = HIGH;
bool buttonState = HIGH;
unsigned long lastDebounceTime = 0;

unsigned long stateEnteredTime = 0;

unsigned long lastRowSwitch = 0;
uint8_t currentRefreshRow = 0;

// Capital "L" bitmap. grid/bitmap indexing is [y][x], (0,0) = top-left.
const bool L_SHAPE[8][8] = {
  {0,0,0,0,0,0,0,0},
  {0,1,0,0,0,0,0,0},
  {0,1,0,0,0,0,0,0},
  {0,1,0,0,0,0,0,0},
  {0,1,0,0,0,0,0,0},
  {0,1,0,0,0,0,0,0},
  {0,1,1,1,1,1,0,0},
  {0,0,0,0,0,0,0,0}
};

// ================= SETUP =================
void setup() {
  for (int i = 0; i < 8; i++) {
    pinMode(rowPins[i], OUTPUT);
    pinMode(colPins[i], OUTPUT);
  }
  setRow(-1);
  for (int i = 0; i < 8; i++) setCol(i, false);

  pinMode(JOY_SW_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);

  // Calibrate joystick's resting (center) position — don't assume it's
  // exactly 512, cheap modules vary.
  joyCenterX = analogRead(JOY_X_PIN);
  joyCenterY = analogRead(JOY_Y_PIN);

  randomSeed(analogRead(JOY_X_PIN) + analogRead(JOY_Y_PIN) + micros());

  startNewRound();
}

// ================= MAIN LOOP =================
void loop() {
  refreshMatrix(); // must run every loop iteration, no blocking delay() anywhere else

  switch (state) {
    case SHOW_TARGET:
      handleShowTarget();
      break;
    case PLAYING:
      handlePlaying();
      break;
    case RESULT_SUCCESS:
    case RESULT_FAILURE:
      handleResult();
      break;
  }
}

// ================= GAME STATE HANDLERS =================
void startNewRound() {
  targetX = random(0, 8);
  targetY = random(0, 8);
  // Don't let the target be the player's own starting cell — that would
  // make the round trivial (no navigation needed).
  while (targetX == 0 && targetY == 0) {
    targetX = random(0, 8);
    targetY = random(0, 8);
  }

  clearGrid();
  grid[targetY][targetX] = true;
  beep(1000, 150); // "target shown" tone

  playerX = 0;
  playerY = 0;
  joyArmedX = true;
  joyArmedY = true;

  state = SHOW_TARGET;
  stateEnteredTime = millis();
}

void handleShowTarget() {
  if (millis() - stateEnteredTime >= TARGET_SHOW_TIME) {
    clearGrid();
    grid[playerY][playerX] = true; // reveal the ball at (0,0)
    state = PLAYING;
  }
}

void handlePlaying() {
  readJoystickMovement();

  if (checkButtonPressed()) {
    if (playerX == targetX && playerY == targetY) {
      clearGrid();
      playSuccessBeeps();
      state = RESULT_SUCCESS;
    } else {
      drawL();
      state = RESULT_FAILURE;
    }
    stateEnteredTime = millis();
  }
}

void handleResult() {
  if (millis() - stateEnteredTime >= RESULT_HOLD_TIME) {
    startNewRound();
  }
}

// ================= JOYSTICK =================
void readJoystickMovement() {
  if (millis() - lastMoveTime < MOVE_COOLDOWN) return;

  int xVal = analogRead(JOY_X_PIN) - joyCenterX;
  int yVal = analogRead(JOY_Y_PIN) - joyCenterY;

  int newX = playerX;
  int newY = playerY;
  bool moved = false;

  // Horizontal movement (edge-triggered: must return to center to re-arm)
  if (joyArmedX) {
    if (xVal > JOY_DEAD_ZONE)      { newX = playerX + 1; moved = true; joyArmedX = false; }
    else if (xVal < -JOY_DEAD_ZONE) { newX = playerX - 1; moved = true; joyArmedX = false; }
  } else if (abs(xVal) < JOY_NEUTRAL_BAND) {
    joyArmedX = true;
  }

  // Vertical movement — skipped this cycle if X already moved, so a
  // diagonal push can't move two cells at once.
  if (!moved && joyArmedY) {
    if (yVal > JOY_DEAD_ZONE)      { newY = playerY + 1; moved = true; joyArmedY = false; }
    else if (yVal < -JOY_DEAD_ZONE) { newY = playerY - 1; moved = true; joyArmedY = false; }
  } else if (abs(yVal) < JOY_NEUTRAL_BAND) {
    joyArmedY = true;
  }

  if (moved) {
    newX = constrain(newX, 0, 7);
    newY = constrain(newY, 0, 7);
    if (newX != playerX || newY != playerY) {
      grid[playerY][playerX] = false;
      playerX = newX;
      playerY = newY;
      grid[playerY][playerX] = true;
    }
    lastMoveTime = millis();
  }
}

// ================= BUTTON (joystick click) =================
bool checkButtonPressed() {
  bool reading = digitalRead(JOY_SW_PIN);
  bool pressedEvent = false;

  if (reading != lastButtonReading) {
    lastDebounceTime = millis();
  }

  if (millis() - lastDebounceTime > BUTTON_DEBOUNCE) {
    if (reading != buttonState) {
      buttonState = reading;
      if (buttonState == LOW) { // INPUT_PULLUP -> pressed pulls the pin LOW
        pressedEvent = true;
      }
    }
  }

  lastButtonReading = reading;
  return pressedEvent;
}

// ================= BUZZER =================
void beep(unsigned int freq, unsigned int durationMs) {
  tone(BUZZER_PIN, freq, durationMs); // non-blocking, stops itself
}

void playSuccessBeeps() {
  for (int i = 0; i < 3; i++) {
    tone(BUZZER_PIN, 1500, 150);
    delay(200); // brief, deliberate pause so the 3 beeps are distinct
  }
  noTone(BUZZER_PIN);
}

// ================= MATRIX DRAWING =================
void clearGrid() {
  for (int y = 0; y < 8; y++)
    for (int x = 0; x < 8; x++)
      grid[y][x] = false;
}

void drawL() {
  for (int y = 0; y < 8; y++)
    for (int x = 0; x < 8; x++)
      grid[y][x] = L_SHAPE[y][x];
}

// ================= MATRIX LOW-LEVEL DRIVE =================
void setRow(int r) {
  for (int i = 0; i < 8; i++) {
    bool selected = (i == r);
    if (ROW_ACTIVE_HIGH)
      digitalWrite(rowPins[i], selected ? HIGH : LOW);
    else
      digitalWrite(rowPins[i], selected ? LOW : HIGH);
  }
}

void setCol(int c, bool on) {
  if (COL_ACTIVE_LOW)
    digitalWrite(colPins[c], on ? LOW : HIGH);
  else
    digitalWrite(colPins[c], on ? HIGH : LOW);
}

// Multiplexed refresh: lights one row at a time, cycling fast enough
// (~62Hz full-matrix refresh) that persistence of vision makes it look
// like several LEDs are lit at once. This is what lets drawL() show
// multiple LEDs even though only one row can be physically powered
// at any instant.
void refreshMatrix() {
  if (micros() - lastRowSwitch < ROW_REFRESH_MICROS) return;
  lastRowSwitch = micros();

  setRow(-1); // blank briefly while columns change, avoids "ghost" LEDs
  for (int c = 0; c < 8; c++) {
    setCol(c, grid[currentRefreshRow][c]);
  }
  setRow(currentRefreshRow);

  currentRefreshRow = (currentRefreshRow + 1) % 8;
}
