#include <FastLED.h>

#define LED_PIN     2
#define COLOR_ORDER GRB
#define CHIPSET     WS2812
#define NUM_LEDS    16
#define BRIGHTNESS  220
#define FRAMES_PER_SECOND 15

const float tau = 2*3.14;

CRGB leds[NUM_LEDS];
int const sensorPin = A0;
int const ledPin = 13;
int const button1Pin = 0;
int sensorValue = 0;
int sensorValue_offset_corr_ = 0;
float sensorValue_spreizfaktor_ = 1.0;
uint8_t mode_ = 0;
uint8_t button_is_pressed = 0;
#define NUM_MODES 3
#define MQ3_MAX 1023
//#define MQ3_MIN 224
//#define MQ3_MAX 980

// duration during with all samples are compressed into one minimum
#define MIN_TIME_COMPRESS 50 * FRAMES_PER_SECOND
#define NUM_MIN_SAMPLES 2*4
uint16_t auto_offset_minimum_samples_[NUM_MIN_SAMPLES];
uint8_t auto_offset_minimum_index_ = 0;
uint16_t auto_offset_sample_counter_ = MIN_TIME_COMPRESS;

void intButtonPressed()
{
  button_is_pressed = millis();
  digitalWrite(ledPin, HIGH);
}

void intButtonReleased()
{
  if (millis() - button_is_pressed > 100)
    mode_ = (mode_ +1) % NUM_MODES;
  button_is_pressed = 0;
  digitalWrite(ledPin, LOW);
}

void setup() {
  Serial.begin(9600);
  pinMode(ledPin, OUTPUT);
  pinMode(sensorPin, INPUT);
  pinMode(button1Pin, INPUT_PULLUP);
  delay(3000); // sanity delay
  FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );
  FastLED.setBrightness( BRIGHTNESS );
  attachInterrupt(button1Pin, intButtonPressed, RISING);
  attachInterrupt(button1Pin, intButtonReleased, FALLING);
  for (uint8_t c=0; c<NUM_MIN_SAMPLES; c++)
    auto_offset_minimum_samples_[c]=(uint16_t) -1;
}

void loop()
{
  // Add entropy to random number generator; we use a lot of it.
  random16_add_entropy( sensorValue ^ random());

  switch (mode_) {
    case 0:
      ColourSinCosWheel();
    break;
    case 1:
      Fire2012(); // run simulation frame
    break;
    case 2:
    default:
      showAnalogValue();
    break;
  }
  FastLED.show(); // display this frame
  FastLED.delay(1000 / FRAMES_PER_SECOND);

  sensorValue = analogRead(sensorPin);
  Serial.print(sensorValue);

  auto_offset_minimum_samples_[auto_offset_minimum_index_] = min(sensorValue, auto_offset_minimum_samples_[auto_offset_minimum_index_]);
  auto_offset_sample_counter_--;
  if (auto_offset_sample_counter_== 0) {
    auto_offset_sample_counter_ = MIN_TIME_COMPRESS;
    auto_offset_minimum_index_++;
    auto_offset_minimum_index_ %= NUM_MIN_SAMPLES;

    uint16_t min_over_recent_time = auto_offset_minimum_samples_[0];
    for (uint8_t c=1; c<NUM_MIN_SAMPLES; c++)
      min_over_recent_time = min(auto_offset_minimum_samples_[c],min_over_recent_time);
    sensorValue_offset_corr_ = min_over_recent_time;
    sensorValue_spreizfaktor_ = 1.0 + ((float) sensorValue_offset_corr_ / 1024.0);
  }

  sensorValue = (int) ((float)(sensorValue - sensorValue_offset_corr_) * sensorValue_spreizfaktor_);
  sensorValue = max(sensorValue,0);

  Serial.print(" - ");
  Serial.println(sensorValue);

  while (button_is_pressed) {
      fadeall();
      FastLED.delay(1000 / FRAMES_PER_SECOND);
  }
}

