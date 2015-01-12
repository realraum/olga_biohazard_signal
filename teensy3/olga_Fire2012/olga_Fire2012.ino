#include <FastLED.h>

#define LED_PIN     2
#define COLOR_ORDER GRB
#define CHIPSET     WS2812
#define NUM_LEDS    16
#define BRIGHTNESS  220
#define FRAMES_PER_SECOND 30

CRGB leds[NUM_LEDS];
int const sensorPin = A0;    // select the input pin for the potentiometer
int const ledPin = 13;
int const button1Pin = 0;
int sensorValue = 0;
uint8_t mode_ = 0;
#define NUM_MODES 3;
#define MQ3_MIN 100
#define MQ3_MAX 980
//#define MQ3_MIN 224
//#define MQ3_MAX 980

void setup() {
  Serial.begin(9600);
  pinMode(ledPin, OUTPUT); 
  pinMode(sensorPin, INPUT);
  pinMode(button1Pin, INPUT_PULLUP);
  delay(3000); // sanity delay
  FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );
  FastLED.setBrightness( BRIGHTNESS );
}

void loop()
{
  // Add entropy to random number generator; we use a lot of it.
  random16_add_entropy( sensorValue ^ random());

  if (mode_ < 2)
  {
    Fire2012(); // run simulation frame
  
    FastLED.show(); // display this frame
    FastLED.delay(1000 / FRAMES_PER_SECOND);
  } else {
    showAnalogValue();
  }

  sensorValue = analogRead(sensorPin);
  //Serial.println(sensorValue);
 
 //FIXME: MAKE IT AN INTERRUPT ROUTINE ASAP
  if (digitalRead(button1Pin) == LOW) {
    digitalWrite(ledPin, HIGH);
    do {
      fadeall();
      FastLED.delay(1000 / FRAMES_PER_SECOND);
    } while (digitalRead(button1Pin) == LOW);
    //showAnalogValue();
    digitalWrite(ledPin, LOW);
    mode_ = (mode_ +1) % NUM_MODES;
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
uint8_t shifthue(int sv)
{
    uint8_t hue_shift;
    hue_shift = sv / 4; //1024 / 4 == 256
    hue_shift &= 0xE0; //do jumps in values of 32
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
        heatcolor.h = HUE_RED + (uint8_t) (((uint16_t)(HUE_RED - HUE_YELLOW)) * 252 /((uint16_t)heatramp));
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


void showAnalogValue()
{
        for (int i = 0; i < NUM_LEDS; i++) { leds[i] = CRGB::Black; }
        delay(100);
	for (int i = 0; i < min(max(sensorValue - MQ3_MIN,0)* NUM_LEDS * 3 / (MQ3_MAX-MQ3_MIN),NUM_LEDS * 3); i++)
        {
		// Set the i'th led to red
                if (i < NUM_LEDS)
                  leds[i % NUM_LEDS] = CRGB::Blue;
                else if (i > 2* NUM_LEDS)
                  leds[i % NUM_LEDS] = CRGB::Green;
                else
                  leds[i % NUM_LEDS] = CRGB::Red;
		// Show the leds
		FastLED.show(); 
		// Wait a little bit before we loop around and do it again
                FastLED.delay(1000 / FRAMES_PER_SECOND * 2);
	}
        FastLED.delay(5000);
        for (int i = 0; i < NUM_LEDS; i++) { leds[i] = CRGB::Black; }        
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
            hsv2rgb_rainbow(HSVHeatColor( heat[j], shifthue(sensorValue)), leds[j]);
            break;
        }
    }
}

