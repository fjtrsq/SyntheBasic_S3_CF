#include <Wire.h>
#include <Encoder_FJ.h>
#include <SimpleWS2812_RMTv3.h>
#define USER_SETUP_LOADED
#include "TFT_Setup.h"
#include <TFT_eSPI.h>
#include <LittleFS.h>
#include "labelText.h"
#include "numText.h"
#include <driver/i2s.h>
#include <esp_heap_caps.h>
#include <esp_psram.h>
#include <Preferences.h>
#include <cstring>
#include "soc/soc_caps.h"
#include "sdkconfig.h"
#include "Catalogo_Formulas.h"

#if SOC_USB_OTG_SUPPORTED && (!defined(ARDUINO_USB_MODE) || !ARDUINO_USB_MODE) && CONFIG_TINYUSB_MIDI_ENABLED
#include "USB.h"
#include "USBMIDI.h"
#define SYNTH_USB_MIDI_ENABLED 1
USBMIDI usbMidi("SyntheBasic-S3");
#else
#define SYNTH_USB_MIDI_ENABLED 0
#endif


#define LAB_TEXT &Orbitron_Medium_12
#define NUM_TEXT &modern_lcd_78pt7b
TFT_eSPI tft = TFT_eSPI();

// --- Bloque DAC I2S (ESP32-S3) ---
const uint8_t I2S_DIN = 39;
const uint8_t I2S_BCK = 40;
const uint8_t I2S_LCK = 41;

// --- Bloque MIDI UART ---
const uint8_t PIN_RX = 17;
const int8_t PIN_TX = -1;

// --- Bloque Leds ---
const uint8_t PIN_LEDS = 21;
const uint8_t NUM_LEDS = 10;
uint8_t LED_BRIGHT = 30;
SimpleWS2812_RMTv3<10> leds(PIN_LEDS);

// --- Bloque I2C + interrupciones PCF ---
const uint8_t ENCODER_COUNT = 8;
const uint8_t ADDR_ENC = 0x20; 
const uint8_t ADDR_BTN = 0x39; 
const uint8_t PIN_INT_ENC = 4;
const uint8_t PIN_INT_BTN = 5;
const uint8_t PIN_SDA = 1;
const uint8_t PIN_SCL = 2;

// --- Bloque controles locales (encoder + botones) ---
const uint8_t PIN_ENC_A = 6;
const uint8_t PIN_ENC_B = 7;
const uint8_t PIN_ENC_BOT = 15;
const uint8_t PIN_TACTIL = 16;
const uint8_t PIN_AM = 8;
const uint8_t PIN_AZ = 18;
Encoder_FJ encoder = Encoder_FJ(PIN_ENC_A, PIN_ENC_B);

const int8_t encTable[] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};
int8_t subSteps[ENCODER_COUNT] = {0};
unsigned long lastClickTime[ENCODER_COUNT] = {0};
unsigned long lastSafetyCheck = 0;
uint16_t lastEncState = 0xFFFF;
uint8_t lastBtnState = 0xFF;
volatile bool updateEnc = false;
volatile bool updateBtn = false;

int8_t enc = 0;
bool botEnc_ant = 0, botEnc = 0, botEnc_estable = 0;
bool botBlue_ant = 0, botBlue = 0, botBlue_estable = 0;
bool botAz_ant = 0, botAz = 0, botAz_estable = 0;
bool botAm_ant = 0, botAm = 0, botAm_estable = 0;

const unsigned long debounceDelay = 50; // ms
unsigned long lastDebounceTime = 0;
unsigned long lastBlueDebounceTime = 0;

#define TABLE_SIZE_DEFAULT 2048

#define SAMPLE_RATE 44100
#define BUFFER_AUDIO 256
#define NUM_VOICES 8

// Declaración del "candado" para sincronizar los núcleos
portMUX_TYPE audioMux = portMUX_INITIALIZER_UNLOCKED;

const uint16_t AUDIO_SCOPE_SAMPLES = 512;
volatile int16_t audioScopeBuffer[AUDIO_SCOPE_SAMPLES] = {0};
volatile uint16_t audioScopeWriteIndex = 0;
volatile uint32_t audioScopeSerial = 0;

enum Waveform : uint8_t {
  WAVE_SAW = 0,
  WAVE_SINE,
  WAVE_TRIANGLE,
  WAVE_SQUARE,
  WAVE_PULSE,
  WAVE_SUPER,
  WAVE_ORGAN,
  WAVE_CRUSH,
  WAVE_DRIVE,
  WAVE_SINE_EXP,
  WAVE_SAW_EXP,
  WAVE_RECTIFIED,
  WAVE_CHEBYSHEV,
  WAVE_FM_1_1,
  WAVE_FM_1_3,
  WAVE_FM_1_4,
  WAVE_FM_1_5,
  WAVE_FM_FEEDBACK,
  WAVE_VOCAL_AA,
  WAVE_VOCAL_OO,
  WAVE_HOLLOW,
  WAVE_EVEN_HARM,
  WAVE_PINCH,
  WAVE_SINC,
  WAVE_CHIRP,
  WAVE_GRIT,
  WAVE_TRI_INVERT,
  WAVE_FRACTAL,
  WAVE_WIERSTRASS,
  WAVE_PLUCK,
  WAVE_CLARINET,
  WAVE_BELL_METAL,
  WAVE_CELLO,
  WAVE_PARABOLIC,
  WAVE_TRAPEZOID,
  WAVE_SOFT_TRI,
  WAVE_FOLD_SAW,
  WAVE_CROSS_MOD,
  WAVE_COS_EXP,
  WAVE_WARP,
  WAVE_SINE_PWM,
  WAVE_TB_HYBRID,
  WAVE_SUB_HARM,
  WAVE_BEAT,
  WAVE_STEP_8BIT,
  WAVE_HARD_SYNC,
  WAVE_POLYNOMIAL,
  WAVE_PSEUDO_NOISE,
  WAVE_NOISE,
  WAVE_COUNT
};

Preferences nvsPrefs;
MemoryMode memoryMode = MEMORY_PSRAM;
bool hardwareReset = false;
uint16_t tableSize = TABLE_SIZE_DEFAULT;
uint16_t tableMask = TABLE_SIZE_DEFAULT - 1;
uint8_t tableBits = 11;
uint8_t tableFracBits = 21;
uint32_t tableFracMask = 0x001FFFFFUL;

const uint8_t N_OSC = 2;
const uint8_t ICON_W = 60;
const uint8_t ICON_H = 25;
uint8_t cacheGraficaOndas[WAVE_COUNT][ICON_W];
TFT_eSprite menuSprite = TFT_eSprite(&tft);

unsigned long waveEncoderMoveTime = 0;
bool pendingWaveUpdate = false;
const unsigned int WAVE_UPDATE_DELAY = 500;

Waveform oscWaveform [N_OSC] = {WAVE_SAW, WAVE_PULSE};
Waveform oscWaveformEnd [N_OSC] = {WAVE_SAW, WAVE_PULSE};
int16_t* waveformCatalog[WAVE_COUNT] = {nullptr};
int16_t* oscWaveCache[N_OSC] = {nullptr};
int16_t* oscWaveCacheEnd[N_OSC] = {nullptr};

volatile Waveform oscWaveCacheType[N_OSC] = {WAVE_NOISE};
volatile Waveform oscWaveCacheEndType[N_OSC] = {WAVE_NOISE};

const int MOD_DELAY_BUFFER_SIZE = 2048;      // ~46ms @44.1kHz (chorus/flanger)

uint8_t midiStatus = 0;
uint8_t midiData1 = 0;
bool waitingForData2 = false;

enum Type : uint8_t {FLOAT = 0, INT, F01, ONOFF, NAME, CHARSEL, FFILE, NULO};


const uint8_t SOUND_PARAM_PAGES = 7;
const uint8_t FILE_PARAM_PAGE = 7;
const uint8_t ADSR_PARAM_PAGE = 8;
const uint8_t PARAM_PAGES = 9;
const uint8_t PARAMS_PER_PAGE = 8;
const uint8_t PN_LEN = 10; //Preset name len
uint8_t currentPage = 0;
uint8_t lastPage = 0;

const float PAGE1_DEFAULTS[PARAMS_PER_PAGE] = {0.0f,5.0f,0.3f,0.0f,     0.0f,4.0f,8.0f,0.20f};    //LFO
const float PAGE3_DEFAULTS[PARAMS_PER_PAGE] = {0.0f, 0.0f, 0.05f, 0.2f, 0.5f, 0.0f, 0.0f, 0.0f};  //Chorus
float page1EditedValues[PARAMS_PER_PAGE] =    {0.0f,5.0f,0.3f,0.0f,     0.0f,4.0f,8.0f,0.20f};    //LFO
float page3EditedValues[PARAMS_PER_PAGE] =    {0.0f, 0.0f, 0.05f, 0.2f, 0.5f, 0.0f, 0.0f, 0.0f};  //Chorus
bool page1UsingDefaults = false;
bool page3UsingDefaults = false;
bool applyingPageDefaultsToggle = false;

const uint8_t stepsForSeq = 16;

enum ADSR : uint8_t {ADSR_DELAY, ATTACK, ATTACK_LEVEL, DECAY, SUSTAIN, RELEASE, TOTAL_ADSR};
const float ADSR_DEFAULTS[N_OSC][TOTAL_ADSR] = {{0.0f,0.01f,1.0f,0.2f,0.7f,0.3f},{0.0f,0.01f,1.0f,0.2f,0.7f,0.3f}};
float ADSRedited[N_OSC][TOTAL_ADSR] = {{0.0f,0.01f,1.0f,0.2f,0.7f,0.3f},{0.0f,0.01f,1.0f,0.2f,0.7f,0.3f}};
float ADSRvalues[N_OSC][TOTAL_ADSR] = {{0.0f,0.01f,1.0f,0.2f,0.7f,0.3f},{0.0f,0.01f,1.0f,0.2f,0.7f,0.3f}};
bool adsrUsingDefaults[N_OSC] = {false, false};

float synthValue[SOUND_PARAM_PAGES][PARAMS_PER_PAGE] = {  //valores de los encoders
  {0.5f,0.2f,0.2f,1.5f,     1.0f,3.0f,0.0f,0.0f},
  {0.0f,5.0f,0.3f,0.0f,     0.0f,4.0f,8.0f,0.20f},
  {0.0f,0.0f,0.0f,4.0f,     0.5f,0.0f,0.5f,0.5f},
  {0.0f,0.0f,0.05f,0.2f,    0.5f, 0.0f, 0.0f, 0.0f},
  {0.0f,0.0f,0.0f,0.0f,     100.0f, 0.0f,0.0f,4.0f},
  {0.0f,1.0f,0.0f,60.0f,    9.0f,1.0f,100.0f,120.0f},//(float)CHORD_REST
  {0.0f,6.0f, 0.0f,1.0f,    0.65f,1.0f, 0.0f,255.0f},
};