//~ HUE_RED = 0,
//~ HUE_ORANGE = 32,
//~ HUE_YELLOW = 64,
//~ HUE_GREEN = 96,
//~ HUE_AQUA = 128,
//~ HUE_BLUE = 160,
//~ HUE_PURPLE = 192,
//~ HUE_PINK = 224

//#define HUE_DISTANCE HUE_RED - HUE_YELLOW
//#define HUE_DISTANCE 85/2
#define HUE_DISTANCE 0x40 // ==64

// input value 0..1023
// output 0,64,128,196
uint8_t shifthue(int sv)
{
    uint8_t hue_shift;
    hue_shift = sv / 4; //1024 / 4 == 256
//    hue_shift &= 0xE0; //do jumps in values of 32
    hue_shift &= 0xC0; //do jumps in values of 64
    return hue_shift;
}

CHSV HSVHeatColor( uint8_t temperature, uint8_t hue_shift)
{
    CHSV heatcolor;

    // Scale 'heat' down from 0-255 to 0-191,
    // which can then be easily divided into three
    // equal 'thirds' of 64 units each.
    uint8_t t192 = scale8_video( temperature, 192);

    // calculate a value that ramps up from
    // zero to 255 in each 'third' of the scale.
    uint8_t heatramp = t192 & 0x3F; // 0..63
    heatramp <<= 2; // scale up to 0..252

    // now figure out which third of the spectrum we're in:
    if( t192 & 0x80) {
        // we're in the hottest third
        heatcolor.h = HUE_YELLOW;
        heatcolor.v = 255; // full light
        heatcolor.s = 254 - heatramp; // lower saturation in order to go towards white

    } else if( t192 & 0x40 ) {
        // we're in the middle third
        heatcolor.h = HUE_RED + (uint8_t) (((uint16_t)(HUE_DISTANCE)) * 252 /((uint16_t)heatramp));
        heatcolor.v = 226 + (heatramp/2); // full light
        heatcolor.s = 252;

    } else {
        // we're in the coolest third
        heatcolor.h = HUE_RED;
        heatcolor.v = heatramp/2; // ramp up red
        heatcolor.s = 250;
    }

    heatcolor.h += hue_shift; //should overflow and wrap around

    return heatcolor;
}


// Fire2012 by Mark Kriegsman, July 2012
// as part of "Five Elements" shown here: http://youtu.be/knWiGsmgycY
////
// This basic one-dimensional 'fire' simulation works roughly as follows:
// There's a underlying array of 'heat' cells, that model the temperature
// at each point along the line.  Every cycle through the simulation,
// four steps are performed:
//  1) All cells cool down a little bit, losing heat to the air
//  2) The heat from each cell drifts 'up' and diffuses a little
//  3) Sometimes randomly new 'sparks' of heat are added at the bottom
//  4) The heat from each cell is rendered as a color into the leds array
//     The heat-to-color mapping uses a black-body radiation approximation.
//
// Temperature is in arbitrary units from 0 (cold black) to 255 (white hot).
//
// This simulation scales it self a bit depending on NUM_LEDS; it should look
// "OK" on anywhere from 20 to 100 LEDs without too much tweaking.
//
// I recommend running this simulation at anywhere from 30-100 frames per second,
// meaning an interframe delay of about 10-35 milliseconds.
//
// Looks best on a high-density LED setup (60+ pixels/meter).
//
//
// There are two main parameters you can play with to control the look and
// feel of your fire: COOLING (used in step 1 above), and SPARKING (used
// in step 3 above).
//
// COOLING: How much does the air cool as it rises?
// Less cooling = taller flames.  More cooling = shorter flames.
// Default 50, suggested range 20-100
#define COOLING  55

// SPARKING: What chance (out of 255) is there that a new spark will be lit?
// Higher chance = more roaring fire.  Lower chance = more flickery fire.
// Default 120, suggested range 50-200.
#define SPARKING 40

