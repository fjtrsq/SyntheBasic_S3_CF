void noteOn(uint8_t note, uint8_t velocity, bool isLive) {

  float baseFreq = 440.0f * pow(2.0f, (note - 69) / 12.0f);
  float velocityNorm = velocity / 127.0f;
  float velocityGain = powf(velocityNorm, velocityExponent);
  int voiceIndex = -1;

  limitReleaseVoices(maxReleaseVoices);
  for (int i = 0; i < numVoices; i++) {
    if (!voices[i].active) {
      voiceIndex = i;
      break;
    }
  }

  if (voiceIndex < 0) {
    voiceIndex = findQuietestReleaseVoice();
  }

  if (voiceIndex < 0) return;

  voices[voiceIndex].isLive = isLive;

  float targetFreq = baseFreq;
  if (voices[voiceIndex].isLive) {
    float semitones = pitchBendNorm * pitchBendRangeSemis;
    float pitchRatio = exp2f(semitones / 12.0f);
    targetFreq *= pitchRatio;
  }

  voices[voiceIndex].phase0 = 0;
  voices[voiceIndex].phase1 = 0;

  float glideStartFreq = (glideTime > 0.0001f && hasLastNote) ? lastNoteFreq : targetFreq;
  voices[voiceIndex].phaseInc0 = (uint32_t)((glideStartFreq * 4294967296.0f) / SAMPLE_RATE);
  voices[voiceIndex].phaseInc1 = (uint32_t)(((glideStartFreq * (1.0f + detuneAmount)) * 4294967296.0f) / SAMPLE_RATE);
  voices[voiceIndex].targetPhaseInc0 = (uint32_t)((targetFreq * 4294967296.0f) / SAMPLE_RATE);
  voices[voiceIndex].targetPhaseInc1 = (uint32_t)(((targetFreq * (1.0f + detuneAmount)) * 4294967296.0f) / SAMPLE_RATE);

  for (uint8_t osc = 0; osc < N_OSC; osc++) {
    voices[voiceIndex].envLevel[osc] = 0.0f;
    voices[voiceIndex].envDelaySamples[osc] = (uint32_t)(delayTime[osc] * SAMPLE_RATE);
    voices[voiceIndex].envState[osc] = (voices[voiceIndex].envDelaySamples[osc] > 0) ? ENV_DELAY : ENV_ATTACK;
  }
  voices[voiceIndex].velocityGain = velocityGain;
  voices[voiceIndex].midiNote = note;
  voices[voiceIndex].active = true;

  // Guardamos baseFreq (o targetFreq según prefieras para el glide de la siguiente nota)
  lastNoteFreq = targetFreq;
  hasLastNote = true;

  voices[voiceIndex].morphPhase = 0.0f;
  voices[voiceIndex].morphActive = true;
  voices[voiceIndex].morphDirection = 1;
}

void noteOff(uint8_t note) {

  for (int i = 0; i < numVoices; i++) {
    if (voices[i].active && voices[i].midiNote == note) {
      for (uint8_t osc = 0; osc < N_OSC; osc++) voices[i].envState[osc] = ENV_RELEASE;
    }
  }
}

void releaseAllVoices() {
  for (int i = 0; i < numVoices; i++) {
    if (voices[i].active) {
      for (uint8_t osc = 0; osc < N_OSC; osc++) voices[i].envState[osc] = ENV_RELEASE;
    }
  }
}

