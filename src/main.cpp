#include <Arduino.h>

#define PIN_PRI_A   9    // OCR1A - high-active primary drive
#define PIN_PRI_B   10   // OCR1B - low-active primary drive

// The timer tick rate depends on what you’re trying accomplish. I needed something between 10 & 50 kHz, so I set the prescaler to 1 to get decent resolution: 62.5 ns.
// Times in microseconds, converted to timer ticks
//  ticks depend on 16 MHz clock, so ticks = 62.5 ns
//  prescaler can divide that, but we're running fast enough to not need it
 
#define TIMER1_PRESCALE   1     // clock prescaler value
#define TCCR1B_CS20       0x01  // CS2:0 bits = prescaler selection

const int default_frequency_kHz = 10; // 10 kHz
const int default_duty_cycle = 20;    // 20%

int g_frequency_kHz;
int g_period_us;
int g_duty_cycle;
unsigned long g_period_ticks;
unsigned long g_on_ticks;

void setupTimer(bool inverted)
{
  // Phase-Frequency Correct mode (WGM1 = 8) runs at half-speed (it counts both up and down in each cycle), so the number of ticks is half what you’d expect. You could use Fast PWM mode or anything else, as long as you get the counts right; see page 133 of the Fine Manual.
  // Phase-freq Correct timer mode -> runs at half the usual rate

  // Configure Timer 1 for Freq-Phase Correct PWM
  //   Timer 1 + output on OC1A, chip pin 15, Arduino PWM9
  //   Timer 1 - output on OC1B, chip pin 16, Arduino PWM10

  // let Arduino setup do its thing
  analogWrite(PIN_PRI_A,128);    
  analogWrite(PIN_PRI_B,128);

  // stop Timer1 clock for register updates
  TCCR1B = 0x00;

  // Clear OC1A on match, P-F Corr PWM Mode: lower WGM1x = 00
  if(inverted) {
    TCCR1A = (1 << COM1A1) | (1 << COM1A0) | (1 << COM1B1) | (0 << COM1B0);
  } else {
    TCCR1A = (1 << COM1A1) | (0 << COM1A0) | (1 << COM1B1) | (1 << COM1B0);
  }

  // upper WGM1x = 10
  TCCR1B = 0x10;
}

// return period in micro-seconds
int calculatePeriod(int frequency_kHz) {
  return (1000 / frequency_kHz);
}

void setFrequencyDutyCycle(int frequency_kHz, int duty_cycle) {
  g_frequency_kHz = frequency_kHz;
  g_duty_cycle = duty_cycle;

  g_period_us = calculatePeriod(frequency_kHz);
  g_period_ticks = microsecondsToClockCycles(g_period_us / 2) / TIMER1_PRESCALE;

  g_on_ticks = g_period_ticks * duty_cycle / 100;

  // stop timer
  TCCR1B &= ~TCCR1B_CS20;

  // update registers
  // PWM period
  ICR1 = g_period_ticks;

  // Output 1 ON duration
  OCR1A = g_on_ticks;

  // Output 1 ON duration
  OCR1B = g_period_ticks - g_on_ticks;

  // force immediate OCR1x compare on next tick
  TCNT1 = OCR1A - 1;

  // start timer, Clock Sel = prescaler
  TCCR1B |= TCCR1B_CS20;
}

void increaseDutyCycle() {
  if(g_duty_cycle < 48) {
    g_duty_cycle++;
    setFrequencyDutyCycle(g_frequency_kHz, g_duty_cycle);
  }
}

void decreaseDutyCycle() {
  if(g_duty_cycle > 0) {
    g_duty_cycle--;
    setFrequencyDutyCycle(g_frequency_kHz, g_duty_cycle);
  }
}

void printDebugInfo()
{
    Serial.print("Period: ");
    Serial.print(g_period_us);
    Serial.print(" us");
    Serial.print("  Frequency: ");
    Serial.print(g_frequency_kHz);
    Serial.print(" kHz");
    Serial.print("  Duty Cycle: ");
    Serial.print(g_duty_cycle);
    Serial.print("%");
    Serial.print("  Period ticks: ");
    Serial.print(g_period_ticks);
    Serial.print("  ON ticks: ");
    Serial.println(g_on_ticks);
}

void setup() {
  Serial.begin(9600);

  setupTimer(true);
  setFrequencyDutyCycle(default_frequency_kHz, default_duty_cycle);
  printDebugInfo();
}

void loop() {
  while(Serial.available()){
    int ch = Serial.read();

    if(ch == '0') {
      setFrequencyDutyCycle(100, default_duty_cycle);
    } else if(ch >= '1' && ch <= '9') {
      setFrequencyDutyCycle((ch - '0') * 10, default_duty_cycle);
    } else if(ch == ',') {
      decreaseDutyCycle();
    } else if(ch == '.') {
      increaseDutyCycle();
    } else if(ch == ' ') {
      if(g_duty_cycle != 48) {
        setFrequencyDutyCycle(g_frequency_kHz, 48);
      } else {
        setFrequencyDutyCycle(g_frequency_kHz, 10);
      }
    }

    printDebugInfo();
  }
}
