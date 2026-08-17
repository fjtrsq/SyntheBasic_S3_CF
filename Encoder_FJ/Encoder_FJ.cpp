/**
 * @file Encoder_FJ.cpp
 * @brief Implementación de la clase Encoder_FJ
 */

#include "Encoder_FJ.h"


#define R_START 0x0

#if ENABLE_HALF_STEP
// Use the half-step state table (emits a code at 00 and 11)
#define R_CCW_BEGIN   0x1
#define R_CW_BEGIN    0x2
#define R_START_M     0x3
#define R_CW_BEGIN_M  0x4
#define R_CCW_BEGIN_M 0x5

const unsigned char ttable[][4] = 
{
  // 00                  01              10            11
  {R_START_M,           R_CW_BEGIN,     R_CCW_BEGIN,  R_START},           // R_START (00)
  {R_START_M | DIR_CCW, R_START,        R_CCW_BEGIN,  R_START},           // R_CCW_BEGIN
  {R_START_M | DIR_CW,  R_CW_BEGIN,     R_START,      R_START},           // R_CW_BEGIN
  {R_START_M,           R_CCW_BEGIN_M,  R_CW_BEGIN_M, R_START},           // R_START_M (11)
  {R_START_M,           R_START_M,      R_CW_BEGIN_M, R_START | DIR_CW},  // R_CW_BEGIN_M 
  {R_START_M,           R_CCW_BEGIN_M,  R_START_M,    R_START | DIR_CCW}  // R_CCW_BEGIN_M
};
#else
// Use the full-step state table (emits a code at 00 only)
#define R_CW_FINAL   0x1
#define R_CW_BEGIN   0x2
#define R_CW_NEXT    0x3
#define R_CCW_BEGIN  0x4
#define R_CCW_FINAL  0x5
#define R_CCW_NEXT   0x6

const unsigned char ttable[][4] = 
{
  // 00         01           10           11
  {R_START,    R_CW_BEGIN,  R_CCW_BEGIN, R_START},           // R_START
  {R_CW_NEXT,  R_START,     R_CW_FINAL,  R_START | DIR_CW},  // R_CW_FINAL
  {R_CW_NEXT,  R_CW_BEGIN,  R_START,     R_START},           // R_CW_BEGIN
  {R_CW_NEXT,  R_CW_BEGIN,  R_CW_FINAL,  R_START},           // R_CW_NEXT
  {R_CCW_NEXT, R_START,     R_CCW_BEGIN, R_START},           // R_CCW_BEGIN
  {R_CCW_NEXT, R_CCW_FINAL, R_START,     R_START | DIR_CCW}, // R_CCW_FINAL
  {R_CCW_NEXT, R_CCW_FINAL, R_CCW_BEGIN, R_START}            // R_CCW_NEXT
};
#endif

/**
 * @brief Constructor de Encoder_FJ
 */
Encoder_FJ::Encoder_FJ(uint8_t pinA, uint8_t pinB) :
  _pinA(pinA),
  _pinB(pinB),
  _state(R_START),
  _accelEnabled(false),
  _lastMoveTime(0),
  _sensibility(120),
  _maxMultiplier(6)
{
}



/**
 * @brief Inicializa los pines del encoder
 */
void Encoder_FJ::begin(bool pull_interno)
{
  pinMode(_pinA, (pull_interno ? INPUT_PULLUP : INPUT));
  pinMode(_pinB, (pull_interno ? INPUT_PULLUP : INPUT));
}


/**
 * @brief Lee el movimiento del encoder
 *
 * La función debe llamarse periódicamente (loop).
 * Devuelve el movimiento acumulado desde la última llamada.
 */
int8_t Encoder_FJ::read(void)
{
  uint8_t pinstate = (digitalRead(_pinB) << 1) | digitalRead(_pinA);
  _state = ttable[_state & 0xf][pinstate];

  int8_t movimiento = ((_state & 0x30) >> 3);
  if (movimiento != 0) movimiento -= 3;   // -1 o +1

  if (movimiento == 0) return 0;

  if (_accelEnabled)
  {
    uint32_t ahora = millis();
    uint32_t delta = ahora - _lastMoveTime;
    _lastMoveTime = ahora;
	
	if (delta < 3) return 0;


    // Constante de sensibilidad (ajustable)
    const uint16_t K = 120;  

    int8_t factor = _sensibility / delta;

    // mínimo x1
    if (factor < 1) factor = 1;

    // limitar máximo
    if (factor > _maxMultiplier) factor = _maxMultiplier;

    movimiento *= factor;
  }

  return movimiento;
}



void Encoder_FJ::setAccel(bool enable)
{
  _accelEnabled = enable;
}

void Encoder_FJ::setAccelSpeed(uint8_t sensibility, int8_t maxMultiplier)
{
  _sensibility = sensibility;
  _maxMultiplier = maxMultiplier;
}

