const char* getParamName(Page page, uint8_t param) {
  if (page == PAGE_ADSR) return ADSRpage[param].name;
  else return parametroPages[page][param].name;
}

Type getParamType(Page page, uint8_t param) {
  if (page == PAGE_ADSR) return ADSRpage[param].type;
  else return parametroPages[page][param].type;
}

int getParamValue(Page page, uint8_t param) {
  if (page == PAGE_ADSR) {
      if (param < TOTAL_ADSR) return ADSRvalues[oscSelect][param];
      return ADSRmixValues[param - TOTAL_ADSR];
  }
  else return parametroPages[page][param].value;
}

const char* getNameValue (uint8_t param, int value){ // page * PARAMS_PER_PAGE + param
  switch(param){
    case 6:   return TABLE_SIZES[value]; break;
    case 8:   return LFO_SHAPE_NAMES[value]; break;
    case 11:  return LFO_TARGET_NAMES[value]; break;
    case 18:  return WAVE_NAMES[value];break;
    case 19:  return WAVE_NAMES[value];break;
    case 21:  return MORPH_MODE_NAMES[value];break;
    case 25:  return FX_MODE_NAMES[value];break;
    case 33:  return CHORD_TYPE_NAMES[value]; break;
    case 40:  return SEQ_STATE_NAMES[value]; break;
    case 42:  return SEQ_MODE_NAMES[value]; break;
    case 44:  return CHORD_TYPE_NAMES[value]; break;
    case 47:  return DURATION_NAMES[value]; break;
    case 50:  return ARP_MODE_NAMES[value]; break; 
    case 53:  return ARP_HOLD_NAMES[value]; break;
    default:  return "ERROR"; break;
  }
}

void processEncoders() {
  updateEnc = false;

  Wire.requestFrom(ADDR_ENC, (uint8_t)2);

  if (Wire.available() == 2) {

    uint8_t lowByte  = Wire.read();
    uint8_t highByte = Wire.read();
    uint16_t current = lowByte | (highByte << 8);

    if (current != lastEncState) {

      unsigned long now = millis();

      for (int i = 0; i < ENCODER_COUNT; i++) {

        uint8_t prevAB = (lastEncState >> (i * 2)) & 0x03;
        uint8_t currAB = (current >> (i * 2)) & 0x03;

        if (currAB != prevAB) {

          int8_t delta = encTable[(prevAB << 2) | currAB];
          if (!delta) continue;

          subSteps[i] += delta;

          if (abs(subSteps[i]) >= 4) {
            unsigned long diff = now - lastClickTime[i];
            lastClickTime[i] = now;
            int32_t rango = 1;
            int32_t acc = 1;
            if (currentPage == PAGE_ADSR) rango = ADSRpage[i].max - ADSRpage[i].min; 
            else rango = parametroPages[currentPage][i].max - parametroPages[currentPage][i].min;

            if(rango > 15){
              
              if (diff < 80) {
                  int32_t factor = 80 - diff; 
                  int32_t factorCuadratico = factor * factor; 
                  // Escalado al ~10% del rango del encoder específico 'i'
                  int32_t saltoDinamico = (factorCuadratico * rango) / 64000;
                  acc += saltoDinamico;
              }

              // Límite de seguridad individual (15% máximo de su propio rango)
              int32_t accMaximo = (rango * 15) / 100; 
              if (acc > accMaximo) acc = accMaximo;
            }
            if (acc < 1) acc = 1;
            
            int direccion = (subSteps[i] > 0) ? -1 : 1;

            refreshValue(currentPage, i, (int)(direccion * acc));
            subSteps[i] = 0;
          }
        }
      }

      lastEncState = current;
    }
  }
}

void applyPageDefaultsToggle(Page page, const int defaults[PARAMS_PER_PAGE], int edited[PARAMS_PER_PAGE], bool &usingDefaults) {
  applyingPageDefaultsToggle = true;
  if (!usingDefaults) {
    for (uint8_t i = 0; i < PARAMS_PER_PAGE; i++) {
      edited[i] = parametroPages[page][i].value;
      parametroPages[page][i].value = defaults[i];
      refreshValue(page, i, 0);
      if (currentPage == page) drawValue(i);
    }
    usingDefaults = true;
    Serial.printf("Page %d defaults ON\n", page + 1);
  }
  else {
    for (uint8_t i = 0; i < PARAMS_PER_PAGE; i++) {
      parametroPages[page][i].value = edited[i];
      refreshValue(page, i, 0);
      if (currentPage == page) drawValue(i);
    }
    usingDefaults = false;
    Serial.printf("Page %d defaults OFF\n", page + 1);
  }
  applyingPageDefaultsToggle = false;
}

void toggleADSRDefaults(uint8_t osc) {
  if (!adsrUsingDefaults[osc]) {
    // Guardar los valores actuales editados y cargar los valores por defecto
    for (uint8_t i = 0; i < TOTAL_ADSR; i++) {
      ADSRedited[osc][i] = ADSRvalues[osc][i];
      ADSRvalues[osc][i] = ADSR_DEFAULTS[osc][i];
    }
    adsrUsingDefaults[osc] = true;
    Serial.printf("Osc %d ADSR defaults ON\n", osc);
  } 
  else {
    // Restaurar los valores que el usuario había editado
    for (uint8_t i = 0; i < TOTAL_ADSR; i++) {
      ADSRvalues[osc][i] = ADSRedited[osc][i];
    }
    adsrUsingDefaults[osc] = false;
    Serial.printf("Osc %d ADSR defaults OFF\n", osc);
  }

  // Redibujar la pantalla o la sección ADSR para ver los cambios inmediatamente
  drawMainVisualization(); 
}

