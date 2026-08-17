/**
 * @file Encoder_FJ.h
 * @brief Librería para encoder rotativo mecánico con detección de dirección
 *        y aceleración opcional.
 *
 * Basada en la librería MD_REncoder.
 *
 * @author FJTRSQ
 * @version 1.1.0
 */

#ifndef ENCODER_FJ_H
#define ENCODER_FJ_H

#include <Arduino.h>

/** @def ENABLE_HALF_STEP
 *  @brief Habilita modo half-step (1) o full-step (0)
 */
#define ENABLE_HALF_STEP  0

/** @def ENABLE_PULLUPS
 *  @brief Habilita resistencias pull-up internas por defecto
 */
#define ENABLE_PULLUPS    1

/** @def DIR_NONE
 *  @brief Sin movimiento detectado
 */
#define DIR_NONE  0x00

/** @def DIR_CW
 *  @brief Giro horario
 */
#define DIR_CW    0x10

/** @def DIR_CCW
 *  @brief Giro antihorario
 */
#define DIR_CCW   0x20

/**
 * @class Encoder_FJ
 * @brief Manejo de encoder rotativo mecánico con aceleración opcional.
 *
 * La función read() devuelve:
 * - -1 giro antihorario
 * -  0 sin movimiento
 * - +1 giro horario
 *
 * Si la aceleración está habilitada, el valor puede ser mayor en magnitud.
 */
class Encoder_FJ
{
  public:

    /**
     * @brief Constructor de la clase Encoder_FJ
     * @param pinA Pin conectado a la salida A del encoder
     * @param pinB Pin conectado a la salida B del encoder
     */
    Encoder_FJ(uint8_t pinA, uint8_t pinB);

    /**
     * @brief Inicializa los pines del encoder
     * @param pull_interno Habilita resistencias pull-up internas
     */
    void begin(bool pull_interno = ENABLE_PULLUPS);

    /**
     * @brief Lee el estado del encoder
     *
     * @return int8_t
     * - -1 / +1 giro normal
     * -  0 sin movimiento
     * - valores mayores si la aceleración está habilitada
     */
    int8_t read(void);

    /**
     * @brief Habilita o deshabilita la aceleración
     * @param enable true para activar aceleración
     */
    void setAccel(bool enable);

    /**
     * @brief Configura los parámetros de aceleración
     *
     * @param minIntervalMs Tiempo mínimo (ms) entre pasos para acelerar
     * @param maxMultiplier Multiplicador máximo de aceleración
     */
    void setAccelSpeed(uint8_t sensibility, int8_t maxMultiplier);

  private:
    uint8_t _pinA;     ///< Pin A del encoder
    uint8_t _pinB;     ///< Pin B del encoder
    uint8_t _state;    ///< Estado interno del autómata

    bool     _accelEnabled;     ///< Aceleración habilitada
    uint32_t _lastMoveTime;     ///< Tiempo del último movimiento
    uint8_t  _sensibility;    	///< sensibilidad de aceleración
    int8_t   _maxMultiplier;    ///< Multiplicador máximo
};

#endif
