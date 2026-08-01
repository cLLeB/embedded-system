// =====================================================================
//  BuzzerTest.ino - does nothing except try to make the buzzer sound.
//
//  Everything else is disconnected from the problem: no sensor, no relay,
//  no buttons, no state machine. Just pin 8.
//
//  It sweeps through frequencies because a passive buzzer is loudest at
//  its own resonant frequency and can be nearly silent well away from it.
//  Then it tries a steady DC level, which is what an ACTIVE buzzer wants.
//  One of these should make a noise.
//
//  Serial Monitor at 9600 tells you what it is playing, so you can match
//  what you hear to what it is doing.
// =====================================================================

const uint8_t PIN_BUZZER = 8;

// Long enough that a quiet tone cannot be missed. The startup beep in the
// main sketch is only 80 ms, which is easy to overlook on a faint buzzer.
const unsigned long TONE_MS = 1200;
const unsigned long GAP_MS  = 400;

const unsigned int FREQS[] = { 500, 1000, 1500, 2000, 2400, 3000, 4000, 5000 };
const uint8_t FREQ_COUNT = sizeof(FREQS) / sizeof(FREQS[0]);

void setup() {
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);

  Serial.begin(9600);
  while (!Serial) { }

  Serial.println();
  Serial.println(F("=================================="));
  Serial.println(F(" BUZZER TEST - pin 8 only"));
  Serial.println(F("=================================="));
  Serial.println(F("Listen. Note WHICH step you hear."));
  Serial.println();
}

void loop() {
  // --- Pass 1: sweep tones. This is what a PASSIVE buzzer needs. ---
  for (uint8_t i = 0; i < FREQ_COUNT; i++) {
    Serial.print(F("tone "));
    Serial.print(FREQS[i]);
    Serial.println(F(" Hz"));

    tone(PIN_BUZZER, FREQS[i]);
    delay(TONE_MS);
    noTone(PIN_BUZZER);
    digitalWrite(PIN_BUZZER, LOW);
    delay(GAP_MS);
  }

  // --- Pass 2: steady on. This is what an ACTIVE buzzer needs. ---
  Serial.println(F("steady HIGH (active-buzzer test)"));
  digitalWrite(PIN_BUZZER, HIGH);
  delay(TONE_MS);
  digitalWrite(PIN_BUZZER, LOW);
  delay(GAP_MS);

  // --- Pass 3: slow clicking. Audible even on a nearly-dead buzzer,
  //     because each transition kicks the diaphragm.               ---
  Serial.println(F("slow clicks"));
  for (uint8_t i = 0; i < 10; i++) {
    digitalWrite(PIN_BUZZER, HIGH);
    delay(50);
    digitalWrite(PIN_BUZZER, LOW);
    delay(150);
  }

  Serial.println(F("--- repeating ---"));
  Serial.println();
  delay(1500);
}