void refreshValue(Page page, uint8_t param , int dirAcc){
  uint16_t color = 0;
  if(page < SOUND_PARAM_PAGES) {
    parametroPages[page][param].value = constrain(parametroPages[page][param].value + dirAcc,
                parametroPages[page][param].min,parametroPages[page][param].max);
    const int value = parametroPages[page][param].value;
    uint8_t pageParam = page * PARAMS_PER_PAGE + param;
    switch (pageParam) {
      //pagina 1 CONFIG currentPage = 0
      case 0:  velocityExponent = value * 0.1f; break;
      case 1:  sequencerBpm = (uint16_t)value; break;
      case 2:  varPulse = value * 0.01; 
        regeneratePulseTable(varPulse); 
        break;
      case 3:  masterGain = value * 0.01; break;
      case 4:  break;
      case 5:  maxReleaseVoices = (uint8_t)value; break;
      case 6:  { int selectedIndex = value;
                uint16_t selectedSize = TABLE_SIZE_VALUES[selectedIndex]; //tableSizeFromIndex(selectedIndex);
                changed = selectedSize != tableSize;
                bool saved = !changed;
                if (changed) {
                  saved = saveTableSizeToNvs(selectedSize);
                  drawExtraValue();
                  Serial.printf("[WAV] Cambio TABLE_SIZE -> %u (%s)\n",
                                selectedSize,
                                saved ? "guardado, requiere reset HW" : "error NVS");
                }
                parametroPages[0][6].value = saved ? selectedIndex : tableSizeToIndex(tableSize);
                break;
              }
      case 7: { MemoryMode selectedMode = (value > 0.5f) ? MEMORY_INTERNAL : MEMORY_PSRAM;
                changed = selectedMode != memoryMode;
                if (changed) {
                  bool saved = saveMemoryModeToNvs(selectedMode);
                  drawExtraValue();
                  Serial.printf("[MEM] Cambio modo memoria -> %s (%s)\n",
                                selectedMode == MEMORY_INTERNAL ? "RAM interna primero" : "AUTO (PSRAM primero)",
                                saved ? "guardado, requiere reset HW" : "error NVS");
                  
                }
                parametroPages[0][7].value = (memoryMode == MEMORY_INTERNAL) ? 1.0f : 0.0f;
                break;
              } 
          
      //pagina 2 LFO currentPage = 1
      case 8:  lfoWaveform = (LfoWaveform)value; break;
      case 9:  lfoRateHz = value * 0.1f; break;
      case 10: lfoDepth = value * 0.01f; break;
      case 11: lfoTarget = (LfoTarget)value; break;
      case 12: lfoAttackTime = value * 0.001f; break;
      case 13: cutoffControl = value * 0.1f; 
               filterCutoffHz = cutoffControlToHz(cutoffControl); break;
      case 14: lfoPitchUpdateSamples = (uint8_t)value; break;
      case 15: filterResonance = value * 0.01f; break;

      //pagina 3 GLIDE/MORPH currentPage = 2
      case 16: glideTime = value * 0.001f; break;
      case 17: morphEnabled = value >= 0.5f ? 1.0f : 0.0f;
               color = morphEnabled ? TFT_YELLOW : TFT_DARKGREY;
               drawWaveAudioIcon(oscWaveformEnd[0], 255, 159, color);
               color = morphEnabled ? TFT_CYAN : TFT_DARKGREY;
               drawWaveAudioIcon(oscWaveformEnd[1], 255, 214, color);
               Serial.printf("OSC A WaveForm start %s end %s\n", WAVE_NAMES[oscWaveCacheType[0]], WAVE_NAMES[oscWaveCacheEndType[0]]);
               Serial.printf("OSC B WaveForm start %s end %s\n", WAVE_NAMES[oscWaveCacheType[1]], WAVE_NAMES[oscWaveCacheEndType[1]]);
               break;
      case 18: oscWaveformEnd[0] = (Waveform)value;
               color = morphEnabled ? TFT_YELLOW : TFT_DARKGREY;
               drawWaveAudioIcon(oscWaveformEnd[0], 255, 159, color); 
               leds.setColor(currentPage, RED);
               leds.show(); 
               break;
      case 19: oscWaveformEnd[1] = (Waveform)value;
               color = morphEnabled ? TFT_CYAN : TFT_DARKGREY;
               drawWaveAudioIcon(oscWaveformEnd[1], 255, 214, color); 
               leds.setColor(currentPage, RED);
               leds.show();
               break;
      case 20: morphBase = value * 0.01f; break;
      case 21: morphMode = (MorphMode)value; break; 
      case 22: morphRateHz = value * 0.01f; break; 
      case 23: morphDepth = value * 0.01f; break;
      
      //pagina 4 FX CHORUS currentPage = 3
      case 24: modFxEnabled = value >= 0.5f ? 1.0f : 0.0f; break;
      case 25: modFxMode = (FxMode)value; break;
      case 26: modFxRateHz = value * 0.01f; break;
      case 27: modFxDepthMs = value * 0.1f; break;
      case 28: modFxBaseMs = value * 0.1f; break;
      case 29: modFxFeedback = value * 0.01f; break;
      case 30: modFxMix = value * 0.01; break;
      case 31: modFxStereo = value * 0.01f; break;
  
      //pagina 5 CHORD  currentPage = 4
      case 32: if (sequencerState != SEQ_STATE_OFF) break;
                else{ chordAssistantEnabled = value >= 0.5f; break;}
      case 33: if (sequencerState != SEQ_STATE_OFF) break;
                else{ chordType = (ChordType)value; break; }
      case 34: if (sequencerState != SEQ_STATE_OFF) sequencerSteps[sequencerEditStep].chordInversion = (uint8_t)value;
                else chordInversion = (uint8_t)value;
                break;
      case 35: if (sequencerState != SEQ_STATE_OFF) sequencerSteps[sequencerEditStep].chordOctaveShift = (int8_t)value;
                else chordOctaveShift = (int8_t)value;
                break;
      case 36: if (sequencerState != SEQ_STATE_OFF) sequencerSteps[sequencerEditStep].chordVelocityScale = value * 0.01f;
                else chordVelocityScale = value * 0.01f;
                break;
      case 37: if (sequencerState != SEQ_STATE_OFF) sequencerSteps[sequencerEditStep].chordSpread = value * 0.01f;
                else chordSpread = value * 0.01f;
                break;
      case 38: if (sequencerState != SEQ_STATE_OFF) sequencerSteps[sequencerEditStep].chordStrumMs = (uint8_t)value;
                else chordStrumMs = (uint8_t)value;
                break;
      case 39: if (sequencerState != SEQ_STATE_OFF) sequencerSteps[sequencerEditStep].chordDensity = (uint8_t)value;
                else chordDensity = (uint8_t)value;
                break;
      
      //pagina 6 SEQUENCER currentPage = 5
      case 40:
        sequencerState = (SequencerState)value;
        sequencerEnabled = sequencerState == SEQ_STATE_ON;
        sequencerPlayStep = 0;
        sequencerPlayBar = 1;
        sequencerUiStepShown = 0xFF;
        sequencerUiBarShown = 0xFF;
        sequencerNextStepMs = millis();
        sequencerStepStartMs = sequencerNextStepMs;
        sequencerArpNextMs = sequencerNextStepMs;
        sequencerArpOffMs = sequencerNextStepMs;
        sequencerArpNoteOn = false;
        if (sequencerState != SEQ_STATE_ON) {
          sequencerGateActive = false;
          releaseAllVoices();
        }
        syncSequencerScopedValues(sequencerEditStep);
        break;
      case 41:
        sequencerEditStep = (uint8_t)value - 1;
        if (sequencerState != SEQ_STATE_OFF) syncSequencerScopedValues(sequencerEditStep);
        parametroPages[5][2].value = sequencerSteps[sequencerEditStep].mode;
        parametroPages[5][3].value = sequencerSteps[sequencerEditStep].root;
        parametroPages[5][4].value = sequencerSteps[sequencerEditStep].chord;
        parametroPages[5][5].value = sequencerSteps[sequencerEditStep].bars;
        parametroPages[5][6].value = sequencerSteps[sequencerEditStep].velocity;
        parametroPages[5][7].value = selectedDurationIdx;
        if (!suppressUiRefresh && page == PAGE_SEQ) {
          for (uint8_t j = 2; j < 8; j++) drawValue(j);
        }
        break;
      case 42: sequencerSteps[sequencerEditStep].mode = (SequencerMode)value;break;  
      case 43: sequencerSteps[sequencerEditStep].root = (uint8_t)value; break;
      case 44: sequencerSteps[sequencerEditStep].chord = (ChordType)value;
                if (sequencerSteps[sequencerEditStep].chord != CHORD_PLAYED) sequencerSteps[sequencerEditStep].playedCount = 0;
                break;
      case 45: sequencerSteps[sequencerEditStep].bars = (uint8_t)value; break;
      case 46: sequencerSteps[sequencerEditStep].velocity = (uint8_t)value; break;
      case 47: selectedDurationIdx = DURATION_PRESETS[value];
                sequencerSteps[sequencerEditStep].melodyDurations[MAX_MELODY_NOTES] = selectedDurationIdx;
                break;

      //pagina 7 ARPPEGIATOR currentPage = 6
      case 48: if (sequencerState != SEQ_STATE_OFF) break;
                else{
                  arpEnabled = value >= 0.5f; 
                  if (!arpEnabled) arpClearNotes(); break;
                }
      case 49: if (sequencerState != SEQ_STATE_OFF) sequencerSteps[sequencerEditStep].arpRateHz = value * 0.1f;
                else arpRateHz = value  * 0.1f;
                break;
      case 50: if (sequencerState != SEQ_STATE_OFF) sequencerSteps[sequencerEditStep].arpMode = (ArpMode)value;
                else arpMode = (ArpMode)value;
                break;
      case 51: if (sequencerState != SEQ_STATE_OFF) sequencerSteps[sequencerEditStep].arpOctaves = value;
                else arpOctaves = value;
                break;
      case 52: if (sequencerState != SEQ_STATE_OFF) sequencerSteps[sequencerEditStep].arpGate = value * 0.01f;
                else arpGate = value * 0.01f;
                break;
      case 53: if (sequencerState != SEQ_STATE_OFF) break;
                else{
                  arpHold = (ArpHold)value;
                  parametroPages[6][5].value = arpHold; break;
                }
      case 54: if (sequencerState != SEQ_STATE_OFF) sequencerSteps[sequencerEditStep].arpSwing = value * 0.01;
                else arpSwing = value * 0.01;
                break;
      case 55: if (sequencerState != SEQ_STATE_OFF) sequencerSteps[sequencerEditStep].arpPatternMask = (uint8_t)value;
                else arpPatternMask = value;
                drawExtraValue();
                break;
  
      //Pagina 8 FILE currentPage 7
      case 56: presetSelectFileByIndex(parametroPages[PAGE_FILE][param].value); break;
      case 57:
        if (parametroPages[PAGE_FILE][param].value == 1) {
          finalizePresetName();
          bool ok = presetLoadByName(presetEditName);
          Serial.printf("Load preset %s -> %s\n", presetEditName, ok ? "OK" : "FAIL");
          setPresetActionFeedback(1, ok ? "OK" : "FAIL");
          parametroPages[PAGE_FILE][param].value = 0;
        }
        break;

      case 58:
        if (parametroPages[PAGE_FILE][param].value == 1) {
          finalizePresetName();
          bool ok = presetSaveByName(presetEditName);
          Serial.printf("Save preset %s -> %s\n", presetEditName, ok ? "OK" : "FAIL");
          setPresetActionFeedback(2, ok ? "OK" : "FAIL");
          parametroPages[PAGE_FILE][param].value = 0;
          refreshPresetFileList(true);
        }
        break;
      case 59:
        if (parametroPages[PAGE_FILE][param].value == 1){
          setPresetActionFeedback(3, "¿?");
          leds.setColor(currentPage, RED);
          leds.show();
        }
        if (parametroPages[PAGE_FILE][param].value == 10) {
          finalizePresetName();
          bool ok = presetDeleteByName(presetEditName);
          Serial.printf("Delete preset %s -> %s\n", presetEditName, ok ? "OK" : "FAIL");
          setPresetActionFeedback(3, ok ? "OK" : "FAIL");
          parametroPages[PAGE_FILE][param].value = 0;
          refreshPresetFileList(false);
          leds.setColor(currentPage, GREEN);
          leds.show();
        }
        break;
      case 60:
        if (parametroPages[PAGE_FILE][param].value == 1){
          setPresetActionFeedback(3, "¿?");
          leds.setColor(currentPage, RED);
          leds.show();
        }
        if (parametroPages[PAGE_FILE][param].value == 10) {
          memset(presetEditName, ' ', PN_LEN);
          presetEditName[PN_LEN] = '\0';
          setPresetActionFeedback(4, "CLEAR");
          parametroPages[PAGE_FILE][param].value = 0;
          leds.setColor(currentPage, GREEN);
          leds.show();
        }
        break;
      case 61:
        if (parametroPages[PAGE_FILE][param].value == 1) {
          presetInsertSelectedChar(true);
          setPresetActionFeedback(5, "WRITE");
          parametroPages[PAGE_FILE][param].value = 0;
        }
        break;
      case 62: break;
      case 63: break;
    }

  }

  //pagina 9 ADSR currentPage = 8
  else if(page == PAGE_ADSR) { 
    if (param < TOTAL_ADSR) {
      ADSRvalues[oscSelect][param] = constrain(ADSRvalues[oscSelect][param] + dirAcc,
        ADSRpage[param].min, ADSRpage[param].max);    
      ADSRedited[oscSelect][param] = ADSRvalues[oscSelect][param];
      adsrUsingDefaults[oscSelect] = false;
    }
    else{
      uint8_t mixIdx = param - TOTAL_ADSR; // 0 para OSC MIX, 1 para DETUNE
      ADSRmixValues[mixIdx] = constrain(ADSRmixValues[mixIdx] + dirAcc,
        ADSRpage[param].min, ADSRpage[param].max);
    }
    updateEnvelopeRates(oscSelect);
  }

  if (!applyingPageDefaultsToggle) {
    if (page == PAGE_LFO && page1UsingDefaults) {
      for (uint8_t i = 0; i < PARAMS_PER_PAGE; i++) page1EditedValues[i] = parametroPages[1][i].value;
      page1UsingDefaults = false;
      if (!suppressUiRefresh && currentPage == PAGE_LFO) drawExtraValue();
    }
    
    else if (page == PAGE_CHORUS && page3UsingDefaults) {
      for (uint8_t i = 0; i < PARAMS_PER_PAGE; i++) page3EditedValues[i] = parametroPages[3][i].value;
      page3UsingDefaults = false;
      if (!suppressUiRefresh && currentPage == PAGE_CHORUS) drawExtraValue();
    }

  }

  if (!suppressUiRefresh) {
    drawValue(param);
    if (page == PAGE_FILE || page == PAGE_ADSR) {
      drawMainVisualization();
    }

    Serial.printf("Page %d Param %d = %.2f\n", page + 1, param, getParamValue(page, param));
  }
}

