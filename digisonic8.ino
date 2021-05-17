/*
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation version 3.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see https://www.gnu.org/licenses
*/

#include "DaisyDuino.h"
#include <elapsedMillis.h>
#include <ArduinoTapTempo.h>
#include <Bounce2.h>

DaisyHardware hw;

size_t num_channels;

// Set max delay time to * of samplerate.
#define MAX_DELAY static_cast<size_t>(48000 * 1.5f)

// Create filter variables
static Tone flt, flt_del;             // low pass filter for delay feedback signal
float freq, freq2;

// Create Delay variables
float delay_time, delay_feedback, sample_rate;

// Declare a DelayLine of MAX_DELAY number of floats.
static DelayLine<float, MAX_DELAY> del;

// CHORUS
static Chorus koorus;

// Create potentiometer variables
float old_pot0, old_pot1, old_pot2, old_pot3, pot0, pot1, pot2, pot3, logpot3, old_logpot3, pot4, old_pot4, pot5, old_pot5;

// Create elapsedmillis variables
elapsedMillis potikkaviive;
elapsedMillis sincePrint;
elapsedMillis countAt;
elapsedMillis btn2Press;

// Momentary switches
const int BUTTON_PIN = 7; // Tap tempo button
const int BUTTON2_PIN = 8; // Switch to select tap speed

// Led to display tap tempo speed
int led1pin = 9; //Pin for led to display tap tempo
int ledstate = LOW;
unsigned long ledtimeout = 0;

// Second led
int led2pin = 10;

Bounce bouncer = Bounce ();
Bounce bouncer2 = Bounce();

int taputustempo = 0;
unsigned int aika = 2000; //How long tap button must be held down to enable/disable tap tempo
byte tapbtnVal = HIGH;
int lastTapBtnPressMillis = 0;

unsigned int count = 1;            // how many times has it changed to low
//unsigned long countAt = 0;         // when count changed
unsigned int countPrinted = 0;     // last count printed

// make an ArduinoTapTempo object
ArduinoTapTempo tapTempo;

void MyCallback(float **in, float **out, size_t size)
{
  // Set filter variable values
  flt.SetFreq(freq);
  flt_del.SetFreq(freq2);

  // Set Delay time
  del.SetDelay(sample_rate * delay_time);

  for (size_t i = 0; i < size; i++)
  {
    
    float dry_left, dry_right, del_out, dry_filtered, filtered, sig_out, chorused, summa; 

    // Get dry signal
    dry_right = in[1][i];
    dry_left = in[0][i];

    //apply chorus
    chorused = koorus.Process(dry_left);

    //Mix chorused and dry signal
    summa = (chorused + dry_right) * 0.5f;

    // Apply filter
    dry_filtered = flt_del.Process(summa);

    // Read from delay line
    del_out = del.Read(); 

    // Apply filter (first order low pass filter to delayed signal)
    filtered = flt.Process(del_out);

    // Calculate output and feedback (feedback goes through low pass filter)
    del.Write(((del_out * 0.001) + dry_filtered) + filtered * delay_feedback);

    // Output
    out[1][i] = del_out;
    out[0][i] = del_out;
    
  }
  
}

void setup() {
  Serial.begin(9600);

  // float sample_rate;
  // Initialize for Daisy pod at 48kHz
  hw = DAISY.init(DAISY_SEED, AUDIO_SR_48K);
  num_channels = hw.num_channels;
  sample_rate = DAISY.get_samplerate();

  // Create filter objects
  flt.Init(sample_rate);
  flt_del.Init(sample_rate);
  
  // Create delay object
  del.Init();

  // Create Chorus object and set variables
  koorus.Init(sample_rate);
  koorus.SetLfoDepth(1.0f);
  koorus.SetLfoFreq(1.0f);
  koorus.SetDelay(0.1f);
  koorus.SetDelayMs(2.0f);
  koorus.SetFeedback(0.1f);

  // setup button
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  digitalWrite(BUTTON_PIN, HIGH);

  // setup bouncer to pin and interval in ms
  bouncer .attach(BUTTON_PIN);
  bouncer .interval(10);

  pinMode(BUTTON2_PIN, INPUT_PULLUP);

  // setup bouncer2 to pin and interval in ms
  bouncer2 .attach(BUTTON2_PIN);
  bouncer2 .interval(10);

  // setup maximum and minimum lenghts for tap tempo in ms
  tapTempo.setMaxBeatLengthMS(1500);
  tapTempo.setMinBeatLengthMS(250);

  pinMode(led1pin, OUTPUT);
  digitalWrite(led1pin, ledstate);

  pinMode(led2pin, OUTPUT);
  analogWrite(led2pin, 0);

  DAISY.begin(MyCallback);
  
}

