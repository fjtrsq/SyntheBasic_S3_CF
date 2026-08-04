void noteOn(uint8_t note, uint8_t velocity) {

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

  voices[voiceIndex].phase0 = 0;
  voices[voiceIndex].phase1 = 0;

  float glideStartFreq = (glideTime > 0.0001f && hasLastNote) ? lastNoteFreq : baseFreq;
  voices[voiceIndex].phaseInc0 = (uint32_t)((glideStartFreq * 4294967296.0f) / SAMPLE_RATE);
  voices[voiceIndex].phaseInc1 = (uint32_t)(((glideStartFreq * (1.0f + detuneAmount)) * 4294967296.0f) / SAMPLE_RATE);
  voices[voiceIndex].targetPhaseInc0 = (uint32_t)((baseFreq * 4294967296.0f) / SAMPLE_RATE);
  voices[voiceIndex].targetPhaseInc1 = (uint32_t)(((baseFreq * (1.0f + detuneAmount)) * 4294967296.0f) / SAMPLE_RATE);

  for (uint8_t osc = 0; osc < N_OSC; osc++) {
    voices[voiceIndex].envLevel[osc] = 0.0f;
    voices[voiceIndex].envDelaySamples[osc] = (uint32_t)(delayTime[osc] * SAMPLE_RATE);
    voices[voiceIndex].envState[osc] = (voices[voiceIndex].envDelaySamples[osc] > 0) ? ENV_DELAY : ENV_ATTACK;
  }
  voices[voiceIndex].velocityGain = velocityGain;
  voices[voiceIndex].midiNote = note;
  voices[voiceIndex].active = true;
  lastNoteFreq = baseFreq;
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
  if (midiCcMappings[cc].assigned) {
    applyMidiParamUpdate(midiCcMappings[cc].page, midiCcMappings[cc].param, value);
  }
}

void processMidiMessage(uint8_t status, uint8_t data1, uint8_t data2) {
  uint8_t command = status & 0xF0;

  if (command == 0x90) {   // Note On
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
          noteOn((uint8_t)transposedNote, data2);
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
          else noteOn(data1, data2);
        }
      }
    }
    else {
      processMidiMessage(0x80 | (status & 0x0F), data1, 0);
    }
  }
  else if (command == 0x80) {  // Note Off
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
        else noteOff(data1);
      }
    }
  }
  else if (command == 0xB0) { // Control Change (CC)
    handleControlChange(data1, data2);
  }
}

void handleMidiUsb() {
  #if SYNTH_USB_MIDI_ENABLED
    midiEventPacket_t packet = {0, 0, 0, 0};
    while (usbMidi.readPacket(&packet)) {
      midi_code_index_number_t cin = MIDI_EP_HEADER_CIN_GET(packet.header);
      if (cin == MIDI_CIN_NOTE_ON || cin == MIDI_CIN_NOTE_OFF || cin == MIDI_CIN_CONTROL_CHANGE) {
        processMidiMessage(packet.byte1, packet.byte2, packet.byte3);
      }
    }
  #endif
}

void handleMIDI() {
  while (Serial2.available()) {
    uint8_t midibyte = Serial2.read();

    if (midibyte & 0x80) {
      // Es status byte
      midiStatus = midibyte;
      waitingForData2 = false;
    }
    else {
      if (!waitingForData2) {
        midiData1 = midibyte;
        waitingForData2 = true;
      }
      else {
        uint8_t midiData2 = midibyte;
        waitingForData2 = false;

        // En lugar de duplicar toda la lógica aquí, llamamos a processMidiMessage
        processMidiMessage(midiStatus, midiData1, midiData2);
      }
    }
  }

  handleMidiUsb();
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

void saveMidiMappingsToNvs() {
  nvsPrefs.begin("midi_map", false);
  nvsPrefs.putBytes("cc_map", midiCcMappings, sizeof(midiCcMappings));
  nvsPrefs.end();
  Serial.println("[MIDI] Mapeo guardado en NVS");
}

void loadMidiMappingsFromNvs() {
  nvsPrefs.begin("midi_map", true);
  if (nvsPrefs.isKey("cc_map")) {
    nvsPrefs.getBytes("cc_map", midiCcMappings, sizeof(midiCcMappings));
    Serial.println("[MIDI] Mapeo cargado desde NVS");
  } else {
    //initDefaultMidiMappings();
  }
  nvsPrefs.end();
}

void applyMidiParamUpdate(uint8_t page, uint8_t paramIndex, uint8_t ccValue) {
  // Asegurarnos de que el parámetro está dentro del rango válido
  if (paramIndex >= PARAMS_PER_PAGE) return;

  // 1. MANEJO DE LA PÁGINA ADSR (PAGE_ADSR = 8)
  if (page == PAGE_ADSR) {
    if (paramIndex < TOTAL_ADSR) {
      int minVal = ADSRpage[paramIndex].min;
      int maxVal = ADSRpage[paramIndex].max;
      ADSRvalues[oscSelect][paramIndex] = map(ccValue, 0, 127, minVal, maxVal);
    } else if (paramIndex == 6) { // OSC MIX (0..100)
      ADSRmixValues[0] = map(ccValue, 0, 127, 0, 100);
    } else if (paramIndex == 7) { // DETUNE (0..100)
      ADSRmixValues[1] = map(ccValue, 0, 127, 0, 100);
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
    int mappedValue = map(ccValue, 0, 127, minVal, maxVal);
    
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