void resetADSR(uint8_t osc){
  adsrUsingDefaults[osc] = !adsrUsingDefaults[osc];
  if(adsrUsingDefaults[osc]){
    for(byte i=0;i<TOTAL_ADSR;i++){
      ADSRedited[osc][i] = ADSRvalues[osc][i];
      ADSRvalues[osc][i] = ADSR_DEFAULTS[osc][i];
      drawValue(i);
    }
    adsrUsingDefaults[osc] = true;
  } else{
    for(byte i=0;i<TOTAL_ADSR;i++){
      ADSRvalues[osc][i] = ADSRedited[osc][i];
      drawValue(i);
    }
    adsrUsingDefaults[osc] = false;
  }
  updateEnvelopeRates(osc);
  drawMainVisualization();
}

void setPage(Page page){
  if(page >= PARAM_PAGES) return;
  if(page != currentPage){
    currentPage = page;
    if (sequencerState != SEQ_STATE_OFF && (currentPage == PAGE_CHORD || currentPage == PAGE_ARP)) {
      syncSequencerScopedValues(sequencerEditStep);
    }
    drawUI();
    leds.setColor(lastPage, OFF);
    leds.setColor(currentPage, GREEN);
    leds.show();
    lastPage = currentPage;
  }
  else{
    switch(page){
      case PAGE_LFO:
        applyPageDefaultsToggle(page, PAGE1_DEFAULTS, page1EditedValues, page1UsingDefaults);
      break;
      case PAGE_MORPH:
        syncActiveWaveCache(0, oscWaveform[0], WAVE_START);
        syncActiveWaveCache(1, oscWaveform[1], WAVE_START);
        syncActiveWaveCache(0, oscWaveformEnd[0], WAVE_END);
        syncActiveWaveCache(1, oscWaveformEnd[1], WAVE_END);
        
        Serial.printf("OSC A WaveForm start %s end %s\n", WAVE_NAMES[oscWaveCacheType[0]], WAVE_NAMES[oscWaveCacheEndType[0]]);
        Serial.printf("OSC B WaveForm start %s end %s\n", WAVE_NAMES[oscWaveCacheType[1]], WAVE_NAMES[oscWaveCacheEndType[1]]);
        leds.setColor(currentPage, GREEN);
        leds.show();
      break;
      case PAGE_CHORUS:
        applyPageDefaultsToggle(page, PAGE3_DEFAULTS, page3EditedValues, page3UsingDefaults);
      break;
      case PAGE_SEQ:
        seqCopyStep(sequencerEditStep);
      break;
      case PAGE_FILE:
        presetInsertSelectedChar(true);
        //setPresetStatus("CHAR OK", 800);
      break;
      case PAGE_ADSR:
        syncActiveWaveCache(0, oscWaveform[0], WAVE_START);
        syncActiveWaveCache(1, oscWaveform[1], WAVE_START);
        leds.setColor(currentPage, GREEN);
        leds.show();
        drawExtraValue();
      break;
    }
  }
  Serial.printf("Page -> %d\n", currentPage + 1);
}