void loop() {
  
  bouncer.update();
  int value = bouncer.read();
  
  // get the state of the button
  boolean buttonDown = value == LOW;
  
  // update ArduinoTapTempo
  tapTempo.update(buttonDown);
  unsigned int tapdelay = tapTempo.getBeatLength();
  if (sincePrint > 10000) {
    sincePrint = 0;
    Serial.print("taptempo:");
    Serial.print("bpm: ");
    Serial.print(tapTempo.getBPM());
    Serial.print(", ms: ");
    Serial.println(tapTempo.getBeatLength());    
  }

  // Tap tempo LED
  if (countAt - ledtimeout >= tapdelay / 2) {
    ledtimeout = countAt;
    if (ledstate == LOW) {
      ledstate = HIGH;
    }
    else {
      ledstate = LOW;
    }
  digitalWrite(led1pin, ledstate);
  }
  
  // setup button 2 to count from 1 to 4 and reset to 1 after 4
  if (bouncer2.update()) {
    if (bouncer2.fell()) {
      count = count + 1;
      //elapsedMillis countAt;
      if (count > 4) {
        count = 1;
      }
    }
  } else {
    if (count != countPrinted) {
      elapsedMillis nowMillis;
      if (nowMillis - countAt > 100) {
        Serial.print("count: ");
        Serial.println(count);
        countPrinted = count;
      }
    }
  }

  // holdin tap tempo button down enables/disables tap tempo
  tapbtnVal = digitalRead(BUTTON_PIN);
  if (tapbtnVal == HIGH) {    // assumes btn is LOW when pressed
     lastTapBtnPressMillis = btn2Press;   // btn not pressed so reset clock
  }
  if (btn2Press - lastTapBtnPressMillis >= aika) {
    // button has been pressed for longer than interval
    if (taputustempo == 0) {
      taputustempo = 1; 
      lastTapBtnPressMillis = btn2Press;
    }
    else {
      taputustempo = 0;
      lastTapBtnPressMillis = btn2Press;
    }
      Serial.print("taputustempo; ");
      Serial.println(taputustempo);
  }

  // if tap tempo is enabled delay time is set by tap otherwise pot is used to set delay time
  if (taputustempo == 1) {
    tapdelay = tapTempo.getBeatLength();
    analogWrite(led2pin, 10);
    if (count == 2) {
      tapdelay = tapdelay * 3/4;
      analogWrite(led2pin, 40);
      //Serial.println(tapdelay);
    }
    else if (count == 3) {
      tapdelay = tapdelay * 2/4;
      analogWrite(led2pin, 130);
      //Serial.println(tapdelay);
    }
    else if (count == 4) {
      tapdelay = tapdelay * 1/4;
      analogWrite(led2pin, 255);
      //Serial.println(tapdelay);
    }
    delay_time = float(tapdelay) / 1000.0;
  }
  else {
    analogWrite(led2pin, 0);
    // Delay time pot
    pot3 = (map(analogRead(A1), 0.0, 1023.0, 250.0, 1023.0)); 
    delay_time = logpot3 / 1023.0; // makes sure delay time is set by pot3 when changing back from tap 
    if (abs(pot3 - old_pot3) > 20){
      if (potikkaviive > 1500) {
        old_pot3 = pot3;
        potikkaviive = 0;
        logpot3 = expf(pot3 / 290) * 30;
        delay_time = logpot3 / 1023.0;
        Serial.print("Delay time value: ");
        Serial.println(delay_time);
      }
    }
  }
  
  // Delay filter pot
  pot1 = (map(analogRead(A3), 0.0, 1023.0, 350.0, 6000.0));
  if (abs(pot1 - old_pot1) > 100) {
    old_pot1 = pot1;
    freq = pot1;
    freq2 = freq * 1.7;
    Serial.print("Delay lowpass filter value: ");
    Serial.println(freq);
    Serial.println(freq2);
  }

  // Delay feedback pot
  pot2 = (map(analogRead(A0), 0.0, 1023.0, 10.0, 1023.0));
  if (abs(pot2 - old_pot2) > 30){
    old_pot2 = pot2;
    delay_feedback = pot2 / 1023.0;
    Serial.print("Delay feedback value: ");
    Serial.println(delay_feedback);
  }

}
