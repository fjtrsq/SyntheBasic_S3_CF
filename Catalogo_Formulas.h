inline float saw(float phase){ return  (2.0f * phase) - 1.0f; }
inline float sine(float phase){ return sinf(2.0f * PI * phase);}
inline float triangle(float phase){ return 4.0f * fabsf(phase - 0.5f) - 1.0f;}
inline float square(float phase){ return (phase < 0.5f) ? 1.0f : -1.0f;}

float pulse(float phase){ return (phase < varPulse) ? 1.0f : -1.0f;}
float super(float phase){ return  (0.60f * saw(phase))
                + (0.25f * sinf((2.0f * PI * phase) + 0.12f))
                + (0.15f * sinf((2.0f * PI * phase * 2.0f) - 0.07f));}
float organ(float phase){ return (sine(phase) + 0.50f * sinf(2.0f * PI * phase * 2.0f) + 0.30f * sinf(2.0f * PI * phase * 3.0f));}
float crush(float phase){ return roundf(sine(phase) * 6.0f);}

// 1. Seno Saturado (Cálido, estilo distorsión de válvulas)
    float warm_drive(float phase){ return tanhf(2.5f * sine(phase)) / tanhf(2.5f);}
// 2. Seno Exponencial (Armónicos impares muy agresivos)
    float sine_exp(float phase){ return (sine(phase) >= 0.0f ? 1.0f : -1.0f) * (1.0f - expf(-4.0f * fabsf(sine(phase))));}
// 3. Sierra Exponencial (Ataque agresivo y decaimiento curvo, ideal para "plucks")
    float saw_exp(float phase){ return (-1.0f + 2.0f * (1.0f - expf(-3.0f * phase)) / (1.0f - expf(-3.0f)));}
// 4. Rectificación de Onda Completa (Sube el tono una octava de forma distorsionada)
    float full_rect(float phase){ return (fabsf(sine(phase)) * 2.0f) - 1.0f;}
