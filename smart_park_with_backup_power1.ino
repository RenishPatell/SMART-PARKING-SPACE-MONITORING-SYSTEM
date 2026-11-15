// Smart Parking Space Monitoring System
// Each bay records when a carrives and how long it stays in the bay.
// There is nightlight and a 9V backup power to takeover if the 12V mains supply fails.
// The LCD indicated B when the system is on backup

#include <Adafruit_LiquidCrystal.h>

const int NUM_BAYS = 4;
const int sensorPins[NUM_BAYS] = { 2, 3, 4, 5};     // SIG pin of each bay's sensor
const int redPins[NUM_BAYS] = {6, 7, 8, 9};         // red leg of each RGB LED = occupied
const int greenPins[NUM_BAYS] = {10, 11, 12, 13};   // green leg of each RGB LED = free
const int BUZZER_PIN = A0;
const int THRESHOLD_CM = 300;                       // occupied when distance <= 300 cm
const unsigned long VIEW_MS = 3000;                 // LCD view changes every 3 s
const int LDR_PIN = A1;                             // photoresistor divider (light level)
const int NIGHT_LED_PIN = A2;                       // Green nightlight LED
const int DARK_ON = 400;                            // light level below this = night
const int DARK_OFF = 500;                           // light level above this = day
const int MAINS_PIN = A3;                           // mains sense divider (high = mains on)
const int MAINS_OK_LEVEL = 500;                         // A3 reading above this = mains present

Adafruit_LiquidCrystal lcd(0);                      // LCD type MCP23008, address 32 (0x20)

bool occupied[NUM_BAYS] = {false, false, false, false};
long distances[NUM_BAYS] = {0, 0, 0, 0};
unsigned long arrivalMS[NUM_BAYS] = {0, 0, 0, 0};   // when each car arrived
unsigned long lastViewChange = 0;  
bool showTimers = false;                            //false = status view, true = timer view
bool nightOn = false;                               // is the nightlight on?
int lightLevel = 0;                                 // last reading, 0 (dark) to 1023 (bright)
bool onBackup = false;                              // true = running on the backup battery

long readDistanceCm(int pin){
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
    delayMicroseconds(2);
    digitalWrite(pin, HIGH);
    delayMicroseconds(10);
    digitalWrite(pin, LOW);
    pinMode(pin, INPUT);
    long duration = pulseIn(pin, HIGH, 30000UL);
    if (duration == 0) {
        return 999;
    }
    return duration * 0.01723;
}

unsigned long parkedSeconds (int i) {
    return (millis() - arrivalMS[i]) / 1000;
}

// Writes exactly 4 characters for one bay: "----", " 45s " or " 12m "
void formatTime(char *out, int i) {
    if (!occupied[i]) {
        strcpy(out, "----");
        return;
    }
    unsigned long s = parkedSeconds(i);
    if (s < 1000) {
        sprintf(out, "%3lus", s);
    } else {
        unsigned long m = s / 60;
        if (m > 999) m = 999;
        sprintf(out, "%3lum", m);
    }
}

// Turns the nightlight on when dark and off when bright.
// The gap between 400 and 500 stops flicker at dusk.
void updateNightLight() {
    lightLevel = analogRead(LDR_PIN);
    if (!nightOn && lightLevel < DARK_ON) {
        nightOn = true;
    } else if (nightOn && lightLevel > DARK_OFF) {
        nightOn = false;
    }
    digitalWrite(NIGHT_LED_PIN, nightOn ? HIGH : LOW);
}

void showStatusView(int freeCount) {
    char line[17];
    sprintf(line, "Free bays: %d/4 %c", freeCount, nightOn ? 'N' : ' ');
    lcd.setCursor(0, 0);
    lcd.print(line);
    sprintf(line, "1:%c 2:%c 3:%c 4:%c%c", 
        occupied[0] ? 'X' : 'F', occupied[1] ? 'X' : 'F',
        occupied[2] ? 'X' : 'F', occupied[3] ? 'X' : 'F',
        onBackup ? 'B' : ' ');                   // B = running on backup power
    lcd.setCursor(0, 1);
    lcd.print(line);
}

void showTimerView() {
    char t1[5], t2[5], t3[5], t4[5], line[17];
    formatTime(t1, 0);
    formatTime(t2, 1);
    formatTime(t3, 2);
    formatTime(t4, 3);
    sprintf(line,  "B1:%s B2:%s", t1, t2);
    lcd.setCursor(0, 0);
    lcd.print(line);
    sprintf(line, "B3:%s B4:%s%c", t3, t4, onBackup ? 'B' : ' ');
    lcd.setCursor(0, 1);
    lcd.print(line);
}

// Mains present = A3 high. If it drops, the backup battery is already
// powering the board through its diode; the code just reports it.
void checkPower() {
    bool backupNow = (analogRead(MAINS_PIN) < MAINS_OK_LEVEL);
    if (backupNow && !onBackup) {
        tone(BUZZER_PIN, 2000, 300);                 // one high beep: mains lost
    } else if (!backupNow && onBackup) {
        tone(BUZZER_PIN, 800, 100);                 // short low beep: mains back
    }
    onBackup = backupNow;
}

void setup() {
    for ( int i=0; i < NUM_BAYS; i++) {
        pinMode(redPins[i], OUTPUT);
        pinMode(greenPins[i], OUTPUT);
    }
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(NIGHT_LED_PIN, OUTPUT);
    lcd.begin(16, 2);
    lcd.setBacklight(1);
    onBackup = (analogRead(MAINS_PIN) < MAINS_OK_LEVEL);    // no beep at start-up
    lcd.print("LU Smart Car");
    lcd.setCursor(0, 1);
    lcd.print("Park System");
    delay(1000);
    lcd.clear();
    lastViewChange = millis();
}

void loop() {
    int freeCount = 0;
    bool newArrival = false;

    for (int i = 0; i < NUM_BAYS; i++) {
        distances[i] = readDistanceCm(sensorPins[i]);
        bool nowOccupied = (distances[i] <= THRESHOLD_CM);

        if (nowOccupied && !occupied[i]) {              // car arrived
            newArrival = true;
            arrivalMS[i] = millis();
        }

        occupied[i] = nowOccupied;
        digitalWrite(redPins[i], nowOccupied ? HIGH : LOW);
        digitalWrite(greenPins[i], nowOccupied ? LOW : HIGH);
        if (!nowOccupied) {
            freeCount++;
        }
        delay(20);
    }

    if (newArrival) {
        tone(BUZZER_PIN, 1000, 500);
    }

    updateNightLight();
    checkPower();

    if (millis() - lastViewChange >= VIEW_MS) {         // switch LCD view every 3 s
        showTimers = !showTimers;
        lastViewChange = millis();
    }
    if (showTimers) {
        showTimerView();
    } else {
        showStatusView(freeCount);
    }

    delay(200);
}