void handleControlChange(uint8_t cc, uint8_t value) {
  // --- MODO MIDI LEARN ACTIVO ---
  if (midiLearnActive) {
    // Buscamos si este CC ya estaba asignado a otro parámetro y lo reasignamos
    // O simplemente guardamos la asignación en la lista de mapeos dinámicos:
    
    // Asignamos directamente la pareja (Página, Parámetro) a este número de CC
    midiCcMappings[cc].page = midiLearnTargetPage;
    midiCcMappings[cc].param = midiLearnTargetParam;
    midiCcMappings[cc].assigned = true;

    Serial.printf("[MIDI LEARN] ¡Asignado! CC %d -> Pag %d, Param %d (%s)\n", 
                  cc, midiLearnTargetPage, midiLearnTargetParam, 
                  pageParam[midiLearnTargetPage][midiLearnTargetParam].name);

    // Feedback en pantalla
    //snprintf(presetStatus, sizeof(presetStatus), "CC %d -> %s", cc, pageParam[midiLearnTargetPage][midiLearnTargetParam].name);
    

    // Desactivar modo Learn
    midiLearnActive = false;
    leds.setColor(9, colorAnt);
    leds.show();
    return;
  }

  // --- FUNCIONAMIENTO NORMAL ---
  // Si el CC recibido tiene una asignación activa, aplicamos el cambio
  if (midiCcMappings[cc].assigned && midiCcMappings[cc].page >= 0) {
    applyMidiParamUpdate(midiCcMappings[cc].page, midiCcMappings[cc].param, value);
  }
}

void handleAfterChannel(uint8_t value){
  if (midiLearnActive) {
    afterChannelTarget.page = midiLearnTargetPage;
    afterChannelTarget.param = midiLearnTargetParam;
    afterChannelTarget.assigned = true;

    Serial.printf("[MIDI LEARN] ¡Asignado! AfterChannel -> Pag %d, Param %d (%s)\n", 
                  midiLearnTargetPage, midiLearnTargetParam, 
                  pageParam[midiLearnTargetPage][midiLearnTargetParam].name);
    midiLearnActive = false;
    leds.setColor(9, colorAnt);
    leds.show();
    return;
  }

  if (afterChannelTarget.assigned && afterChannelTarget.page >= 0) {  
    applyMidiParamUpdate(afterChannelTarget.page, afterChannelTarget.param, value);
  }

}

void handlePitchBend(uint8_t data1, uint8_t data2) {
  if (midiLearnActive) {
    
    // Asignamos directamente la pareja (Página, Parámetro) a pitchBend*/
    pitchBendTarget.page = midiLearnTargetPage;
    pitchBendTarget.param = midiLearnTargetParam;
    pitchBendTarget.assigned = true;

    Serial.printf("[MIDI LEARN] ¡Asignado! PitchBend -> Pag %d, Param %d (%s)\n", 
                  midiLearnTargetPage, midiLearnTargetParam, 
                  pageParam[midiLearnTargetPage][midiLearnTargetParam].name);

    // Feedback en pantalla
    //snprintf(presetStatus, sizeof(presetStatus), "CC %d -> %s", cc, pageParam[midiLearnTargetPage][midiLearnTargetParam].name);
 
    // Desactivar modo Learn
    midiLearnActive = false;
    leds.setColor(9, colorAnt);
    leds.show();
    return;
  }
  // Combinar LSB (data1) y MSB (data2) para obtener los 14 bits (0 a 16383)
  uint16_t raw14bit = ((uint16_t)data2 << 7) | (data1 & 0x7F);

  // Normalizar de -1.0f a +1.0f (centro en 8192)
  pitchBendNorm = ((float)raw14bit - 8192.0f) / 8192.0f;

  // MODO 1: Mapeado personalizado a otro parámetro del sintetizador
  if (pitchBendTarget.assigned && pitchBendTarget.page >= 0) {
    // Mapeamos el rango 0..16383 (o pitchBendNorm) de 0 a 127 para reutilizar applyMidiParamUpdate
    uint8_t simulatedPBend = (uint8_t)map(raw14bit, 0, 16383, 0, 127);
    applyMidiParamUpdate(pitchBendTarget.page, pitchBendTarget.param, simulatedPBend);
    return;
  }

  // MODO 2: Funcionamiento estándar (Control de Pitch)
  // Calculamos el factor de escala de frecuencia basado en los semitonos configurados
  float semitones = pitchBendNorm * pitchBendRangeSemis;
  float pitchRatio = exp2f(semitones / 12.0f);

  // Actualizamos la frecuencia incremental de todas las voces activas
  portENTER_CRITICAL(&audioMux);
  for (int i = 0; i < numVoices; i++) {
    if (voices[i].active && voices[i].isLive) {
      float baseFreq = 440.0f * powf(2.0f, (voices[i].midiNote - 69) / 12.0f);
      float bentFreq = baseFreq * pitchRatio;

      voices[i].targetPhaseInc0 = (uint32_t)((bentFreq * 4294967296.0f) / SAMPLE_RATE);
      voices[i].targetPhaseInc1 = (uint32_t)(((bentFreq * (1.0f + detuneAmount)) * 4294967296.0f) / SAMPLE_RATE);
    }
  }
  portEXIT_CRITICAL(&audioMux);
}