void processButtons() {
  updateBtn = false;
  Wire.requestFrom(ADDR_BTN, (uint8_t)1);
  if (!Wire.available()) return;

  uint8_t current = Wire.read();

  for (int i = 0; i < ENCODER_COUNT; i++) {
    bool pressed = !bitRead(current, i) && bitRead(lastBtnState, i);
    
    if (pressed) {
      if (currentPage == PAGE_FILE && i == 7) {
        presetInsertSelectedChar(true);
        //setPresetStatus("CHAR OK", 800);
      }
      else if(i < PARAM_PAGES) {
        setPage((Page)i);
      }
    }
  }
  lastBtnState = current;
}

void processControl(){
  enc = encoder.read();
  if (enc){ //Encoder
    if(currentPage < SOUND_PARAM_PAGES){
      switch(currentPage){
        case PAGE_CONF:
          
        break;
        case PAGE_LFO:
          applyPageDefaultsToggle(currentPage, PAGE1_DEFAULTS, page1EditedValues, page1UsingDefaults);
        break;
        case PAGE_MORPH:
          oscWaveform[oscSelect] = (Waveform)constrain((int)oscWaveform[oscSelect] + enc, 0, (int)WAVE_COUNT);
          if(oscWaveform[oscSelect] == WAVE_COUNT && enc == 1) oscWaveform[oscSelect] = WAVE_SAW;
          if(oscWaveform[oscSelect] == WAVE_SAW && enc == -1) oscWaveform[oscSelect] = WAVE_NOISE;
          drawWaveAudioIcon(oscWaveform[oscSelect], 185, oscSelect ? 214 : 159, oscSelect ? TFT_CYAN : TFT_YELLOW);
          leds.setColor(currentPage, RED);
          leds.show();
        break;
        case PAGE_CHORUS:
          applyPageDefaultsToggle(currentPage, PAGE3_DEFAULTS, page3EditedValues, page3UsingDefaults);
        break;
        case PAGE_CHORD:
        break;
        case PAGE_SEQ:
          sequencerTransitionMode = (SeqTransitionMode)constrain((int)sequencerTransitionMode + enc, 0, SEQ_TRANS_COUNT - 1);
        break;
        case PAGE_ARP:
          
        break;
       }
      drawExtraValue();
    }
    else if(currentPage == PAGE_ADSR){
      oscWaveform[oscSelect] = (Waveform)constrain((int)oscWaveform[oscSelect] + enc, 0, (int)WAVE_COUNT);
      if(oscWaveform[oscSelect] == WAVE_COUNT && enc == 1) oscWaveform[oscSelect] = WAVE_SAW;
      if(oscWaveform[oscSelect] == WAVE_SAW && enc == -1) oscWaveform[oscSelect] = WAVE_NOISE;
      drawWaveAudioIcon(oscWaveform[oscSelect], 255, oscSelect ? 214 : 159, oscSelect ? TFT_CYAN : TFT_YELLOW); 
      drawExtraValue();
      leds.setColor(currentPage, RED);
      leds.show();
    }
  }

  botEnc = !digitalRead(PIN_ENC_BOT); //select ADSR control
  if (botEnc != botEnc_ant) {
    lastDebounceTime = millis();
  }
  if ((millis() - lastDebounceTime) > debounceDelay) { 
    if (botEnc && !botEnc_estable) {
      setPage(PAGE_ADSR);
    }
    botEnc_estable = botEnc;
  }
  botEnc_ant = botEnc;
  
  botAz = !digitalRead(PIN_AZ); //selected osc 1
  if (botAz != botAz_ant) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (botAz && !botAz_estable) {
      if(oscSelect != 1){  
        oscSelect = 1;
        drawMainVisualization();
      }
      else toggleADSRDefaults(1);
    }
    
    botAz_estable = botAz;
  }
  botAz_ant = botAz;

  botAm = !digitalRead(PIN_AM); //Selected osc 0
  if (botAm != botAm_ant) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (botAm && !botAm_estable) {
      if(currentPage == PAGE_SEQ){
        uint8_t step = sequencerEditStep & 0x07;
        sequencerSteps[step].layerChord = !sequencerSteps[step].layerChord; 
        drawExtraValue();
      }
      else{
        if (oscSelect != 0) {
          oscSelect = 0;
          drawMainVisualization();
        }
        else toggleADSRDefaults(0);
      }
    }
    
    botAm_estable = botAm;
  }
  botAm_ant = botAm;

  botBlue = digitalRead(PIN_TACTIL); //latch
  if (botBlue != botBlue_ant) {
    lastBlueDebounceTime = millis();
  }
  if ((millis() - lastBlueDebounceTime) > debounceDelay) {
    if (botBlue != botBlue_estable) {
      botBlue_estable = botBlue;
      latchEnabled = botBlue_estable;
      if (!latchEnabled) {
        if (arpEnabled) arpReleaseLatchedNotes();
        else if (midiKeysPressedCount == 0) releaseAllVoices();
      }
      if (currentPage == PAGE_ARP) drawValue(5);
      Serial.printf("Latch -> %s\n", latchEnabled ? "ON" : "OFF");
    }
  }
  botBlue_ant = botBlue;
}