struct ADSRparam {
  const char* name;
  float min;
  float max;
  float step; //encoder increment
  const Type type;
};
const ADSRparam ADSRpage[PARAMS_PER_PAGE] = {
  //Name, min,  max, encoder increment, type
    {"DELAY",  0.0f, 2000.0f, 5.0f, INT},      {"ATTACK",  1.0f, 5000.0f, 5.0f, INT},
    {"ATT LVL",  0.0f, 100.0f, 1.0f, INT},     {"DECAY",  1.0f, 5000.0f, 5.0f, INT},
    {"SUSTAIN",  0.0f, 100.0f, 1.0f, INT},     {"RELEASE",  1.0f, 8000.0f, 5.0f, INT},
    {"OSC MIX",  0.0f, 100.0f, 1.0f, INT},     {"DETUNE",  0.0f, 100.0f, 1.0f, INT}
};

struct SyntParam {
  const char* name;
  float min;
  float max;
  float step; //encoder increment
  const Type type;
};
const SyntParam parametroPages[SOUND_PARAM_PAGES][PARAMS_PER_PAGE] = {
      //Name,    min,  max,  encoder increment,   type
  {
    {"OSC MIX",  0.0f, 1.0f, 0.01f, F01},       {"DETUNE",  0.0f, 1.0f, 0.01f, F01},
    {"%PULSE",  0.1f, 0.9f, 0.05f, F01},        {"M GAIN",  0.1f, 3.0f, 0.01f, FLOAT},
    {"CURVE",  0.1f, 3.0f, 0.05f, FLOAT},       {"N RLEAS",  1.0f, 6.0f, 1.0f, INT},
    {"WT SIZE",  0.0f, 3.0f, 1.0f, NAME},       {"RAM/PSR",  0.0f, 1.0f, 1.0f, ONOFF},
  },
  {
    {"LFO SHP",  0.0f, 4.0f, 1.0f, NAME},       {"RATE",  0.1f, 20.0f, 0.05f, FLOAT},
    {"DEPTH",  0.0f, 1.0f, 0.01f, F01},         {"TARGET",  0.0f, 2.0f, 1.0f, NAME},
    {"ATTACK",  0.0f, 5000.0f, 5.0f, INT},      {"CUTOFF",  0.1f, 20.0f, 0.05f, FLOAT},
    {"PTCH UP",  1.0f, 32.0f, 1.0f, INT},       {"RESONAN",  0.0f, 1.0f, 0.01f, F01},
  },
  {
    {"GLIDE",  0.0f, 1.0f, 0.01f, F01},         {"MORPH",  0.0f, 1.0f, 1.0f, ONOFF},
    {"END A",  0.0f, 49.0f, 1.0f, NAME},        {"END B",  0.0f, 49.0f, 1.0f, NAME},
    {"MIX",  0.0f, 1.0f, 0.01f, F01},           {"MODE",  0.0f, 1.0f, 1.0f, NAME},
    {"VEL",  0.0f, 1.0f, 0.01f, F01},           {"DEPTH",  0.0f, 1.0f, 0.01f, F01},
  },
  { 
    {"CHORUS",  0.0f, 1.0f, 1.0f, ONOFF},       {"MODE",  0.0f, 1.0f, 1.0f, NAME},
    {"RATE",  0.05f, 5.0f, 0.1f, FLOAT},        {"DEPTH",  0.2f, 12.0f, 0.1f, FLOAT},
    {"BASE",  0.5f, 25.0f, 0.1f, FLOAT},        {"FDBACK",  -0.85f, 0.85f, 0.01f, FLOAT},
    {"MIX", 0.0f, 1.0f, 0.01f, F01},            {"XOVR%",  0.0f, 100.0f, 1.0f, INT},
  },
  {
    {"CHORD",  0.0f, 1.0f, 1.0f, ONOFF},        {"TYPE",  0.0f, 7.0f, 1.0f, NAME},
    {"INV",  0.0f, 2.0f, 1.0f, INT},            {"OCT", -1.0f, 1.0f, 1.0f, INT},
    {"VEL%",  30.0f, 120.0f, 1.0f, INT},        {"SPRD", 0.0f, 1.0f, 0.01f, F01},
    {"STRM",  0.0f, 100.0f, 1.0f, INT},         {"DENS",  2.0f, 4.0f, 1.0f, INT},
  },
  {
    {"SEQ",  0.0f, 2.0f, 1.0f, NAME},           {"STEP",  1.0f, 16.0 , 1.0f, INT},//(float)stepsForSeq
    {"MODE",  0.0f, 1.0f, 1.0f, NAME},          {"ROOT",  24.0f, 96.0f, 1.0f, INT},
    {"TYPE",  0.0f, 10.0, 1.0f, NAME},          {"BARS",  1.0f, 8.0f, 1.0f, INT},//(float)(CHORD_TYPE_COUNT - 1)
    {"VEL%",  20.0f, 120.0f, 1.0f, INT},        {"BPM",  60.0f, 200.0f, 1.0f, INT},
  },
  {
    {"ARP",  0.0f, 1.0f, 1.0f, ONOFF},          {"RATE",  1.0f, 20.0f, 0.1f, FLOAT},
    {"MODE", 0.0f, 7.0f, 1.0f, NAME},           {"OCTAVE",  1.0f, 3.0f, 1.0f, INT},
    {"GATE",  0.1f, 0.95f, 0.01f, F01},         {"HOLD",  1.0f, 3.0f, 1.0f, NAME},
    {"SWING", 0.0f, 0.45f, 0.005f, F01},        {"MASK",  1.0f, 255.0f, 1.0f, INT},
  }
};

struct FileParam {
  const char* name;
  int value;
  int min;
  int max;
  int step; //encoder increment
  const Type type;
};
FileParam filePageParams[PARAMS_PER_PAGE] = {
  {"FILE", 0, 0, 0, 1, INT},       {"LOAD", 0, 0, 1, 1, FFILE},
  {"SAVE", 0, 0, 1, 1, FFILE},     {"DEL", 0, 0, 1, 1, FFILE},
  {"CLEAR", 0, 0, 1, 1, FFILE},    {"WRITE", 0, 0, 1, 1, FFILE},
  {"POS", 0, 0, PN_LEN-1, 1, INT}, {"CHAR", 1, 0, 38, 1, CHARSEL},
};

const char* EXTRA_NAMES[] = {"", "LFO:", "OSCstart:", "Chorus:", "Chord:", "Sequencr:", "Mask:", "Preset:", "ADSR:"};
const char* WAVE_NAMES[] = {"Saw", "Sine", "Tri", "Sqr", "Pulse", "SpSaw", "Organ", "Crush",
                            "Warm", "SinEx", "SawEx", "FlRct", "Chebs", "FMcla", "FMmet", 
                            "FMnoi", "FMcrp", "FMfdB", "Voc A", "Voc O", "Holow", "Harmo", 
                            "Pinch", "SyPul", "Chirp", "Grit" ,"TrInv", "Frctl", "Wiers",
                            "Pluck", "Clart", "BelTb", "Cello", "Prbol", "Trapz", "TrSft",
                            "SawFl", "CrosM", "CosEx", "WarpD", "SnPwm", "SwSqr", "SbHar",
                            "Beat", "Stp8b", "SawHd", "Polyn", "NoisP", "Noise"};
const char* OSC_NAME[] = {"A", "B"};
const char* LFO_SHAPE_NAMES[] = {"Sine", "Tri", "Saw", "Sqr", "Pulse"};
const char* LFO_TARGET_NAMES[] = {"Pitch", "Vol", "CutOf"};
const char* FX_MODE_NAMES[] = {"Chrus", "Flger"};
const char* MORPH_MODE_NAMES[] = {"Hard", "Equal"};
const char* ARP_MODE_NAMES[] = {"Up", "Down", "UpDwn", "Rnd", "Pat", "DwnUp", "InOut", "OutIn"};
const char* ARP_HOLD_NAMES[] = {"Off", "Order", "Play", "Stack", };
const char* CHORD_TYPE_NAMES[] = {"Maj", "Min", "Sus2", "Sus4", "Pwr", "Maj7", "Min7", "7", "Playd", "Rest", "End"};
const char* SEQ_MODE_NAMES[] = {"Chord", "Arp"};
const char* SEQ_STATE_NAMES[] = {"Off", "Rec", "On"};
const char* SEQ_DIR_NAMES[] = {"Fwd", "Bwd", "Rnd"};
const char* SEQ_TRANSITION_NAMES[] = {"Trg", "Leg"};
const char* ADSR_ABRV[] = {"DL", "A", "LV", "D", "S", "R"};
const char* TABLE_SIZES[] = {"256", "512", "1024", "2048"};
const uint16_t TABLE_SIZE_VALUES[] = {256, 512, 1024, 2048};
const uint8_t TABLE_SIZE_COUNT = sizeof(TABLE_SIZE_VALUES) / sizeof(TABLE_SIZE_VALUES[0]);

enum EnvState : uint8_t {ENV_IDLE = 0, ENV_DELAY, ENV_ATTACK, ENV_DECAY, ENV_SUSTAIN, ENV_RELEASE};
enum LfoWaveform : uint8_t {LFO_SINE = 0, LFO_TRIANGLE, LFO_SAW, LFO_SQUARE, LFO_PULSE, LFO_WAVE_COUNT};
enum LfoTarget : uint8_t {LFO_TARGET_PITCH = 0, LFO_TARGET_VOLUME, LFO_TARGET_CUTOFF, LFO_TARGET_COUNT};
enum FxMode : uint8_t {FX_CHORUS = 0, FX_FLANGER};
enum MorphMode : uint8_t {MORPH_HARD = 0, MORPH_EQUAL};
enum ArpMode : uint8_t {ARP_UP = 0, ARP_DOWN, ARP_UPDOWN, ARP_RANDOM, ARP_PATTERN, ARP_DOWNUP, ARP_INOUT, ARP_OUTIN, ARP_MODE_COUNT};
enum ArpHold : uint8_t {HOLD_OFF = 0, HOLD_ORDER, HOLD_PLAY, HOLD_STACK, ARP_HOLD_COUNT};
enum SeqTransitionMode : uint8_t {SEQ_TRANS_RETRIG = 0, SEQ_TRANS_LEGATO, SEQ_TRANS_COUNT};
enum ChordType : uint8_t {CHORD_MAJOR = 0,CHORD_MINOR,CHORD_SUS2,CHORD_SUS4,CHORD_POWER,CHORD_MAJ7,CHORD_MIN7,CHORD_DOM7,CHORD_PLAYED,CHORD_REST,CHORD_END,CHORD_TYPE_COUNT};
enum SequencerMode : uint8_t {SEQ_MODE_CHORD = 0,SEQ_MODE_ARP,SEQ_MODE_COUNT};
enum SequencerState : uint8_t {SEQ_STATE_OFF = 0,SEQ_STATE_REC,SEQ_STATE_ON,SEQ_STATE_COUNT};
enum MemoryMode : uint8_t {MEMORY_PSRAM = 0, MEMORY_INTERNAL = 1};