void handleSecondNoteOff(uint8_t data1){
  return;
}
void handleSecondNoteOn(uint8_t data1, uint8_t data2){
  return;
}

void processMidiMessage(uint8_t status, uint8_t data1, uint8_t data2) {
  uint8_t channelSynth = 0; 
  uint8_t command = status & 0xF0;
  uint8_t channel = status & 0x0F; // Convertir rango 0-15 a 1-16
  bool isPrimary = channel == channelSynth;
  switch(command){
    case 0x90:   // Note On
      if (data2 > 0) {
        bool wasPressed = midiKeysDown[data1];
        if (!wasPressed) {
          midiKeysDown[data1] = true;
          if (midiKeysPressedCount < 127) midiKeysPressedCount++;
        }
        
        // 1. Guardamos la nota en el paso (tu código original)
        captureSequencerStepFromMidi(data2);

        // --- 2. NUEVO: LÓGICA DE PREESCUCHA ---
        if (sequencerState == SEQ_STATE_REC) {
          SequencerStep &step = sequencerSteps[sequencerEditStep & 0xF];

          if (step.mode == SEQ_MODE_CHORD) {
            playSequencerChordNotes(sequencerEditStep & 0xF, data2);
          } 
          else if (step.mode == SEQ_MODE_ARP) {
            // Si es la primera tecla que pulsamos, forzamos que el arpegio comience inmediatamente
            if (midiKeysPressedCount == 1) {
              sequencerArpIndex = 0;
              sequencerArpNextMs = millis();
            }
          } 
          else {
            int transposedNote = constrain((int)data1 + step.transpose, 0, 127);
            noteOn((uint8_t)transposedNote, data2, false);
          }
        }
        // --- FIN PREESCUCHA (Comienza tu lógica global original) ---
        else {
          if (arpEnabled) {
            ArpHold holdMode = effectiveArpHold();
            if (holdMode != HOLD_OFF && holdMode != HOLD_STACK && midiKeysPressedCount == 1) {
              arpClearNoteSoft();
            }
            arpAddNote(data1, data2);
            if (arpSamplesToNextStep == 0) arpSamplesToNextStep = 1;
          }
          else {
            if (latchEnabled && midiKeysPressedCount == 1) {
              releaseAllVoices();
              resetLfoAttackRequested = true;
            }
            if (chordAssistantEnabled) playChordFromRoot(data1, data2);
            else {
              if(isPrimary) noteOn(data1, data2, true);
              else handleSecondNoteOn(data1, data2);
            }
          }
        }
      }
      else {
        processMidiMessage(0x80 | (status & 0x0F), data1, 0);
      }
    break;
    
    case 0x80:  // Note Off
      if (midiKeysDown[data1]) {
        midiKeysDown[data1] = false;
        if (midiKeysPressedCount > 0) midiKeysPressedCount--;
      }

      // --- NUEVO: APAGAR NOTAS DE PREESCUCHA ---
      if (sequencerState == SEQ_STATE_REC) {
        SequencerStep &step = sequencerSteps[sequencerEditStep & 0xF];
        
        if (step.mode == SEQ_MODE_CHORD) {
          stopChordFromRoot(step.root);
          stopChordFromRoot(data1);
        } 
        else if (step.mode == SEQ_MODE_ARP) {
          // Al soltar todas las teclas, detenemos la nota activa del arpegio
          if (midiKeysPressedCount == 0 && sequencerArpNoteOn) {
            noteOff(sequencerActiveArpNote);
            sequencerArpNoteOn = false;
          }
        } 
        else {
          int transposedNote = constrain((int)data1 + step.transpose, 0, 127);
          noteOff((uint8_t)transposedNote);
        }
      }
      // --- FIN APAGAR NOTAS PREESCUCHA ---
      else {
        if (arpEnabled) {
          if (effectiveArpHold() == HOLD_OFF) arpRemoveNote(data1);
        }
        else if (!latchEnabled) {
          if (chordAssistantEnabled) stopChordFromRoot(data1);
          else {
            if(isPrimary) noteOff(data1);
            else handleSecondNoteOff(data1);
          }
        }
      }
    break;
    case 0xD0: // Channel Aftertouch (Channel Pressure)
      handleAfterChannel(data1);
    break;
    case 0xB0: // Control Change (CC)
      handleControlChange(data1, data2);
    break;
    case 0xE0:  //pitchbend
      handlePitchBend(data1, data2);
    break;
  }
}