void timeOutFile(){
  flagTime = false;
  drawValue(pendingParam);
}

void drawValue(uint8_t i){
  if (!isSequencerScopedControl(currentPage, i)) {
    tft.fillRect((i&3)*80, 28 + ((i>>2)*60), 80, 22, TFT_BLACK);
    return;
  }

  char buf[12];
  char charBuf[2] = {0};
  const char* textValue = nullptr;
  uint8_t pos = 40;
  int value = getParamValue(currentPage, i);
  switch(getParamType(currentPage, i)){
    
    case INT:    
      if (currentPage == PAGE_FILE && i == 0 && presetFileCount > 0) {
        snprintf(buf, sizeof(buf), "%d", value + 1);
      }
      else {
        snprintf(buf, sizeof(buf), "%d", value);
      }
      textValue = buf;
      break;
    
    case ONOFF:
      if (currentPage == PAGE_CONF && i == 7) {
        textValue = (bool)value ? "RAM" : "PSRAM";
      } else {
        textValue = (bool)value ? "ON" : "OFF";
      }
      break;
    
    case NAME:
      textValue = getNameValue((currentPage * PARAMS_PER_PAGE) + i, value);
      break;

    case CHARSEL:
      charBuf[0] = PRESET_CHARS[value];
      textValue = charBuf;
      break;

    case NULO:
      return;
      break;

    case FFILE:
      if ((i > 0 || i < 6) {
        textValue = presetActionLabel;
        pendingParam = i;
      }
      else {
        textValue = "GO";
      }
      if(textValue == "GO" && i == 3){
        parametroPages[PAGE_FILE][i].value = 0;
        leds.setColor(currentPage, GREEN);
        leds.show();
      }
      break;

  }
  tft.setFreeFont(NUM_TEXT);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(TFT_WHITE);
  tft.fillRect((i&3)*80, 28 + ((i>>2)*60), 80, 22, TFT_BLACK);
  tft.drawString(textValue, pos + ((i&3)*80), 30 + ((i>>2)*60));
}

void uint8ToBinaryStr(uint8_t num, char *buffer) {
  for (int i = 7; i >= 0; i--) {
    // Verificamos cada bit y guardamos '1' o '0'
    buffer[7 - i] = (num & (1 << i)) ? '1' : '0';
  }
  buffer[8] = '\0'; // Terminador de cadena
}

void drawExtraValue(){
  char buf[20] = "";
  const int x = 10;
  const int y = 130;
  tft.setFreeFont(NUM_TEXT);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_WHITE);
  tft.fillRect(x, y, 170, 20, TFT_BLACK);
  switch(currentPage){
    case PAGE_CONF: 
      snprintf(buf, sizeof(buf), "%s", changed ? "RESET HW" : "");
      break;
    case PAGE_LFO: 
      snprintf(buf, sizeof(buf), "%s", page1UsingDefaults ? "DEFAULT" : "EDIT"); 
      break;
    case PAGE_MORPH: 
      snprintf(buf, sizeof(buf), "%s",  WAVE_LONG_NAMES[oscWaveform[oscSelect]]);
      break;
    case PAGE_CHORUS: 
      snprintf(buf, sizeof(buf), "%s", page3UsingDefaults ? "DEFAULT" : "EDIT"); 
      break;
    case PAGE_CHORD:
      snprintf(buf, sizeof(buf), "%s I%d", CHORD_TYPE_NAMES[(int)chordType], chordInversion);
      break;
    case PAGE_SEQ:
      snprintf(buf, sizeof(buf), "%s S%d B%d %s", SEQ_TRANSITION_NAMES[(int)sequencerTransitionMode], sequencerPlayStep + 1, 
      sequencerPlayBar, sequencerSteps[sequencerEditStep].layerChord ? "+CHORD" : "");
      break;
    case PAGE_ARP: 
      uint8ToBinaryStr(arpPatternMask, buf);
      break;
    case PAGE_FILE: {
      char nameBuf[PN_LEN + 1];
      strncpy(nameBuf, presetEditName, PN_LEN);
      nameBuf[PN_LEN] = '\0';
      int pos = constrain(parametroPages[PAGE_FILE][6].value, 0, PN_LEN - 1);
      char prefix[PN_LEN + 1];
      strncpy(prefix, nameBuf, pos);
      prefix[pos] = '\0';
      char selected[2] = {nameBuf[pos], '\0'};
      if (selected[0] == '\0') selected[0] = ' ';
      const char* suffix = (nameBuf[pos] != '\0') ? &nameBuf[pos + 1] : "";

      tft.setCursor(x, y + 18);
      tft.setTextColor(TFT_WHITE);
      tft.print(prefix);
      tft.setTextColor(TFT_RED);
      tft.print(selected);
      tft.setTextColor(TFT_WHITE);
      tft.print(suffix);
      break;
    }
    case PAGE_ADSR:
      snprintf(buf, sizeof(buf), "%s: %s", OSC_NAME[oscSelect], WAVE_LONG_NAMES[oscWaveform[oscSelect]]);
      break;
  }
  if (currentPage != PAGE_FILE) {
    tft.drawString(buf, x, y);
  }
}