struct Voice {
  uint32_t phase0;
  uint32_t phase1;

  uint32_t phaseInc0;
  uint32_t phaseInc1;
  uint32_t targetPhaseInc0;
  uint32_t targetPhaseInc1;

  float envLevel[N_OSC];
  uint32_t envDelaySamples[N_OSC];
  float velocityGain;
  uint8_t midiNote;

  EnvState envState[N_OSC];
  bool active; 

  // VARIABLES PARA EL LFO DE MORPHING LENTO
  float morphPhase;       // Fase del LFO de esta voz (va de 0.0f a 1.0f)
  float currentMorph;     // El valor final mezclado que usará el bucle de muestras
};

Voice voices[NUM_VOICES];

const float inv32768 = 1.0f / 32768.0f;

uint8_t oscSelect = 0;
uint8_t oscEndSelect = 0;
float varPulse = 0.2f;
float detuneAmount = 0.01f;  // 1% inicial
float oscMix = 0.5f;        // mezcla entre osc0 y osc1

float attackInc[N_OSC] = {0.0f, 0.0f};
float decayInc[N_OSC] = {0.0f, 0.0f};
float releaseInc[N_OSC] = {0.0f, 0.0f};

float morphDepth = 0.5f;       // Intensidad del movimiento (0.0f = estático, 1.0f = barrido total)
float morphBase = 0.5f;                // Posición central del Morph (el valor del potenciómetro físico)
float morphRateHz = 0.5f;        // velocidad del lfo 0.01 lento,  1 = rapido
float morphEnabled = 0.0f;  
MorphMode morphMode = MORPH_HARD;   

float masterGain = 1.5f;  // Ajusta según necesites
float lfoRateHz = 5.0f;
float lfoDepth = 0.3f;
float lfoPhase = 0.0f;
float lfoAttackTime = 0.0f;
float lfoAttackLevel = 1.0f;
float lfoPitchValueSmoothed = 0.0f;
float lfoPitchRatioCached = 1.0f;
uint8_t maxReleaseVoices = 3;
uint8_t lfoPitchUpdateSamples = 8;
uint8_t lfoPitchUpdateCountdown = 0;
volatile bool resetLfoAttackRequested = false;
LfoWaveform lfoWaveform = LFO_SINE;
LfoTarget lfoTarget = LFO_TARGET_PITCH;
float cutoffControl = 4.0f;
float filterCutoffHz = 1200.0f;
float filterState = 0.0f;
float filterResonance = 0.20f;//para compatibilidad
float filterBp = 0.0f;  // Band pass 

float glideTime = 0.0f;
float lastNoteFreq = 440.0f;
bool hasLastNote = false;
float velocityExponent = 1.0f;


float modFxEnabled = 1.0f;
FxMode modFxMode = FX_CHORUS; // 0 = chorus, 1 = flanger
float modFxRateHz = 0.55f;
float modFxDepthMs = 3.5f;
float modFxBaseMs = 10.0f;
float modFxFeedback = 0.15f;
float modFxMix = 0.35f;
float modFxStereo = 0.0f;
float modFxPhase = 0.0f;
int16_t* modDelayBuffer = nullptr;
int modDelayWriteIndex = 0;

bool arpEnabled = false;
float arpRateHz = 6.0f;
ArpMode arpMode = ARP_UP;
int arpOctaves = 1;
float arpGate = 0.65f;
ArpHold arpHold = HOLD_ORDER;
float arpSwing = 0.0f;
uint8_t arpPatternMask = 0xFF;
uint8_t arpHeldNotes[16] = {0};
uint8_t arpHeldVelocities[16] = {0};
uint8_t arpHeldCount = 0;
int arpStepIndex = 0;
bool arpSwingPhase = false;
uint8_t arpCurrentNote = 0;
bool arpGateActive = false;
uint32_t arpSamplesToNextStep = 0;
uint32_t arpGateSamplesLeft = 0;
struct ArpNote {
  uint8_t note;
  uint8_t velocity;
};

bool latchEnabled = false;


bool midiKeysDown[128] = {false};
uint8_t midiKeysPressedCount = 0;
bool chordAssistantEnabled = false;
ChordType chordType = CHORD_MAJOR;
uint8_t chordInversion = 0;
int8_t chordOctaveShift = 0;
float chordVelocityScale = 1.0f;
float chordSpread = 0.0f;
uint8_t chordDensity = 4;
uint8_t chordStrumMs = 0;
const uint8_t MAX_CHORD_NOTES = 8;
uint8_t chordGeneratedCount[128] = {0};
uint8_t chordGeneratedNotes[128][MAX_CHORD_NOTES] = {{0}};
const uint8_t MAX_PENDING_CHORD_NOTES = 16;
struct PendingChordNote {
  bool active;
  uint8_t root;
  uint8_t note;
  uint8_t velocity;
  uint32_t dueMs;
};
PendingChordNote pendingChordNotes[MAX_PENDING_CHORD_NOTES] = {};

bool sequencerEnabled = false;
SequencerState sequencerState = SEQ_STATE_OFF;
uint8_t sequencerEditStep = 0;
uint8_t sequencerPlayStep = 0;
//uint8_t stepsForSeq = 16; declarado mas arriba
const uint8_t SEQUENCER_MAX_PLAYED_NOTES = MAX_CHORD_NOTES;

struct SequencerStep {
  SequencerMode mode;
  uint8_t root;
  ChordType chord;
  uint8_t playedCount;
  uint8_t playedNotes[SEQUENCER_MAX_PLAYED_NOTES];
  uint8_t chordInversion;
  int8_t chordOctaveShift;
  float chordVelocityScale;
  float chordSpread;
  uint8_t chordStrumMs;
  uint8_t chordDensity;
  float arpRateHz;
  ArpMode arpMode;
  int arpOctaves;
  float arpGate;
  float arpSwing;
  uint8_t arpPatternMask;
  uint8_t bars;
  uint8_t velocity;
};
const SequencerStep SEQUENCER_DEFAULT_STEP = {SEQ_MODE_CHORD, 60, CHORD_REST, 0, {0}, 0, 0, 1.0f, 0.0f, 0, 4, 6.0f, ARP_UP, 1, 0.65f, 0.0f, 0xFF, 1, 100};
SequencerStep* sequencerSteps = nullptr;
uint16_t sequencerBpm = 120;
uint32_t sequencerNextStepMs = 0;
uint32_t sequencerStepStartMs = 0;
uint32_t sequencerStepEndMs = 0;
bool sequencerGateActive = false;
SequencerMode sequencerActiveMode = SEQ_MODE_CHORD;
uint8_t sequencerActiveRoot = 0;
uint8_t sequencerActiveArpNote = 0;
uint8_t sequencerArpIndex = 0;
bool sequencerArpNoteOn = false;
uint32_t sequencerArpNextMs = 0;
uint32_t sequencerArpOffMs = 0;
uint8_t sequencerPlayBar = 1;
uint8_t sequencerUiStepShown = 0xFF;
uint8_t sequencerUiBarShown = 0xFF;
SeqTransitionMode sequencerTransitionMode = SEQ_TRANS_LEGATO;