void handleMidiUsb() {
  #if SYNTH_USB_MIDI_ENABLED
    midiEventPacket_t packet = {0, 0, 0, 0};
    while (usbMidi.readPacket(&packet)) {
      midi_code_index_number_t cin = MIDI_EP_HEADER_CIN_GET(packet.header);
      if (cin == MIDI_CIN_NOTE_ON || 
          cin == MIDI_CIN_NOTE_OFF ||
          cin == MIDI_CIN_CONTROL_CHANGE || 
          cin == MIDI_CIN_PITCH_BEND_CHANGE ||
          cin == MIDI_CIN_CHANNEL_PRESSURE ||
          cin == MIDI_CIN_PROGRAM_CHANGE) {
        processMidiMessage(packet.byte1, packet.byte2, packet.byte3);
      }
    }
  #endif
}

void handleMIDI() {
  while (Serial2.available()) {
    uint8_t midibyte = Serial2.read();

    // Descartar mensajes Realtime de sistema (0xF8 a 0xFF) para no romper la máquina de estados
    if (midibyte >= 0xF8) continue;

    if (midibyte & 0x80) {
      // Es status byte (0x80 a 0xF7)
      midiStatus = midibyte;
      waitingForData2 = false;
    }
    else {
      // Es data byte (0x00 a 0x7F)
      if (midiStatus == 0) continue; // Ignorar si no hay status definido

      uint8_t cmd = midiStatus & 0xF0;

      if (!waitingForData2) {
        midiData1 = midibyte;

        // Comprobamos si es un mensaje de 1 solo byte de datos
        if (cmd == 0xD0 || cmd == 0xC0) {
          processMidiMessage(midiStatus, midiData1, 0);
          waitingForData2 = false; // Mantiene el estado listo para Running Status
        } else {
          waitingForData2 = true;  // Requiere 2 bytes (0x80, 0x90, 0xA0, 0xB0, 0xE0)
        }
      }
      else {
        uint8_t midiData2 = midibyte;
        waitingForData2 = false;

        processMidiMessage(midiStatus, midiData1, midiData2);
      }
    }
  }

  handleMidiUsb();
}

void resetPitchBend() {
  // Resetear el PitchBend a su comportamiento original (Control de Pitch)
  pitchBendTarget.assigned = false;
  pitchBendTarget.page = -1;
  pitchBendTarget.param = -1;
  Serial.println("[PITCH BEND] Restaurado a control de afinación estándar");
}

void resetAfterChannel() {
  afterChannelTarget.assigned = false;
  afterChannelTarget.page = -1;
  afterChannelTarget.param = -1;
  Serial.println("[AFTER CHANNEL] Reset");

}

void resetMidiMappings() {
  for (uint8_t i = 0; i < 128; i++) {
    midiCcMappings[i].page = -1;
    midiCcMappings[i].param = -1;
    midiCcMappings[i].assigned = false;
  }
  Serial.println("[CC]  All Reset");
}

void triggerMidiLearn(uint8_t page, uint8_t param) {
  midiLearnActive = true;
  midiLearnTargetPage = page;
  midiLearnTargetParam = param;
  
  // Feedback en consola / UI
  Serial.printf("[MIDI LEARN] Esperando CC para: Pagina %d, Parametro %d (%s)...\n", 
                page, param, pageParam[page][param].name);

  // Puedes mostrar un mensaje en la pantalla usando tu sistema de estado/status
  //snprintf(presetStatus, sizeof(presetStatus), "LEARN: %s", pageParam[page][param].name);
  
}