void drawLinesADSR(uint8_t osc){
  int x0 = 5;
  int y0 = 238;
  int width = 235;
  int height = 79;

  // 1. Extraemos y normalizamos los valores del oscilador ACTUAL (0.0f a 1.0f)
  float L_norm = (ADSRvalues[osc][DELAY] * 1.0f) / ADSRpage[DELAY].max;
  float A_norm = (ADSRvalues[osc][ATTACK] * 1.0f) / ADSRpage[ATTACK].max;
  float AL     = (ADSRvalues[osc][ATTACK_LEV] * 1.0f) / ADSRpage[ATTACK_LEV].max;
  float D_norm = (ADSRvalues[osc][DECAY] * 1.0f) / ADSRpage[DECAY].max;
  float S      = (ADSRvalues[osc][SUSTAIN] * 1.0f) / ADSRpage[SUSTAIN].max;
  float R_norm = (ADSRvalues[osc][RELEASE] * 1.0f) / ADSRpage[RELEASE].max;
  uint16_t color = (osc == 0) ? TFT_YELLOW : TFT_CYAN;

  // Aplicamos la raíz cuadrada para la sensibilidad visual
  float L = sqrtf(L_norm);
  float A = sqrtf(A_norm);
  float D = sqrtf(D_norm);
  float R = sqrtf(R_norm);
  float sustainTime = 0.35f;

  // 2. CONGRUENCIA TEMPORAL: Calculamos los mismos valores para el OTRO oscilador
  uint8_t otroOsc = (osc == 0) ? 1 : 0;
  float L_otro = sqrtf((ADSRvalues[otroOsc][DELAY] * 1.0f) / ADSRpage[DECAY].max);
  float A_otro = sqrtf((ADSRvalues[otroOsc][ATTACK] * 1.0f) / ADSRpage[ATTACK].max);
  float D_otro = sqrtf((ADSRvalues[otroOsc][DECAY] * 1.0f) / ADSRpage[DECAY].max);
  float R_otro = sqrtf((ADSRvalues[otroOsc][RELEASE] * 1.0f) / ADSRpage[RELEASE].max);

  // 3. Buscamos el "peor escenario" (la suma de tiempos más larga entre los dos osciladores)
  float sumaOscActual = L + A + D + sustainTime + R;
  float sumaOscOtro   = L_otro + A_otro + D_otro + sustainTime + R_otro;
  
  // El total de la pantalla se adaptará al oscilador que tenga la envolvente más larga
  float totalWeights = (sumaOscActual > sumaOscOtro) ? sumaOscActual : sumaOscOtro;

  // 5 Sin minimos
  int wL = (int)((L / totalWeights) * width);
  int wA = (int)((A / totalWeights) * width);
  int wD = (int)((D / totalWeights) * width);
  int wS = (int)((sustainTime / totalWeights) * width);
  // Si este es el oscilador más largo, ajustamos el final absorbiendo el resto exacto
  int wR;
  if (sumaOscActual >= sumaOscOtro) {
    wR = width - (wL + wA + wD + wS);
  } else {
    wR = (int)((R / totalWeights) * width);
  }

  // 6. Construir las coordenadas X
  int xL = x0 + wL;
  int xA = xL + wA;
  int xD = xA + wD;
  int xS = xD + wS;
  int xR = xS + wR; 

  // Ajuste de seguridad por si es el oscilador más largo y hay decimales flotantes
  if (xR > (x0 + width) || (sumaOscActual >= sumaOscOtro && xR < (x0 + width))) {
    xR = x0 + width; 
  }

  // 7. Calcular las coordenadas Y
  int yAtk = y0 - (int)(height * AL);
  int ySus = y0 - (int)(height * S);

  // 8. Renderizado
  tft.drawLine(x0, y0, xL, y0, color);       
  tft.drawLine(xL, y0, xA, yAtk, color);     
  tft.drawLine(xA, yAtk, xD, ySus, color);   
  tft.drawLine(xD, ySus, xS, ySus, color);   
  tft.drawLine(xS, ySus, xR, y0, color);     
}