// 5. Polinomio de Chebyshev (Genera un armónico puro de 4ª orden sin usar senos extras)
    float chebyshev4(float phase){ return 8.0f * powf(sine(phase), 4.0f) - 8.0f * (sine(phase) * sine(phase)) + 1.0f;
// 6. FM Clásica 1:1 (Sonido de campana/órgano eléctrico básico)
    float fm_1_1(float phase){ return sinf(2.0f * PI * phase + 1.0f * sinf(2.0f * PI * phase));}
// 7. FM Metálica 1:3 (Tono de campana de metal o campana tubular)
    float fm_1_3(float phase){ return sinf(2.0f * PI * phase + 1.5f * sinf(2.0f * PI * phase * 3.0f));}
// 8. FM Agresiva 1:4 (Sonido industrial, ruidoso e inarmónico)
    float fm_1_4(float phase){ return sinf(2.0f * PI * phase + 2.0f * sinf(2.0f * PI * phase * 4.0f));}
// 9. Automodulación FM (Genera una sierra matemática muy brillante y limpia)
    float fm_feedback(float phase){ return sinf(2.0f * PI * phase + 0.8f * sine(phase));}
// 10. Pseudo-Vocal "AA" (Filtro de formante fijo imitando la voz humana)
    float vocal_aa(float phase){ return (sine(phase) + 0.8f * sinf(2.0f * PI * phase * 3.0f) + 0.4f * sinf(2.0f * PI * phase * 5.0f))f;}
// 11. Pseudo-Vocal "OO" (Formante más cerrado y oscuro)
    float vocal_oo(float phase){ return (sine(phase) + 0.6f * sinf(2.0f * PI * phase * 2.0f) + 0.1f * sinf(2.0f * PI * phase * 3.0f));}
// 12. Onda Hueca / "Hollow" (Solo armónicos impares, textura similar al clarinete)
    float hollow(float phase){ return (sine(phase) + 0.33f * sinf(2.0f * PI * phase * 3.0f) + 0.2f * sinf(2.0f * PI * phase * 5.0f));}
// 13. Súper Armónicos Pares (Suena extremadamente brillante, una octava por encima)
    float even_harmonics(float phase){ return (sinf(2.0f * PI * phase * 2.0f) + 0.5f * sinf(2.0f * PI * phase * 4.0f) + 0.25f * sinf(2.0f * PI * phase * 6.0f));}
// 14. Phase Pinch (Deforma el tiempo; suave al inicio, hiper-frecuente al final)
    float phase_pinch(float phase){ return sinf(2.0f * PI * powf(phase, 1.7f));}
// 15. Sinc Pulse / Función Espectral (Produce pulsos agudos aislados de alta fidelidad)
    float sinc_pulse(float phase){ 
      float sync_offset=(phase - 0.5f) * 15.0f; // Multiplicador controla el número de anillos
      return (fabsf(sync_offset) < 0.0001f) ? 1.0f : sinf(sync_offset) / sync_offset;
    }
// 16. Onda Chirp (La frecuencia se duplica progresivamente a lo largo del ciclo)
    float chirp(float phase){ return sinf(2.0f * PI * phase * (1.0f + phase));}
// 17. Seno partido en ventanas (Grit / Modulación por Anillo Interna)
    float grit(float phase){ return sine(phase) * (sinf(2.0f * PI * phase * 5.0f) > 0.0f ? 1.0f : -1.0f);}    
// 18. Triángulo Invertido Absoluto (Corta la onda por la mitad generando picos duros)
    float tri_invert(float phase){ return 1.0f - 2.0f * fabsf(triangle(phase));}
// 19. Fractal por Módulo (Multi-rampa matemática agresiva tipo glitch)
    float fractal_mod(float phase){ return -1.0f + 2.0f * fmodf(phase * 4.0f, 1.0f);}
// 20. Micro-Ruido Fractal (Onda Weierstrass simplificada, textura arenosa/industrial)
    float noise_fractal(float phase){ return (sine(phase) + 0.5f * cosf(2.0f * PI * phase * 3.0f) + 0.25f * sinf(2.0f * PI * phase * 9.0f));}
// 21. Cuerda Pulsada Exponencial (Ataque ultra rápido y decaimiento no lineal)
    float string_pluck(float phase){ return saw(phase) * expf(-2.0f * phase);}
// 22. Clarinete / Viento Madera (Predominio extremo de armónicos impares con rampa suave)
    float clarinet(float phase){ return (sine(phase) + 0.5f * sinf(2.0f * PI * phase * 3.0f) + 0.25f * sinf(2.0f * PI * phase * 5.0f)) * (1.0f - phase);}
// 23. Metal Resonante / Campana Tibetana (Inarmónicos densos que simulan resonancia metálica)
    float bell_metal(float phase){ return (sine(phase) + 0.7f * sinf(2.0f * PI * phase * 2.71f) + 0.4f * sinf(2.0f * PI * phase * 5.43f));}
// 24. Pseudo-Cello (Combinación aditiva asimétrica que imita el frotado de una cuerda)
    float cello(float phase){ return (saw(phase) + 0.5f * sinf(2.0f * PI * phase) + 0.25f * cosf(2.0f * PI * phase * 2.0f));}
// 25. Sierra Parabólica (Curvatura cuadrática, un sonido intermedio entre triángulo y sierra)
    float saw_parabolic(float phase){ return (saw(phase) * saw(phase)) * (phase < 0.5f ? 1.0f : -1.0f);}
// 26. Onda Trapezoidal (Cuadrada con rampas de subida y bajada suaves, menos aliasing)
    float trapezoid(float phase){  
      float trapez= saw(phase) * 2.0f;
      if (trapez > 1.0f) return  1.0f;
      else if (trapez < -1.0f) return  -1.0f;
    }
// 27. Triángulo con "Soft Clipping" (Aplana los extremos del triángulo imitando un circuito saturado)
    float soft_tri(float phase){ return sinf(triangle(phase) * (PI / 2.0f));}
// 28. Sierra Asimétrica Plegada (Sube linealmente, baja reflejando la fase)
    float folded_saw(float phase){
      float folded= saw(phase);
      if (folded > 0.5f) folded= 1.0f - folded;
      return folded;
    }
// 29. FM 1:5 Escarchada (Genera texturas cristalinas / gélidas muy brillantes)
    float fm_1_5(float phase){ return sinf(2.0f * PI * phase + 1.8f * sinf(2.0f * PI * phase * 5.0f));}
// 30. Modulación Cruzada (AM + FM simultánea dentro del mismo ciclo)
    float cross_mod(float phase){ return sinf(2.0f * PI * phase + 1.2f * sinf(2.0f * PI * phase * 2.0f)) * (0.5f + 0.5f * sine(phase));}  
// 31. Modulación por Coseno Exponencial (Efecto de formante sintético agresivo)
    float cos_exp(float phase){ return sinf(2.0f * PI * phase) * cosf(10.0f * phase * phase);}    
// 32. Phase Warp Dual (Dos velocidades de fase distintas colisionando en el medio)
    float warp(float phase){
      float warp_phase= (phase < 0.5f) ? powf(phase * 2.0f, 2.0f) * 0.5f : 0.5f + (1.0f - powf((1.0f - phase) * 2.0f, 2.0f)) * 0.5f;
      float phase_warp= sinf(2.0f * PI * warp_phase);
      return phase_warp;
    }
// 33. Seno PWM Fijo (Simula un ancho de pulso pero usando ciclos senoidales modificados)
    float sine_pwm(float phase){ return (phase < 0.3f) ? sinf(2.0f * PI * phase * (0.5f / 0.3f)) : sinf(2.0f * PI * (phase - 0.3f) * (0.5f / 0.7f) + PI);}   
// 34. Sierra + Cuadrada (El sonido clásico de los bajos Roland TB-03 / TB-303 híbrido)
    float tb_hybrid(float phase){ return (saw(phase) + triangle(phase)) * 0.5f;}
// 35. Onda Sub-Bajo Harmónica (Un seno limpio acompañado de una octava inferior sutil)
    float sub_harmonic(float phase){ return (sine(phase) * 0.75f) + (sinf(2.0f * PI * phase * 0.5f) * 0.25f);}
// 36. Interferencia de Fase (Dos senos muy cercanos en frecuencia que crean un batido fijo)
    float phase_beat(float phase){ return (sine(phase) + sinf(2.0f * PI * phase * 1.05f)) * 0.5f;}
// 37. Escalera de 8 Bits / Resonancia Cuántica (Divide el ciclo en 8 escalones discretos planos)
    float step_8bit(float phase){ return floorf(sine(phase) * 4.0f) / 4.0f;}
// 38. Sync Incompleto / Hard Sync (Una sierra que se reinicia a mitad de camino tres veces)
    float hard_sync(float phase){ return (2.0f * fmodf(phase * 3.0f, 1.0f)) - 1.0f;}
// 39. Onda Polinomial (Curva suave de tercer orden basada exclusivamente en álgebra)
    float poly_wave(float phase){ return 4.0f * phase * (1.0f - phase) * (phase - 0.5f) * 5.2f; }
// 40. Ruido Pseudo-Aleatorio Sincronizado (Textura metálica/chile de interferencia digital fija)
    float pseudo_noise(float phase){ return sinf(phase * 50.0f) * cosf(phase * 12.0f);}
// 41. Ruido aleatorio
    float noise(float phase){ return ((float)rand() / ((float)RAND_MAX / 2.0f)) - 1.0f;}

typedef float (*FormulaOnda)(float phase);
const FormulaOnda catalogoOndas[NUM_ONDAS] = {
    saw,sine,triangle,square,pulse,super,organ,crush,warm_drive,sine_exp,
    saw_exp,full_rect,chebyshev4,fm_1_1,fm_1_3,fm_1_4,fm_1_5,fm_feedback,vocal_aa,
    vocal_oo,hollow,even_harmonics,phase_pinch,sinc_pulse,chirp,grit,tri_invert,
    fractal_mod,noise_fractal,string_pluck,clarinet,bell_metall,cello,saw_parabolic,
    trapezoid,soft_tri,folded_saw,cross_mod,cos_exp,warp,sine_pwm,tb_hybrid,
    sub_harmonic,phase_beat,step_8bit,hard_sync,poly_wave,pseudo_noise,noise
};


void generateWaveTables(){
    float tempBuffer[WAVE_COUNT]; 
    for (int wave_idx = 0; wave_idx < WAVE_COUNT; wave_idx++) {
        // A. Llenar la tabla con la fórmula pura
        for (int i = 0; i < tableSize; i++) {
            float phase = (float)i / tableSize;
            tempBuffer[i] = catalogoOndas[wave_idx](phase);
        }
    }

    // B. Normalizar (La función que encontraste)
    float max_abs_val = 0.0f; 
    for (int i = 0; i < tableSize; ++i) {
        if (fabsf(tempBuffer[i]) > max_abs_val) { 
            max_abs_val = fabsf(tempBuffer[i]);
        } 
    } 
    if (max_abs_val > 0.00001f) {
        for (int i = 0; i < tableSize; ++i) {
            float floatNormalizado = tempBuffer[i] / max_abs_val;  
            waveformCatalog[wave_idx][i] = (int16_t)(floatNormalizado * 32767.0f);
        }
    }

}

void precalcularIconosDesdeAudioInt16() {
  for (int wave_idx = 0; wave_idx < WAVE_COUNT; wave_idx++) {
    for (int x = 0; x < ICON_W; x++) {
      
      // Mapeamos el píxel X (0 a 59) al índice de tu tabla de audio (0 a 511)
      int targetIndex = (x * tableSize) / ICON_W;
      
      // Leemos el valor entero original de tu catálogo (-32767 a 32767)
      int16_t valorInt16 = waveformCatalog[wave_idx][targetIndex];
      
      // Convertimos el rango entero (-32767 a 32767) al rango de píxeles del icono (0 a 24)
      // Dividir entre 32767.0f nos devuelve el equivalente flotante (-1.0 a 1.0) de forma idéntica
      float valorOndaFlotante = (float)valorInt16 / 32767.0f;
      int yPixel = (ICON_H / 2) + (int)(valorOndaFlotante * -11.0f);

      cacheGraficaOndas[wave_idx][x] = (uint8_t)constrain(yPixel, 0, ICON_H - 1);
    }
  }

void drawWaveAudioIcon(int idOnda, int posX, int posY, uint16_t colorLinea, uint16_t colorFondo) {
  // A. Limpiamos el lienzo del único Sprite antes de pintar la nueva onda
  menuSprite.fillSprite(colorFondo);

  int ultimoX = 0;
  int ultimoY = cacheGraficaOndas[idOnda][0];

  // B. Dibujamos la geometría dentro del Sprite leyendo la caché ultrarrápida
  for (int x = 1; x < ICON_W; x++) {
    int yPixel = cacheGraficaOndas[idOnda][x];
    menuSprite.drawLine(ultimoX, ultimoY, x, yPixel, colorLinea);
    ultimoX = x;
    ultimoY = yPixel;
  }

  // C. Estampamos el Sprite en la pantalla ST7789. 
  // El Sprite se libera inmediatamente para poder ser reutilizado.
  menuSprite.pushSprite(posX, posY);
}