void saveMidiMappingsToFS() {
  MidiMapData data;
  memcpy(&data.ccMap, &midiCcMappings, sizeof(midiCcMappings));
  memcpy(&data.pbTarget, &pitchBendTarget, sizeof(pitchBendTarget));
  memcpy(&data.acTarget, &afterChannelTarget, sizeof(afterChannelTarget));

  fs::File file = LittleFS.open("/midi_map.bin", "w");
  if (!file) {
    Serial.println("[MIDI] Error al abrir LittleFS para guardar");
    return;
  }

  size_t written = file.write((uint8_t*)&data, sizeof(data));
  file.close();

  if (written == sizeof(data)) {
    Serial.println("[MIDI] Mapeo guardado en LittleFS");
  } else {
    Serial.println("[MIDI] Error de escritura en LittleFS");
  }
}

void loadMidiMappingsFromFS() {
  if (!LittleFS.exists("/midi_map.bin")) {
    Serial.println("[MIDI] Archivo no existe");
    return;
  }

  fs::File file = LittleFS.open("/midi_map.bin", "r");
  if (!file) {
    Serial.println("[MIDI] Archivo corrupto");
    return;
  }

  MidiMapData data;
  size_t readBytes = file.read((uint8_t*)&data, sizeof(data));
  file.close();

  if (readBytes != sizeof(data)) {
    Serial.println("[MIDI] Archivo corrupto");    
    return;
  }

  memcpy(&midiCcMappings, &data.ccMap, sizeof(midiCcMappings));
  memcpy(&pitchBendTarget, &data.pbTarget, sizeof(pitchBendTarget));
  memcpy(&afterChannelTarget, &data.acTarget, sizeof(afterChannelTarget)); 

  Serial.println("[MIDI] Mapeo cargado desde LittleFS");
}

void applyMidiParamUpdate(uint8_t page, uint8_t paramIndex, uint8_t value) {
  // Asegurarnos de que el parámetro está dentro del rango válido
  if (paramIndex >= PARAMS_PER_PAGE) return;

  // 1. MANEJO DE LA PÁGINA ADSR (PAGE_ADSR = 8)
  if (page == PAGE_ADSR) {
    if (paramIndex < TOTAL_ADSR) {
      int minVal = ADSRpage[paramIndex].min;
      int maxVal = ADSRpage[paramIndex].max;
      ADSRvalues[oscSelect][paramIndex] = map(value, 0, 127, minVal, maxVal);
    } else if (paramIndex == 6) { // OSC MIX (0..100)
      ADSRmixValues[0] = map(value, 0, 127, 0, 100);
    } else if (paramIndex == 7) { // DETUNE (0..100)
      ADSRmixValues[1] = map(value, 0, 127, 0, 100);
    }
    
    // Recalcular tasas de la envolvente para el oscilador seleccionado
    updateEnvelopeRates(oscSelect);

    // Redibujar la UI si el usuario está actualmente viendo esta página
    if (currentPage == PAGE_ADSR) {
      drawMainVisualization();
      drawValue(paramIndex);
    }
    return;
  }

  // 2. MANEJO DE PÁGINAS PRINCIPALES (PAGE_CONF a PAGE_FILE / PAGE 0 a 7)
  if (page < MAIN_PARAM_PAGES) {
    int minVal = pageParam[page][paramIndex].min;
    int maxVal = pageParam[page][paramIndex].max;

    // Escalado del valor MIDI (0-127) al rango real del parámetro
    int mappedValue = map(value, 0, 127, minVal, maxVal);
    
    // Calculamos el delta (dirección/incremento) necesario para refreshValue
    int delta = mappedValue - pageParam[page][paramIndex].value;

    if (delta != 0) {
      // Ejecutamos la lógica que actualiza tanto la variable global como el motor DSP
      refreshValue((Page)page, paramIndex, delta);

      // Si la página modificada es la actual en pantalla, redibujamos el valor
      if (currentPage == page) {
        drawValue(paramIndex);
      }
    }
  }
}