char presetEditName[PN_LEN + 1] = "INIT";
const char PRESET_CHARS[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_";
char presetStatus[24] = "";
uint32_t presetStatusUntil = 0;
int8_t presetActionParam = -1;
char presetActionLabel[8] = "";
uint32_t presetActionUntil = 0;
const uint8_t MAX_PRESET_FILES = 100;
char presetFileNames[MAX_PRESET_FILES][PN_LEN + 1] = {{0}};
uint8_t presetFileCount = 0;
int8_t presetFileSelection = -1;
bool suppressUiRefresh = false;

struct StoredPreset {
  char name[PN_LEN + 1];
  float values[SOUND_PARAM_PAGES][PARAMS_PER_PAGE];
  float oscAdsr[N_OSC][TOTAL_ADSR];
};



void IRAM_ATTR isrEnc() { updateEnc = true; }
void IRAM_ATTR isrBtn() { updateBtn = true; }

void updateEnvelopeRates() {
  for (uint8_t osc = 0; osc < N_OSC; osc++) {
    float attackLevel = constrain(ADSRvalues[osc][ATTACK_LEVEL], 0.0f, 1.0f);
    float sustainLevel = constrain(ADSRvalues[osc][SUSTAIN], 0.0f, 1.0f);
    float releaseLevel = max(attackLevel, sustainLevel);
    attackInc[osc]  = (ADSRvalues[osc][ATTACK] <= 0.0001f) ? attackLevel : attackLevel / (ADSRvalues[osc][ATTACK] * SAMPLE_RATE);
    decayInc[osc]   = (ADSRvalues[osc][DECAY] <= 0.0001f) ? (attackLevel - sustainLevel) : (attackLevel - sustainLevel) / (ADSRvalues[osc][DECAY] * SAMPLE_RATE);
    releaseInc[osc] = (ADSRvalues[osc][RELEASE] <= 0.0001f) ? releaseLevel : releaseLevel / (ADSRvalues[osc][RELEASE] * SAMPLE_RATE);
  }
}

float softClipAudio(float x) {
  x = constrain(x, -2.0f, 2.0f);
  return x / (1.0f + fabsf(x));
}

void muteVoice(uint8_t voiceIndex) {
  if (voiceIndex >= NUM_VOICES) return;
  voices[voiceIndex].envLevel[0] = 0.0f;
  voices[voiceIndex].envLevel[1] = 0.0f;
  voices[voiceIndex].envDelaySamples[0] = 0;
  voices[voiceIndex].envDelaySamples[1] = 0;
  voices[voiceIndex].envState[0] = ENV_IDLE;
  voices[voiceIndex].envState[1] = ENV_IDLE;
  voices[voiceIndex].active = false;
}

float voiceReleaseLevel(uint8_t voiceIndex) {
  return voices[voiceIndex].envLevel[0] + voices[voiceIndex].envLevel[1];
}

bool voiceIsReleasing(uint8_t voiceIndex) {
  if (!voices[voiceIndex].active) return false;
  for (uint8_t osc = 0; osc < N_OSC; osc++) {
    if (voices[voiceIndex].envState[osc] == ENV_RELEASE) return true;
  }
  return false;
}

int findQuietestReleaseVoice() {
  int quietest = -1;
  float quietestLevel = 1000.0f;

  for (int i = 0; i < NUM_VOICES; i++) {
    if (voiceIsReleasing(i)) {
      float level = voiceReleaseLevel(i);
      if (level < quietestLevel) {
        quietestLevel = level;
        quietest = i;
      }
    }
  }

  return quietest;
}

void limitReleaseVoices(uint8_t maxReleaseVoices) {
  while (true) {
    uint8_t releaseCount = 0;
    for (int i = 0; i < NUM_VOICES; i++) {
      if (voiceIsReleasing(i)) releaseCount++;
    }

    if (releaseCount <= maxReleaseVoices) break;

    int quietest = findQuietestReleaseVoice();
    if (quietest < 0) break;
    muteVoice((uint8_t)quietest);
  }
}

int16_t readWaveSample(uint8_t osc, uint32_t index, uint32_t frac){
  //if (oscWaveform[osc] == WAVE_NOISE) { return (int16_t)random(-32768, 32767);}
  int16_t* table = oscWaveCache[osc];
  int16_t a = table[index];
  int16_t b = table[(index + 1) & tableMask];
  return a + ((int32_t)(b - a) * (int32_t)frac >> tableFracBits);
}

float readWaveSampleMorph(uint8_t osc, uint32_t index, uint32_t frac, float morphVal) {
  // Si el oscilador está en modo ruido, no tiene sentido hacer morphing geométrico
  if (oscWaveform[osc] == WAVE_NOISE) {
    return (float)random(-32768, 32767) * inv32768;
  }

  // Apuntamos a las dos tablas en RAM asignadas a este oscilador de la voz 'v'
  int16_t* tableStart = oscWaveCache[osc];
  int16_t* tableEnd   = oscWaveCacheEnd[osc];
  
  uint32_t nextIndex = (index + 1) & tableMask;

  // 1. Interpolación de Punto Fijo para Tabla INICIO (Idéntica a tu fórmula)
  int16_t a_start = tableStart[index];
  int16_t b_start = tableStart[nextIndex];
  int16_t interpStart = a_start + ((int32_t)(b_start - a_start) * (int32_t)frac >> tableFracBits);

  // 2. Interpolación de Punto Fijo para Tabla FIN (Idéntica a tu fórmula)
  int16_t a_end = tableEnd[index];
  int16_t b_end = tableEnd[nextIndex];
  int16_t interpEnd = a_end + ((int32_t)(b_end - a_end) * (int32_t)frac >> tableFracBits);

  // 3. Mezcla final Morphing en flotantes
  float oscStartFloat = (float)interpStart * inv32768;
  float oscEndFloat   = (float)interpEnd * inv32768;

  return oscStartFloat + morphVal * (oscEndFloat - oscStartFloat);
}


float cutoffControlToHz(float control) {
  return 80.0f * control;
}

float resonanceToQ(float resonance) {
  resonance = constrain(resonance, 0.0f, 1.0f);
  return 0.707f + (resonance * resonance * 11.293f);
}

float lfoWaveValue() {
  switch (lfoWaveform) {
    case LFO_TRIANGLE: {
      float norm = lfoPhase / (2.0f * PI);
      return 4.0f * fabsf(norm - 0.5f) - 1.0f;
    }
    case LFO_SAW:
      return (lfoPhase / PI) - 1.0f;
    case LFO_SQUARE:
      return (lfoPhase < PI) ? 1.0f : -1.0f;
    case LFO_PULSE:
      return ((lfoPhase / (2.0f * PI)) < varPulse) ? 1.0f : -1.0f;
    case LFO_SINE:
    default:
      return sinf(lfoPhase);
  }
}

bool allocateAudioBuffer(int16_t** target, size_t samples, const char* name) {
  const size_t bytes = samples * sizeof(int16_t);
  if (memoryMode == MEMORY_INTERNAL) {
    *target = (int16_t*)heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (*target != nullptr) {
      memset(*target, 0, bytes);
      Serial.printf("[MEM] %s -> RAM interna (%u bytes)\n", name, (unsigned int)bytes);
      return true;
    }
    *target = (int16_t*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (*target != nullptr) {
      memset(*target, 0, bytes);
      Serial.printf("[MEM] %s -> PSRAM (fallback, %u bytes)\n", name, (unsigned int)bytes);
      return true;
    }
  } else {
    *target = (int16_t*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (*target != nullptr) {
      memset(*target, 0, bytes);
      Serial.printf("[MEM] %s -> PSRAM (%u bytes)\n", name, (unsigned int)bytes);
      return true;
    }
    *target = (int16_t*)heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (*target != nullptr) {
      memset(*target, 0, bytes);
      Serial.printf("[MEM] %s -> RAM interna (fallback, %u bytes)\n", name, (unsigned int)bytes);
      return true;
    }
  }

  Serial.printf("[MEM] ERROR: no se pudo reservar %s (%u bytes)\n", name, (unsigned int)bytes);
  return false;
}

void loadMemoryModeFromNvs() {
  nvsPrefs.begin("system", true);
  uint8_t stored = nvsPrefs.getUChar("mem_mode", (uint8_t)MEMORY_PSRAM);
  nvsPrefs.end();
  memoryMode = (stored == (uint8_t)MEMORY_INTERNAL) ? MEMORY_INTERNAL : MEMORY_PSRAM;
}

bool saveMemoryModeToNvs(MemoryMode mode) {
  nvsPrefs.begin("system", false);
  bool ok = nvsPrefs.putUChar("mem_mode", (uint8_t)mode) == sizeof(uint8_t);
  nvsPrefs.end();
  if (ok) {
    memoryMode = mode;
    hardwareReset = true;
  }
  return ok;
}

bool isValidTableSize(uint16_t size) {
  for (uint8_t i = 0; i < TABLE_SIZE_COUNT; i++) {
    if (TABLE_SIZE_VALUES[i] == size) return true;
  }
  return false;
}

uint8_t tableSizeToIndex(uint16_t size) {
  for (uint8_t i = 0; i < TABLE_SIZE_COUNT; i++) {
    if (TABLE_SIZE_VALUES[i] == size) return i;
  }
  return tableSizeToIndex(TABLE_SIZE_DEFAULT);
}

uint16_t tableSizeFromIndex(int index) {
  index = constrain(index, 0, TABLE_SIZE_COUNT - 1);
  return TABLE_SIZE_VALUES[index];
}

bool saveTableSizeToNvs(uint16_t newSize) {
  if (!isValidTableSize(newSize)) return false;
  nvsPrefs.begin("system", false);
  bool ok = nvsPrefs.putUShort("table_size", newSize) == sizeof(uint16_t);
  nvsPrefs.end();
  if (ok) {
    // Las tablas de onda y caches ya estan reservadas con el tableSize actual.
    // Aplicar el cambio en caliente puede dejar mascaras/indices apuntando fuera
    // del buffer, asi que solo se guarda y se activa tras reset.
    hardwareReset = true;
  }
  return ok;
}

void loadTableSizeFromNvs() {
  nvsPrefs.begin("system", true);
  uint16_t stored = nvsPrefs.getUShort("table_size", TABLE_SIZE_DEFAULT);
  nvsPrefs.end();
  if (!isValidTableSize(stored)) stored = TABLE_SIZE_DEFAULT;
  tableSize = stored;
  updateTableAddressingConstants();
}

void updateTableAddressingConstants() {
  tableBits = __builtin_ctz((unsigned int)tableSize);
  tableFracBits = 32 - tableBits;
  tableMask = tableSize - 1;
  tableFracMask = (1UL << tableFracBits) - 1UL;
}

bool allocateWaveTables() {
  const size_t tableBytes = tableSize * sizeof(int16_t);

  for (uint8_t wave = WAVE_SAW; wave < WAVE_NOISE; wave++) {
    waveformCatalog[wave] = (int16_t*)heap_caps_malloc(tableBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (waveformCatalog[wave] == nullptr) {
      waveformCatalog[wave] = (int16_t*)heap_caps_malloc(tableBytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
      if (waveformCatalog[wave] != nullptr) {
        Serial.printf("[MEM] Catálogo wave %u -> RAM interna\n", wave);
      }
    } else {
      Serial.printf("[MEM] Catálogo wave %u -> PSRAM\n", wave);
    }
    if (waveformCatalog[wave] == nullptr) return false;
  }

  for (uint8_t osc = 0; osc < N_OSC; osc++) {
    oscWaveCache[osc] = (int16_t*)heap_caps_malloc(tableBytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    oscWaveCacheEnd[osc] = (int16_t*)heap_caps_malloc(tableBytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (oscWaveCache[osc] == nullptr || oscWaveCacheEnd[osc] == nullptr) return false;
    memset(oscWaveCache[osc], 0, tableBytes);
    memset(oscWaveCacheEnd[osc], 0, tableBytes);
  }

  return true;
}

void syncActiveWaveCache(uint8_t osc, Waveform wave, uint8_t end) {
  //osc = 0 || 1 Normal wave or start Wave; 2 || 3 End Wave
  if (osc > 1 || wave >= WAVE_NOISE) return;
  if(!end){
    if (oscWaveCache[osc] == nullptr || waveformCatalog[wave] == nullptr) return;
    if (oscWaveCacheType[osc] == wave) return;
    //if (pendingWaveUpdate && (millis() - waveEncoderMoveTime > WAVE_UPDATE_DELAY)) {
      //portENTER_CRITICAL(&audioMux);
      memcpy(oscWaveCache[osc], waveformCatalog[wave], tableSize * sizeof(int16_t));
      oscWaveCacheType[osc] = wave;
      //portEXIT_CRITICAL(&audioMux);
      //  pendingWaveUpdate = false;
      Serial.printf("Waveform oscilator %d => %s\n", osc+1, WAVE_NAMES[wave]);
    //}

  }
  else {
    if (oscWaveCacheEnd[osc] == nullptr || waveformCatalog[wave] == nullptr) return;
    if (oscWaveCacheEndType[osc] == wave) return;
    //if (pendingWaveUpdate && (millis() - waveEncoderMoveTime > WAVE_UPDATE_DELAY)) {
    //  portENTER_CRITICAL(&audioMux);
      memcpy(oscWaveCacheEnd[osc], waveformCatalog[wave], tableSize * sizeof(int16_t));
      oscWaveCacheEndType[osc] = wave;
    //  portEXIT_CRITICAL(&audioMux);
      Serial.printf("Waveform End oscilator %d => %s\n", osc+1, WAVE_NAMES[wave]);
    //}
  }
} 

bool initAudioMemory() {
  if (!allocateWaveTables()) return false;
  if (!allocateAudioBuffer(&modDelayBuffer, MOD_DELAY_BUFFER_SIZE, "modDelayBuffer")) return false;
  
  size_t totalBytes = (size_t)MOD_DELAY_BUFFER_SIZE + (size_t)(WAVE_NOISE * tableSize  * sizeof(int16_t));                           
  totalBytes += (size_t)(WAVE_NOISE * tableSize  * sizeof(int16_t));
  Serial.printf("[MEM] Reserva audio+tablas: %u bytes (%.2f MB)\n", (unsigned int)totalBytes, totalBytes / (1024.0f * 1024.0f));
  return true;
}

void regeneratePulseTable(float var){
  for (int i = 0; i < tableSize ; i++) {
    float phase = (float)i / tableSize ;
    float pulse = (phase < var) ? 1.0f : -1.0f; 
    waveformCatalog[WAVE_PULSE][i] = (int16_t)(pulse * 32767.0f);
  }
  for(byte n=0;n<N_OSC;n++){
    if(oscWaveform[n] == WAVE_PULSE) {
      oscWaveCacheType[n] = WAVE_COUNT;
      syncActiveWaveCache(n, WAVE_PULSE, 0);
    }
  }
  for(byte n=0;n<N_OSC;n++){
    if(oscWaveformEnd[n] == WAVE_PULSE) {
      oscWaveCacheEndType[n] = WAVE_COUNT;
      syncActiveWaveCache(n, WAVE_PULSE, 1);
    }
  }
}

void generateWaveTables() {
  for (int i = 0; i < tableSize ; i++) {
    float phase = (float)i / tableSize ;

    float saw = (2.0f * phase) - 1.0f;
    waveformCatalog[WAVE_SAW][i] = (int16_t)(constrain(saw, -1.0f, 1.0f) * 32767.0f);

    float sine = sinf(2.0f * PI * phase);
    waveformCatalog[WAVE_SINE][i] = (int16_t)(constrain(sine, -1.0f, 1.0f) * 32767.0f);

    float triangle = 4.0f * fabsf(phase - 0.5f) - 1.0f;
    waveformCatalog[WAVE_TRIANGLE][i] = (int16_t)(constrain(triangle, -1.0f, 1.0f) * 32767.0f);

    float square = (phase < 0.5f) ? 1.0f : -1.0f;
    waveformCatalog[WAVE_SQUARE][i] = (int16_t)(constrain(square, -1.0f, 1.0f) * 32767.0f);

    float pulse = (phase < varPulse) ? 1.0f : -1.0f; // 20% duty
    waveformCatalog[WAVE_PULSE][i] = (int16_t)(constrain(pulse, -1.0f, 1.0f) * 32767.0f);

    float super = (0.60f * saw)
                + (0.25f * sinf((2.0f * PI * phase) + 0.12f))
                + (0.15f * sinf((2.0f * PI * phase * 2.0f) - 0.07f));
    waveformCatalog[WAVE_SUPER][i] = (int16_t)(constrain(super, -1.0f, 1.0f) * 32767.0f);

    float organ = (sine + 0.50f * sinf(2.0f * PI * phase * 2.0f) + 0.30f * sinf(2.0f * PI * phase * 3.0f)) / 1.8f;
    waveformCatalog[WAVE_ORGAN][i] = (int16_t)(constrain(organ, -1.0f, 1.0f) * 32767.0f);

    float crush = roundf(sine * 6.0f) / 6.0f;
    waveformCatalog[WAVE_CRUSH][i] = (int16_t)(constrain(crush, -1.0f, 1.0f) * 32767.0f);

    // 1. Seno Saturado (Cálido, estilo distorsión de válvulas)
    float warm_drive = tanhf(2.5f * sine) / tanhf(2.5f);
    waveformCatalog[WAVE_DRIVE][i] = (int16_t)(constrain(warm_drive, -1.0f, 1.0f) * 32767.0f);

    // 2. Seno Exponencial (Armónicos impares muy agresivos)
    float sine_exp = (sine >= 0.0f ? 1.0f : -1.0f) * (1.0f - expf(-4.0f * fabsf(sine)));
    waveformCatalog[WAVE_SINE_EXP][i] = (int16_t)(constrain(sine_exp, -1.0f, 1.0f) * 32767.0f);

    // 3. Sierra Exponencial (Ataque agresivo y decaimiento curvo, ideal para "plucks")
    float saw_exp = (-1.0f + 2.0f * (1.0f - expf(-3.0f * phase)) / (1.0f - expf(-3.0f)));
    waveformCatalog[WAVE_SAW_EXP][i] = (int16_t)(constrain(saw_exp, -1.0f, 1.0f) * 32767.0f);

    // 4. Rectificación de Onda Completa (Sube el tono una octava de forma distorsionada)
    float full_rect = (fabsf(sine) * 2.0f) - 1.0f;
    waveformCatalog[WAVE_RECTIFIED][i] = (int16_t)(constrain(full_rect, -1.0f, 1.0f) * 32767.0f);

    // 5. Polinomio de Chebyshev (Genera un armónico puro de 4ª orden sin usar senos extras)
    float chebyshev4 = 8.0f * powf(sine, 4.0f) - 8.0f * (sine * sine) + 1.0f;
    waveformCatalog[WAVE_CHEBYSHEV][i] = (int16_t)(constrain(chebyshev4, -1.0f, 1.0f) * 32767.0f);

    // 6. FM Clásica 1:1 (Sonido de campana/órgano eléctrico básico)
    float fm_1_1 = sinf(2.0f * PI * phase + 1.0f * sinf(2.0f * PI * phase));
    waveformCatalog[WAVE_FM_1_1][i] = (int16_t)(constrain(fm_1_1, -1.0f, 1.0f) * 32767.0f);

    // 7. FM Metálica 1:3 (Tono de campana de metal o campana tubular)
    float fm_1_3 = sinf(2.0f * PI * phase + 1.5f * sinf(2.0f * PI * phase * 3.0f));
    waveformCatalog[WAVE_FM_1_3][i] = (int16_t)(constrain(fm_1_3, -1.0f, 1.0f) * 32767.0f);

    // 8. FM Agresiva 1:4 (Sonido industrial, ruidoso e inarmónico)
    float fm_1_4 = sinf(2.0f * PI * phase + 2.0f * sinf(2.0f * PI * phase * 4.0f));
    waveformCatalog[WAVE_FM_1_4][i] = (int16_t)(constrain(fm_1_4, -1.0f, 1.0f) * 32767.0f);

    // 9. Automodulación FM (Genera una sierra matemática muy brillante y limpia)
    float fm_feedback = sinf(2.0f * PI * phase + 0.8f * sine);
    waveformCatalog[WAVE_FM_FEEDBACK][i] = (int16_t)(constrain(fm_feedback, -1.0f, 1.0f) * 32767.0f);

    // 10. Pseudo-Vocal "AA" (Filtro de formante fijo imitando la voz humana)
    float vocal_aa = (sine + 0.8f * sinf(2.0f * PI * phase * 3.0f) + 0.4f * sinf(2.0f * PI * phase * 5.0f)) / 2.2f;
    waveformCatalog[WAVE_VOCAL_AA][i] = (int16_t)(constrain(vocal_aa, -1.0f, 1.0f) * 32767.0f);

    // 11. Pseudo-Vocal "OO" (Formante más cerrado y oscuro)
    float vocal_oo = (sine + 0.6f * sinf(2.0f * PI * phase * 2.0f) + 0.1f * sinf(2.0f * PI * phase * 3.0f)) / 1.7f;
    waveformCatalog[WAVE_VOCAL_OO][i] = (int16_t)(constrain(vocal_oo, -1.0f, 1.0f) * 32767.0f);

    // 12. Onda Hueca / "Hollow" (Solo armónicos impares, textura similar al clarinete)
    float hollow = (sine + 0.33f * sinf(2.0f * PI * phase * 3.0f) + 0.2f * sinf(2.0f * PI * phase * 5.0f)) / 1.53f;
    waveformCatalog[WAVE_HOLLOW][i] = (int16_t)(constrain(hollow, -1.0f, 1.0f) * 32767.0f);

    // 13. Súper Armónicos Pares (Suena extremadamente brillante, una octava por encima)
    float even_harmonics = (sinf(2.0f * PI * phase * 2.0f) + 0.5f * sinf(2.0f * PI * phase * 4.0f) + 0.25f * sinf(2.0f * PI * phase * 6.0f)) / 1.75f;
    waveformCatalog[WAVE_EVEN_HARM][i] = (int16_t)(constrain(even_harmonics, -1.0f, 1.0f) * 32767.0f);

    // 14. Phase Pinch (Deforma el tiempo; suave al inicio, hiper-frecuente al final)
    float phase_pinch = sinf(2.0f * PI * powf(phase, 1.7f));
    waveformCatalog[WAVE_PINCH][i] = (int16_t)(constrain(phase_pinch, -1.0f, 1.0f) * 32767.0f);

    // 15. Sinc Pulse / Función Espectral (Produce pulsos agudos aislados de alta fidelidad)
    float sync_offset = (phase - 0.5f) * 15.0f; // Multiplicador controla el número de anillos
    float sinc_pulse = (fabsf(sync_offset) < 0.0001f) ? 1.0f : sinf(sync_offset) / sync_offset;
    waveformCatalog[WAVE_SINC][i] = (int16_t)(constrain(sinc_pulse, -1.0f, 1.0f) * 32767.0f);

    // 16. Onda Chirp (La frecuencia se duplica progresivamente a lo largo del ciclo)
    float chirp = sinf(2.0f * PI * phase * (1.0f + phase));
    waveformCatalog[WAVE_CHIRP][i] = (int16_t)(constrain(chirp, -1.0f, 1.0f) * 32767.0f);

    // 17. Seno partido en ventanas (Grit / Modulación por Anillo Interna)
    float grit = sine * (sinf(2.0f * PI * phase * 5.0f) > 0.0f ? 1.0f : -1.0f);
    waveformCatalog[WAVE_GRIT][i] = (int16_t)(constrain(grit, -1.0f, 1.0f) * 32767.0f);

    // 18. Triángulo Invertido Absoluto (Corta la onda por la mitad generando picos duros)
    float tri_invert = 1.0f - 2.0f * fabsf(triangle);
    waveformCatalog[WAVE_TRI_INVERT][i] = (int16_t)(constrain(tri_invert, -1.0f, 1.0f) * 32767.0f);

    // 19. Fractal por Módulo (Multi-rampa matemática agresiva tipo glitch)
    float fractal_mod = -1.0f + 2.0f * fmodf(phase * 4.0f, 1.0f);
    waveformCatalog[WAVE_FRACTAL][i] = (int16_t)(constrain(fractal_mod, -1.0f, 1.0f) * 32767.0f);

    // 20. Micro-Ruido Fractal (Onda Weierstrass simplificada, textura arenosa/industrial)
    float noise_fractal = (sine + 0.5f * cosf(2.0f * PI * phase * 3.0f) + 0.25f * sinf(2.0f * PI * phase * 9.0f)) / 1.75f;
    waveformCatalog[WAVE_WIERSTRASS][i] = (int16_t)(constrain(noise_fractal, -1.0f, 1.0f) * 32767.0f);

    // 21. Cuerda Pulsada Exponencial (Ataque ultra rápido y decaimiento no lineal)
    float string_pluck = saw * expf(-2.0f * phase);
    waveformCatalog[WAVE_PLUCK][i] = (int16_t)(constrain(string_pluck, -1.0f, 1.0f) * 32767.0f);

    // 22. Clarinete / Viento Madera (Predominio extremo de armónicos impares con rampa suave)
    float clarinet = (sine + 0.5f * sinf(2.0f * PI * phase * 3.0f) + 0.25f * sinf(2.0f * PI * phase * 5.0f)) * (1.0f - phase);
    waveformCatalog[WAVE_CLARINET][i] = (int16_t)(constrain(clarinet, -1.0f, 1.0f) * 32767.0f);

    // 23. Metal Resonante / Campana Tibetana (Inarmónicos densos que simulan resonancia metálica)
    float bell_metal = (sine + 0.7f * sinf(2.0f * PI * phase * 2.71f) + 0.4f * sinf(2.0f * PI * phase * 5.43f)) / 2.1f;
    waveformCatalog[WAVE_BELL_METAL][i] = (int16_t)(constrain(bell_metal, -1.0f, 1.0f) * 32767.0f);

    // 24. Pseudo-Cello (Combinación aditiva asimétrica que imita el frotado de una cuerda)
    float cello = (saw + 0.5f * sinf(2.0f * PI * phase) + 0.25f * cosf(2.0f * PI * phase * 2.0f)) / 1.75f;
    waveformCatalog[WAVE_CELLO][i] = (int16_t)(constrain(cello, -1.0f, 1.0f) * 32767.0f);

    // 25. Sierra Parabólica (Curvatura cuadrática, un sonido intermedio entre triángulo y sierra)
    float saw_parabolic = (saw * saw) * (phase < 0.5f ? 1.0f : -1.0f);
    waveformCatalog[WAVE_PARABOLIC][i] = (int16_t)(constrain(saw_parabolic, -1.0f, 1.0f) * 32767.0f);

    // 26. Onda Trapezoidal (Cuadrada con rampas de subida y bajada suaves, menos aliasing)
    float trapezoid = saw * 2.0f;
    if (trapezoid > 1.0f) trapezoid = 1.0f;
    else if (trapezoid < -1.0f) trapezoid = -1.0f;
    waveformCatalog[WAVE_TRAPEZOID][i] = (int16_t)(constrain(trapezoid, -1.0f, 1.0f) * 32767.0f);

    // 27. Triángulo con "Soft Clipping" (Aplana los extremos del triángulo imitando un circuito saturado)
    float soft_tri = sinf(triangle * (PI / 2.0f));
    waveformCatalog[WAVE_SOFT_TRI][i] = (int16_t)(constrain(soft_tri, -1.0f, 1.0f) * 32767.0f);

    // 28. Sierra Asimétrica Plegada (Sube linealmente, baja reflejando la fase)
    float folded_saw = saw;
    if (folded_saw > 0.5f) folded_saw = 1.0f - folded_saw;
    waveformCatalog[WAVE_FOLD_SAW][i] = (int16_t)(constrain(folded_saw * 2.0f, -1.0f, 1.0f) * 32767.0f);

    // 29. FM 1:5 Escarchada (Genera texturas cristalinas / gélidas muy brillantes)
    float fm_1_5 = sinf(2.0f * PI * phase + 1.8f * sinf(2.0f * PI * phase * 5.0f));
    waveformCatalog[WAVE_FM_1_5][i] = (int16_t)(constrain(fm_1_5, -1.0f, 1.0f) * 32767.0f);

    // 30. Modulación Cruzada (AM + FM simultánea dentro del mismo ciclo)
    float cross_mod = sinf(2.0f * PI * phase + 1.2f * sinf(2.0f * PI * phase * 2.0f)) * (0.5f + 0.5f * sine);
    waveformCatalog[WAVE_CROSS_MOD][i] = (int16_t)(constrain(cross_mod, -1.0f, 1.0f) * 32767.0f);

    // 31. Modulación por Coseno Exponencial (Efecto de formante sintético agresivo)
    float cos_exp = sinf(2.0f * PI * phase) * cosf(10.0f * phase * phase);
    waveformCatalog[WAVE_COS_EXP][i] = (int16_t)(constrain(cos_exp, -1.0f, 1.0f) * 32767.0f);

    // 32. Phase Warp Dual (Dos velocidades de fase distintas colisionando en el medio)
    float warp_phase = (phase < 0.5f) ? powf(phase * 2.0f, 2.0f) * 0.5f : 0.5f + (1.0f - powf((1.0f - phase) * 2.0f, 2.0f)) * 0.5f;
    float phase_warp = sinf(2.0f * PI * warp_phase);
    waveformCatalog[WAVE_WARP][i] = (int16_t)(constrain(phase_warp, -1.0f, 1.0f) * 32767.0f);

    // 33. Seno PWM Fijo (Simula un ancho de pulso pero usando ciclos senoidales modificados)
    float sine_pwm = (phase < 0.3f) ? sinf(2.0f * PI * phase * (0.5f / 0.3f)) : sinf(2.0f * PI * (phase - 0.3f) * (0.5f / 0.7f) + PI);
    waveformCatalog[WAVE_SINE_PWM][i] = (int16_t)(constrain(sine_pwm, -1.0f, 1.0f) * 32767.0f);

    // 34. Sierra + Cuadrada (El sonido clásico de los bajos Roland TB-03 / TB-303 híbrido)
    float tb_hybrid = (saw + square) * 0.5f;
    waveformCatalog[WAVE_TB_HYBRID][i] = (int16_t)(constrain(tb_hybrid, -1.0f, 1.0f) * 32767.0f);

    // 35. Onda Sub-Bajo Harmónica (Un seno limpio acompañado de una octava inferior sutil)
    float sub_harmonic = (sine * 0.75f) + (sinf(2.0f * PI * phase * 0.5f) * 0.25f);
    waveformCatalog[WAVE_SUB_HARM][i] = (int16_t)(constrain(sub_harmonic, -1.0f, 1.0f) * 32767.0f);

    // 36. Interferencia de Fase (Dos senos muy cercanos en frecuencia que crean un batido fijo)
    float phase_beat = (sine + sinf(2.0f * PI * phase * 1.05f)) * 0.5f;
    waveformCatalog[WAVE_BEAT][i] = (int16_t)(constrain(phase_beat, -1.0f, 1.0f) * 32767.0f);


    // 37. Escalera de 8 Bits / Resonancia Cuántica (Divide el ciclo en 8 escalones discretos planos)
    float step_8bit = floorf(sine * 4.0f) / 4.0f;
    waveformCatalog[WAVE_STEP_8BIT][i] = (int16_t)(constrain(step_8bit, -1.0f, 1.0f) * 32767.0f);

    // 38. Sync Incompleto / Hard Sync (Una sierra que se reinicia a mitad de camino tres veces)
    float hard_sync = (2.0f * fmodf(phase * 3.0f, 1.0f)) - 1.0f;
    waveformCatalog[WAVE_HARD_SYNC][i] = (int16_t)(constrain(hard_sync, -1.0f, 1.0f) * 32767.0f);

    // 39. Onda Polinomial (Curva suave de tercer orden basada exclusivamente en álgebra)
    float poly_wave = 4.0f * phase * (1.0f - phase) * (phase - 0.5f) * 5.2f; 
    waveformCatalog[WAVE_POLYNOMIAL][i] = (int16_t)(constrain(poly_wave, -1.0f, 1.0f) * 32767.0f);

    // 40. Ruido Pseudo-Aleatorio Sincronizado (Textura metálica/chile de interferencia digital fija)
    float pseudo_noise = sinf(phase * 50.0f) * cosf(phase * 12.0f);
    waveformCatalog[WAVE_PSEUDO_NOISE][i] = (int16_t)(constrain(pseudo_noise, -1.0f, 1.0f) * 32767.0f);
  }

  syncActiveWaveCache(0, oscWaveform[0], 0);
  syncActiveWaveCache(1, oscWaveform[1], 0);
  syncActiveWaveCache(0, oscWaveformEnd[0], 1);
  syncActiveWaveCache(1, oscWaveformEnd[1], 1);
}

void setupI2S() {
  i2s_config_t config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = 0,
    .dma_buf_count = 12,
    .dma_buf_len = BUFFER_AUDIO,
    .use_apll = true
  };

  i2s_pin_config_t pins = {
    .bck_io_num = I2S_BCK,
    .ws_io_num = I2S_LCK,
    .data_out_num = I2S_DIN,
    .data_in_num = I2S_PIN_NO_CHANGE
  };

  i2s_driver_install(I2S_NUM_0, &config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pins);
}

void audioTask(void *param) {
  int16_t buffer[BUFFER_AUDIO];          // 128 muestras estéreo
  size_t bytesWritten;
  float dacDiagPhase = 0.0f;
  uint32_t audioBlockCounter = 0;

  while (true) {
    
    const float modFxFeedbackClamped = constrain(modFxFeedback, -0.95f, 0.95f);

    bool anyVoiceActive = false;
    uint8_t activeVoiceCount = 0;
    for (int v = 0; v < NUM_VOICES; v++) {
      if (voices[v].active) {
        anyVoiceActive = true;
        activeVoiceCount++;
      }
    }
    const float polyVoiceGain = (activeVoiceCount > 1) ? (1.0f / sqrtf((float)activeVoiceCount)) : 1.0f;
    
    const bool runModFx = (modFxEnabled >= 0.5f);
    //const bool runReverb = (reverbMix > 0.005f);
    const float lfoAttackInc = (lfoAttackTime <= 0.0001f) ? 1.0f : 1.0f / (lfoAttackTime * SAMPLE_RATE);
    const float lfoPhaseInc = (2.0f * PI * lfoRateHz) / SAMPLE_RATE;
    const float modFxPhaseInc = (2.0f * PI * modFxRateHz) / SAMPLE_RATE;
    const bool cutoffModulated = (lfoTarget == LFO_TARGET_CUTOFF);
    const float baseCutoffHz = cutoffControlToHz(cutoffControl);
    float filterG = 0.0f;
    float filterR = 0.0f;
    float filterH = 0.0f;

    if (!cutoffModulated) {
      float cutoffHz = constrain(baseCutoffHz, 20.0f, SAMPLE_RATE * 0.45f);
      filterG = tanf(PI * cutoffHz / SAMPLE_RATE);
      float q = resonanceToQ(filterResonance);
      filterR = 1.0f / (2.0f * q);
      filterH = 1.0f / (1.0f + (2.0f * filterR * filterG) + (filterG * filterG));
    }

    uint16_t scopeWrite = audioScopeWriteIndex;

    float morphInc = morphRateHz / SAMPLE_RATE; 


    for (int i = 0; i < BUFFER_AUDIO; i += 2) {


      float mix = 0.0f;
      if (resetLfoAttackRequested) {
        lfoAttackLevel = 0.0f;
        lfoPitchValueSmoothed = lfoWaveValue();
        lfoPitchRatioCached = 1.0f;
        lfoPitchUpdateCountdown = 0;
        resetLfoAttackRequested = false;
      }

      if (anyVoiceActive) {
        lfoAttackLevel += lfoAttackInc;
        if (lfoAttackLevel > 1.0f) lfoAttackLevel = 1.0f;
      }
      else {
        lfoAttackLevel = 0.0f;
      }



      float effectiveLfoDepth = lfoDepth * lfoAttackLevel;
      float lfoValue = lfoWaveValue();
      float pitchRatio = 1.0f;
      float ampMod = 1.0f;

      if (lfoTarget == LFO_TARGET_PITCH) {
        if (lfoPitchUpdateCountdown == 0) {
          // Suaviza formas no sinusoidales y evita exp2f() en cada muestra.
          if (!anyVoiceActive) lfoPitchValueSmoothed = lfoValue;
          lfoPitchValueSmoothed += (lfoValue - lfoPitchValueSmoothed) * 0.003f;
          float pitchCents = lfoPitchValueSmoothed * effectiveLfoDepth * 100.0f;
          lfoPitchRatioCached = exp2f(pitchCents / 1200.0f);
          lfoPitchUpdateCountdown = lfoPitchUpdateSamples;
        }
        lfoPitchUpdateCountdown--;
        pitchRatio = lfoPitchRatioCached;
      }
      else if (lfoTarget == LFO_TARGET_VOLUME) {
        lfoPitchRatioCached = 1.0f;
        lfoPitchUpdateCountdown = 0;
        ampMod = 1.0f - (0.5f * effectiveLfoDepth * (lfoValue + 1.0f));
      }
      else if (lfoTarget == LFO_TARGET_CUTOFF) {
        lfoPitchRatioCached = 1.0f;
        lfoPitchUpdateCountdown = 0;
        float currentCutoffHz = baseCutoffHz + (lfoValue * effectiveLfoDepth * 2500.0f);
        currentCutoffHz = constrain(currentCutoffHz, 80.0f, 8000.0f);
        filterG = tanf(PI * currentCutoffHz / SAMPLE_RATE);
        float q = resonanceToQ(filterResonance);
        filterR = 1.0f / (2.0f * q);
        filterH = 1.0f / (1.0f + (2.0f * filterR * filterG) + (filterG * filterG));
      }
      else {
        lfoPitchValueSmoothed = lfoValue;
        lfoPitchRatioCached = 1.0f;
        lfoPitchUpdateCountdown = 0;
        pitchRatio = 1.0f;
      }

      lfoPhase += lfoPhaseInc;
      if (lfoPhase >= 2.0f * PI) lfoPhase -= 2.0f * PI;

      if (arpEnabled) {
        if (arpHeldCount == 0) {
          if (arpGateActive) {
            noteOff(arpCurrentNote);
            arpGateActive = false;
          }
          arpSamplesToNextStep = 0;
          arpGateSamplesLeft = 0;
        }
        else {
          if (arpGateActive && arpGateSamplesLeft > 0) {
            arpGateSamplesLeft--;
            if (arpGateSamplesLeft == 0) {
              noteOff(arpCurrentNote);
              arpGateActive = false;
            }
          }

          if (arpSamplesToNextStep > 0) {
            arpSamplesToNextStep--;
          }

          if (arpSamplesToNextStep == 0) {
            ArpNote next = arpGetNextNote();
            if (arpGateActive) {
              noteOff(arpCurrentNote);
              arpGateActive = false;
            }
            if (next.velocity > 0) {
              arpCurrentNote = next.note; 
              noteOn(next.note, next.velocity);
              arpGateActive = true;
            }

            uint32_t stepSamp = arpStepSamples();
            arpSamplesToNextStep = stepSamp;
            arpGateSamplesLeft = (uint32_t)(stepSamp * arpGate);
            if (arpGateSamplesLeft < 1) arpGateSamplesLeft = 1;
          }
        }
      }

      for (int v = 0; v < NUM_VOICES; v++) {

        if (!voices[v].active) continue;

        // ---------- ENVOLVENTE ----------
        bool allOscIdle = true;
        for (uint8_t osc = 0; osc < N_OSC; osc++) {
          switch (voices[v].envState[osc]) {
            case ENV_DELAY:
              if (voices[v].envDelaySamples[osc] > 0) {
                voices[v].envDelaySamples[osc]--;
              }
              if (voices[v].envDelaySamples[osc] == 0) {
                voices[v].envState[osc] = ENV_ATTACK;
              }
              break;

            case ENV_ATTACK: {
              float attackLevel = constrain(ADSRvalues[osc][ATTACK_LEVEL], 0.0f, 1.0f);
              voices[v].envLevel[osc] += attackInc[osc];
              if (voices[v].envLevel[osc] >= attackLevel) {
                voices[v].envLevel[osc] = attackLevel;
                voices[v].envState[osc] = ENV_DECAY;
              }
              break;
            }

            case ENV_DECAY: {
              float sustainLevel = constrain(ADSRvalues[osc][SUSTAIN], 0.0f, 1.0f);
              float step = decayInc[osc];
              voices[v].envLevel[osc] -= step;
              if ((step >= 0.0f && voices[v].envLevel[osc] <= sustainLevel) ||
                  (step < 0.0f && voices[v].envLevel[osc] >= sustainLevel)) {
                voices[v].envLevel[osc] = sustainLevel;
                voices[v].envState[osc] = ENV_SUSTAIN;
              }
              break;
            }

            case ENV_RELEASE:
              voices[v].envLevel[osc] -= releaseInc[osc];
              if (voices[v].envLevel[osc] <= 0.0f) {
                voices[v].envLevel[osc] = 0.0f;
                voices[v].envState[osc] = ENV_IDLE;
                voices[v].envDelaySamples[osc] = 0;
              }
              break;

            case ENV_SUSTAIN:
            case ENV_IDLE:
            default:
              break;
          }

          if (voices[v].envState[osc] != ENV_IDLE) allOscIdle = false;
        }

        if (allOscIdle) {
          voices[v].active = false;
          resetLfoAttackRequested = true;
        }

        // ---------- OSCILADORES ----------
        if (glideTime > 0.0001f) {
          uint32_t glideStep0 = (uint32_t)(fabs((float)((int32_t)voices[v].targetPhaseInc0 - (int32_t)voices[v].phaseInc0)) / (glideTime * SAMPLE_RATE));
          uint32_t glideStep1 = (uint32_t)(fabs((float)((int32_t)voices[v].targetPhaseInc1 - (int32_t)voices[v].phaseInc1)) / (glideTime * SAMPLE_RATE));
          if (glideStep0 < 1) glideStep0 = 1;
          if (glideStep1 < 1) glideStep1 = 1;

          if (voices[v].phaseInc0 < voices[v].targetPhaseInc0) {
            uint32_t next = voices[v].phaseInc0 + glideStep0;
            voices[v].phaseInc0 = (next > voices[v].targetPhaseInc0) ? voices[v].targetPhaseInc0 : next;
          }
          else if (voices[v].phaseInc0 > voices[v].targetPhaseInc0) {
            uint32_t next = voices[v].phaseInc0 - glideStep0;
            voices[v].phaseInc0 = (next < voices[v].targetPhaseInc0) ? voices[v].targetPhaseInc0 : next;
          }

          if (voices[v].phaseInc1 < voices[v].targetPhaseInc1) {
            uint32_t next = voices[v].phaseInc1 + glideStep1;
            voices[v].phaseInc1 = (next > voices[v].targetPhaseInc1) ? voices[v].targetPhaseInc1 : next;
          }
          else if (voices[v].phaseInc1 > voices[v].targetPhaseInc1) {
            uint32_t next = voices[v].phaseInc1 - glideStep1;
            voices[v].phaseInc1 = (next < voices[v].targetPhaseInc1) ? voices[v].targetPhaseInc1 : next;
          }
        }
        else {
          voices[v].phaseInc0 = voices[v].targetPhaseInc0;
          voices[v].phaseInc1 = voices[v].targetPhaseInc1;
        }

        if (morphEnabled && voices[v].active) {
          // 1. Avanzar la fase del LFO de la voz según el tamaño del bloque
          voices[v].morphPhase += morphInc;
          if (voices[v].morphPhase >= 1.0f) voices[v].morphPhase -= 1.0f;

          // 2. Calcular la oscilación senoidal
          float lfoVal = sinf(2.0f * PI * voices[v].morphPhase);

          if(morphMode == MORPH_HARD){
            // 3. Mezclar con el valor base del pote de morph y la intensidad (Depth)
            voices[v].currentMorph = morphBase + (lfoVal * morphDepth);
            voices[v].currentMorph = constrain(voices[v].currentMorph, 0.0f, 1.0f);
          }
          else { //morphMode == MORPH_EQUAL
            // 3. CALCULAR EL ESPACIO MÁXIMO DISPONIBLE PARA QUE SEA SIMÉTRICO
            // Distancia al borde izquierdo (0.0) es 'morphBase'. Distancia al derecho (1.0) es '1.0 - morphBase'.
            float maxAllowedDepth = (morphBase < 0.5f) ? morphBase : (1.0f - morphBase);

            // 4. Escalar la intensidad del potenciómetro (morphDepth) según el espacio real disponible
            float safeDepth = morphDepth * maxAllowedDepth;

            // 5. Calcular la posición final (Ya no necesita 'constrain' porque matemáticamente nunca se saldrá de)
            voices[v].currentMorph = morphBase + (lfoVal * safeDepth);
          }
        }
        else{
          voices[v].currentMorph = 0.0f; 
        }

        // ---------- EXTRAER ÍNDICES ----------
        uint32_t index0 = voices[v].phase0 >> tableFracBits;
        uint32_t index1 = voices[v].phase1 >> tableFracBits;

        uint32_t frac0 = voices[v].phase0 & tableFracMask;
        uint32_t frac1 = voices[v].phase1 & tableFracMask;

        float osc0, osc1;

        if (morphEnabled) {
          // Pasamos voices[v].currentMorph que ya fue calculado eficientemente por bloque
          osc0 = readWaveSampleMorph(0, index0, frac0, voices[v].currentMorph);
          osc1 = readWaveSampleMorph(1, index1, frac1, voices[v].currentMorph);
        } 
        else {
          int16_t interp0 = readWaveSample(0, index0, frac0);
          int16_t interp1 = readWaveSample(1, index1, frac1);

          osc0 = interp0 * inv32768;
          osc1 = interp1 * inv32768;
        }

        float sample = (osc0 * (1.0f - oscMix) * voices[v].envLevel[0]) +
                       (osc1 * oscMix * voices[v].envLevel[1]);

        mix += sample * voices[v].velocityGain * ampMod * 0.75f;

        // Avance de fase con LFO sobre tono
        uint32_t modPhaseInc0 = (uint32_t)(voices[v].phaseInc0 * pitchRatio);
        uint32_t modPhaseInc1 = (uint32_t)(voices[v].phaseInc1 * pitchRatio);
        voices[v].phase0 += modPhaseInc0;
        voices[v].phase1 += modPhaseInc1;
      }

      mix *= polyVoiceGain;
      mix = softClipAudio(mix) * 1.6f;
 
      // Filtro low-pass resonante TPT SVF. Coeficientes por bloque salvo cutoff modulado.
      float hp = (mix - ((2.0f * filterR + filterG) * filterBp) - filterState) * filterH;
      float bp = (filterG * hp) + filterBp;
      float lp = (filterG * bp) + filterState;
      filterBp = constrain((2.0f * bp) - filterBp, -4.0f, 4.0f);
      filterState = constrain((2.0f * lp) - filterState, -4.0f, 4.0f);
      float filtered = constrain(lp, -1.0f, 1.0f);

      float modOut = filtered;

      if (runModFx) {
        // Chorus/Flanger ligero: una sola línea de retardo modulada + interpolación lineal.
        float modeBlend = constrain(roundf(modFxMode), 0.0f, 1.0f); // 0 chorus, 1 flanger
        float lfo = sinf(modFxPhase);
        float depthSamples = (modFxDepthMs * SAMPLE_RATE) * 0.001f;
        float baseSamples = (modFxBaseMs * SAMPLE_RATE) * 0.001f;
        float minSamples = 2.0f + (modeBlend * 1.0f);
        float maxSamples = 36.0f + (modeBlend * 10.0f);
        float dSamp = baseSamples + (lfo * depthSamples * (0.3f + 0.7f * modFxStereo));
        dSamp = constrain(dSamp, minSamples, maxSamples);

        int readA = modDelayWriteIndex - (int)dSamp;
        if (readA < 0) readA += MOD_DELAY_BUFFER_SIZE;
        int readB = readA + 1;
        if (readB >= MOD_DELAY_BUFFER_SIZE) readB = 0;
        float frac = dSamp - (int)dSamp;
        float wetA = modDelayBuffer[readA] * inv32768;
        float wetB = modDelayBuffer[readB] * inv32768;
        float modWet = wetA + (wetB - wetA) * frac;

        float modWrite = filtered + (modWet * modFxFeedbackClamped * 0.75f);
        modDelayBuffer[modDelayWriteIndex] = (int16_t)(constrain(modWrite, -1.0f, 1.0f) * 32767.0f);
        modDelayWriteIndex++;
        if (modDelayWriteIndex >= MOD_DELAY_BUFFER_SIZE) modDelayWriteIndex = 0;

        modOut = (filtered * (1.0f - modFxMix)) + (modWet * modFxMix);
        modFxPhase += (2.0f * PI * modFxRateHz) / SAMPLE_RATE;
        modFxPhase += modFxPhaseInc;
        if (modFxPhase > 2.0f * PI) modFxPhase -= 2.0f * PI;
      }
      float outL = modOut;
      float outR = modOut;
      float fxOutL = outL;
      float fxOutR = outR;
      
      int16_t outL_i = (int16_t)(softClipAudio(fxOutL * masterGain) * 32767);
      int16_t outR_i = (int16_t)(softClipAudio(fxOutR * masterGain) * 32767);

      buffer[i]     = outL_i;
      buffer[i + 1] = outR_i;
      audioScopeBuffer[scopeWrite] = outL_i;
      scopeWrite++;
      if (scopeWrite >= AUDIO_SCOPE_SAMPLES) scopeWrite = 0;
    }
    audioScopeWriteIndex = scopeWrite;
    audioScopeSerial += 1;

    // Enviar bloque al DAC por I2S
    i2s_write(I2S_NUM_0, buffer, sizeof(buffer), &bytesWritten, portMAX_DELAY);
    audioBlockCounter++;
    if ((audioBlockCounter & 0x1F) == 0) {
      vTaskDelay(1);
    } else {
      taskYIELD();
    }
  }
}


void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000);
  if (psramFound()) {
    Serial.printf("[MEM] PSRAM detectada: %u bytes\n", (unsigned int)ESP.getPsramSize());
  } else {
    Serial.println("[MEM] PSRAM no detectada, usando RAM interna");
  }
  loadMemoryModeFromNvs();
  synthValue[0][7] = (memoryMode == MEMORY_INTERNAL) ? 1.0f : 0.0f;
  Serial.printf("[MEM] Modo reserva buffers: %s\n", memoryMode == MEMORY_INTERNAL ? "RAM interna primero" : "AUTO (PSRAM primero)");

  loadTableSizeFromNvs();
  synthValue[0][6] = (float)tableSizeToIndex(tableSize);
  Serial.printf("[WAV] TABLE_SIZE: %u (bits=%u fracBits=%u)\n", tableSize, tableBits, tableFracBits);

  if (!initAudioMemory()) {
    Serial.println("[MEM] ERROR FATAL reservando memoria de audio");
    while (true) delay(1000);
  }

  setupI2S();
  Serial2.begin(31250, SERIAL_8N1, PIN_RX, PIN_TX);
  #if SYNTH_USB_MIDI_ENABLED
    usbMidi.begin();
    USB.begin();
    Serial.println("USB MIDI configurado");
  #else
    Serial.println("USB MIDI no disponible: usa USB OTG + TinyUSB MIDI en la configuracion de placa");
  #endif

  randomSeed((uint32_t)micros());
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS init failed");
  }
  refreshPresetFileList(false);
  generateWaveTables();
  for(byte n=0;n<2;n++){
    syncActiveWaveCache(n, oscWaveform[n], 0);
    syncActiveWaveCache(n, oscWaveform[n], 1);
  }
  updateEnvelopeRates();
  filterCutoffHz = cutoffControlToHz(cutoffControl);

  xTaskCreatePinnedToCore(audioTask, "Audio", 8192, NULL, 3, NULL, 0);

  // 0. Leds
  leds.begin();
  leds.setBrightness(LED_BRIGHT);
  for(byte i=0;i<NUM_LEDS;i++){
    leds.setColor(i,OFF);
  }
  leds.setColor(currentPage, GREEN);
  SimpleColor color = oscSelect ? CYAN : YELLOW;
  leds.setColor(9, color);
  leds.show();

  // 1. Encoders
  Wire.begin(PIN_SDA, PIN_SCL); 
  Wire.setClock(400000);
  Serial.println("Test PCF8575");
  // Poner TODOS los pines como entrada (HIGH)
  Wire.beginTransmission(0x20);
  Wire.write(0xFF);
  Wire.write(0xFF);
  Wire.endTransmission();
  
  delay(10);

  Wire.requestFrom(ADDR_ENC, (uint8_t)2); 
  if (Wire.available() == 2) {
    uint8_t lowByte = Wire.read();
    uint8_t highByte = Wire.read();
    lastEncState = lowByte | (highByte << 8);
  }
  Wire.requestFrom(ADDR_BTN, (uint8_t)1);
  if (Wire.available()) {
    lastBtnState = Wire.read();
  }
  encoder.begin(false); //pinMode A/B INPUT

  // 2. TFT
  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  tft.setFreeFont(LAB_TEXT);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("SyntheBasic", 100, 120);
  delay(500);
  
  drawUI();

  Serial.println("TFT configurada");

  // 3. Configurar pines 
  pinMode(PIN_INT_ENC, INPUT_PULLUP);
  pinMode(PIN_INT_BTN, INPUT_PULLUP);
  pinMode(PIN_ENC_BOT, INPUT);
  pinMode(PIN_TACTIL, INPUT);
  pinMode(PIN_AM, INPUT_PULLUP);
  pinMode(PIN_AZ, INPUT_PULLUP);

  // 4. Interrupciones
  attachInterrupt(digitalPinToInterrupt(PIN_INT_ENC), isrEnc, FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_INT_BTN), isrBtn, FALLING);  
  Serial.println("Interrupciones OK");
  Serial.printf("Buffer de audio: %d muestras\n", BUFFER_AUDIO);

  sequencerSteps = new SequencerStep[stepsForSeq];
  seqDefault(stepsForSeq);


}

void loop() {

  handleMIDI();
  processSequencer();
  flushPendingChordNotes();

  if (updateEnc) processEncoders();
  if (updateBtn) processButtons();
  processControl();
  refreshAudioScope();
}