void drawADSR(){
  char buf[9];
  uint8_t oscFront = (uint8_t)oscSelect;
  uint8_t oscBack = (uint8_t)!oscSelect;
  uint16_t color = (oscSelect == 0) ? TFT_YELLOW : TFT_CYAN;
  
  //int x0 = 5;
  //int y0 = 240;
  //int width = 235;
  //int height = 80;
  //     |left,top,240-left,240-top
  tft.fillRect(5,159,235,80,TFT_BLACK);
  tft.drawRect(4,158,237,82,TFT_DARKGREY);
  
  drawLinesADSR(oscBack);
  drawLinesADSR(oscFront);

}

void drawAudioWaveform(){
  const int x = 5;
  const int y = 160;
  const int w = (currentPage == PAGE_MORPH) ? 160 : 220;
  const int h = 75;
  const int left = x + 2;//8
  const int right = x + w - 3;//236
  const int top = y + 4;//165
  const int bottom = y + h - 6;//233
  const int midY = y + h / 2;//120
  const int amp = (bottom - top) / 2;
  int16_t samples[AUDIO_SCOPE_SAMPLES];
  int32_t sum = 0;
  uint16_t writeIndex = audioScopeWriteIndex;

  for (uint16_t i = 0; i < AUDIO_SCOPE_SAMPLES; i++) {
    uint16_t idx = writeIndex + i;
    if (idx >= AUDIO_SCOPE_SAMPLES) idx -= AUDIO_SCOPE_SAMPLES;
    samples[i] = audioScopeBuffer[idx];
    sum += samples[i];
  }
  int32_t center = sum / AUDIO_SCOPE_SAMPLES;
  int32_t peak = 1;

  for (uint16_t i = 0; i < AUDIO_SCOPE_SAMPLES; i++) {
    int32_t centered = (int32_t)samples[i] - center;
    int32_t absCentered = abs(centered);
    if (absCentered > peak) peak = absCentered;
  }

  uint16_t start = 0;
  int32_t bestSlope = 0;
  for (uint16_t i = 1; i < AUDIO_SCOPE_SAMPLES; i++) {
    if (((int32_t)samples[i - 1] - center) < 0 && ((int32_t)samples[i] - center) >= 0) {
      int32_t slope = (int32_t)samples[i] - (int32_t)samples[i - 1];
      if (slope > bestSlope) {
        bestSlope = slope;
        start = i;
      }
    }
  }

  tft.fillRect(x, y, w, h, TFT_BLACK);
  tft.drawFastHLine(left, midY, right - left + 1, TFT_DARKGREY);

  int prevX = left;
  int prevY = midY - (((int32_t)samples[start] - center) * amp / peak);
  prevY = constrain(prevY, top, bottom);

  for (int px = left + 1; px <= right; px++) {
    uint16_t idx = (start + ((uint32_t)(px - left) * (AUDIO_SCOPE_SAMPLES - 1)) / (right - left)) % AUDIO_SCOPE_SAMPLES;
    int yy = midY - (((int32_t)samples[idx] - center) * amp / peak);
    yy = constrain(yy, top, bottom);
    tft.drawLine(prevX, prevY, px, yy, TFT_GREEN);
    prevX = px;
    prevY = yy;
  }
}