int analogValueShowStep=-1;
void showAnalogValue()
{
  if (analogValueShowStep< 0)
  {
    for (int i = 0; i < NUM_LEDS; i++) { leds[i] = CRGB::Black; }
    analogValueShowStep=0;
    return;
  }

  if (analogValueShowStep < min(sensorValue* NUM_LEDS * 3 / (MQ3_MAX - sensorValue_offset_corr_ ),NUM_LEDS * 3))
  {
    // Set the i'th led to red
    if (analogValueShowStep < NUM_LEDS)
      leds[analogValueShowStep % NUM_LEDS] = CRGB::Blue;
    else if (analogValueShowStep > 2* NUM_LEDS)
      leds[analogValueShowStep % NUM_LEDS] = CRGB::Green;
    else
      leds[analogValueShowStep % NUM_LEDS] = CRGB::Red;
  } else if (analogValueShowStep > NUM_LEDS * 3) {
    analogValueShowStep=-1;
    return;
  }

  analogValueShowStep++;
}

void fadeall() { for(int i = 0; i < NUM_LEDS; i++) { leds[i].nscale8(250); } }

void Fire2012()
{
// Array of temperature readings at each simulation cell
  static byte heat[NUM_LEDS];

  // Step 1.  Cool down every cell a little
    for( int i = 0; i < NUM_LEDS; i++) {
      heat[i] = qsub8( heat[i],  random8(0, ((COOLING * 10) / NUM_LEDS) + 2));
    }

    // Step 2.  Heat from each cell drifts 'up' and diffuses a little
    for( int k= NUM_LEDS - 1; k >= 2; k--) {
      heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2] ) / 3;
    }

    // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
    if( random8() < SPARKING ) {
      int y = random8(7);
      heat[y] = qadd8( heat[y], random8(160,255) );
    }

    // Step 4.  Map from heat cells to LED colors
    for( int j = 0; j < NUM_LEDS; j++) {
        switch (mode_) {
          case 0:
            leds[j] = HeatColor( heat[j] );
            break;
          case 1:
//            hsv2rgb_rainbow(HSVHeatColor( heat[j], shifthue(sensorValue)), leds[j]);
            hsv2rgb_spectrum(HSVHeatColor( heat[j], shifthue(sensorValue)), leds[j]);
            break;
        }
    }
}

byte steps = 0;
byte rotation_saturation = 0;
byte rotation_value = 0;
byte rotation_hue = NUM_LEDS -1;
void ColourSinCosWheel()
{
  CHSV hsvleds[NUM_LEDS];

//  byte sh = sensorValue * 4 / 20;
  byte sh = sensorValue * 4 / 19;

  for (uint8_t r=0; r<NUM_LEDS; r++)
  {
    //hsvleds[(r+rotation_saturation)%NUM_LEDS].s = 0xA0 + 0x4F * cos(tau * r / NUM_LEDS );
    hsvleds[(r+rotation_value)%NUM_LEDS].v = 96 + (byte) (48.0 * sin(tau * r / NUM_LEDS));
    hsvleds[(r+rotation_saturation)%NUM_LEDS].s = 0xfe;
    //hsvleds[(r+rotation_value)%NUM_LEDS].v = 96;
    //hsvleds[(r+rotation_hue)%NUM_LEDS].h = sh  + (byte)(50.0 * sin(2* tau * r / NUM_LEDS));
    hsvleds[(r+rotation_hue)%NUM_LEDS].h = sh +20 + (byte)(20.0 * sin(2* tau * r / NUM_LEDS));
  }

  for (uint8_t c=0; c<NUM_LEDS; c++) {
    hsv2rgb_spectrum(hsvleds[c], leds[c]);
  }
  //rotation_saturation++;
  steps++;
  rotation_value++;
  rotation_hue -= steps%2;
  rotation_saturation%= NUM_LEDS;
  rotation_value%= NUM_LEDS;
  if (rotation_hue < 0)
    rotation_hue = NUM_LEDS -1;
}