void drawPresetFileList(){
  const int x = 5;
  const int y = 159;
  const int w = 235;
  const int h = 80;
  const uint8_t cols = 2;
  const uint8_t rows = 5;
  const uint8_t pageSize = cols * rows;
  const int colW = w / cols;
  const int rowH = 13;

  tft.fillRect(x, y, w, h, TFT_BLACK);
  tft.drawRect(x-1, y-1, w+2, h+2, TFT_DARKGREY);
  tft.setFreeFont(LAB_TEXT);
  tft.setTextDatum(TL_DATUM);

  if (presetFileCount == 0) {
    tft.setTextColor(TFT_DARKGREY);
    tft.drawString("NO FILES", x + 8, y + 32);
    return;
  }

  uint8_t selected = (uint8_t)constrain(parametroPages[PAGE_FILE][0].value, 0, presetFileCount - 1);
  uint8_t pageStart = (selected / pageSize) * pageSize;

  for (uint8_t slot = 0; slot < pageSize; slot++) {
    uint8_t fileIndex = pageStart + slot;
    if (fileIndex >= presetFileCount) break;

    uint8_t col = slot % cols;
    uint8_t row = slot / cols;
    int tx = x + 4 + (col * colW);
    int ty = y + 5 + (row * rowH);
    bool isSelected = fileIndex == selected;

    if (isSelected) {
      
      tft.setTextColor(TFT_YELLOW);
    }
    else {
      tft.setTextColor(TFT_WHITE);
    }

    char nameBuf[PN_LEN + 1];
    strncpy(nameBuf, presetFileNames[fileIndex], sizeof(nameBuf) - 1);
    nameBuf[sizeof(nameBuf) - 1] = '\0';
    tft.drawString(nameBuf, tx, ty);
  }
}

void  drawMainVisualization(){
  if (currentPage == PAGE_ADSR) {
    for(byte i=0;i<8;i++) drawValue(i);
    drawADSR();
    drawWaveAudioIcon(oscWaveform[0], 255, 159, oscSelect ? TFT_DARKGREY : TFT_YELLOW);
    drawWaveAudioIcon(oscWaveform[1], 255, 214, oscSelect ? TFT_CYAN : TFT_DARKGREY);
    drawExtraValue();
    SimpleColor color = oscSelect ? CYAN : YELLOW;
    leds.setColor(9, color);
    leds.show();
  }
  else if (currentPage == PAGE_FILE) {
    drawPresetFileList();
  }

  else if (currentPage == PAGE_MORPH){  //Morphing
    drawAudioWaveform();
    drawWaveAudioIcon(oscWaveform[0], 185, 159, oscSelect ? TFT_DARKGREY : TFT_YELLOW);
    drawWaveAudioIcon(oscWaveform[1], 185, 214, oscSelect ? TFT_CYAN : TFT_DARKGREY);
    drawWaveAudioIcon(oscWaveformEnd[0], 255, 159, morphEnabled ? TFT_YELLOW : TFT_DARKGREY);
    drawWaveAudioIcon(oscWaveformEnd[1], 255, 214, morphEnabled ? TFT_CYAN : TFT_DARKGREY);
    drawExtraValue();
    SimpleColor color = oscSelect ? CYAN : YELLOW;
    leds.setColor(9, color);
    leds.show();
  }
  else {
    drawAudioWaveform();
    drawWaveAudioIcon(oscWaveform[0], 255, 159, TFT_YELLOW);
    drawWaveAudioIcon(oscWaveform[1], 255, 214, TFT_CYAN);
    drawExtraValue();
    SimpleColor color = oscSelect ? CYAN : YELLOW;
    leds.setColor(9, color);
    leds.show();
  }
}

void refreshAudioScope(){
  static uint32_t lastDrawMs = 0;
  static uint32_t lastSerial = 0;

  if (currentPage == PAGE_ADSR || currentPage == PAGE_FILE) return;
  if (millis() - lastDrawMs < 80) return;
  if (audioScopeSerial == lastSerial) return;

  lastDrawMs = millis();
  lastSerial = audioScopeSerial;
  drawAudioWaveform();
}

void drawLabels(){
  tft.setFreeFont(LAB_TEXT);
  tft.setTextDatum(TC_DATUM);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_GREEN);
  for(byte i=0;i<8;i++){
    if (!isSequencerScopedControl(currentPage, i)) continue;
    tft.drawString(getParamName(currentPage, i), 40 + ((i&3)*80), 5 + ((i>>2)*60));
  }
  
}

void drawWaveAudioIcon(uint8_t idWave, int posX, int posY, uint16_t color) {
  // Seguridad: Si el oscilador es inválido o el ID de onda supera el catálogo, salimos.
  if (idWave < 0 || idWave >= WAVE_COUNT) return;
  
  // A. Limpiamos el lienzo del único Sprite antes de pintar la nueva onda
  menuSprite.fillSprite(TFT_BLACK);
  
  int centroY = ICON_H / 2;
  int ultimoX = 0;
  int ultimoY = centroY;
  
  // B. Dibujamos la geometría dentro del Sprite leyendo la caché ultrarrápida
  for (int x = 1; x < ICON_W - 1; x++) {
    int yPixel = cacheGraficaOndas[idWave][x];
    menuSprite.drawLine(ultimoX, ultimoY, x, yPixel, color);
    ultimoX = x;
    ultimoY = yPixel;
  }
  menuSprite.drawLine(ultimoX, ultimoY, ICON_W - 1, centroY, color);

  // C. Estampamos el Sprite en la pantalla ST7789. 
  // El Sprite se libera inmediatamente para poder ser reutilizado.
  menuSprite.pushSprite(posX, posY);
  // texto
  tft.fillRect(posX,posY-18,60,17,TFT_BLACK);
  tft.setFreeFont(LAB_TEXT);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(color);
  tft.drawString(WAVE_NAMES[idWave], posX + 30, posY -19); 
}

void drawUI(){
  drawLabels();
  for(byte i=0;i<8;i++){
    drawValue(i);
  }
  drawExtraValue(); 
  drawMainVisualization();
}
