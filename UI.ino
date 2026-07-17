const char* getParamName(uint8_t page, uint8_t param) {
  if (page == ADSR_PARAM_PAGE) return ADSRpage[param].name;
  if (page < SOUND_PARAM_PAGES) return parametroPages[page][param].name;
  return filePageParams[param].name;
}

Type getParamType(uint8_t page, uint8_t param) {
  if (page == ADSR_PARAM_PAGE) return ADSRpage[param].type;
  if (page < SOUND_PARAM_PAGES) return parametroPages[page][param].type;
  return filePageParams[param].type;
}

float getParamValue(uint8_t page, uint8_t param) {
  if (page == ADSR_PARAM_PAGE && param < TOTAL_ADSR) {
    float value = ADSRvalues[oscSelect][param];
    if (param == ADSR_DELAY || param == ATTACK || param == DECAY || param == RELEASE) return value * 1000.0f;
    if (param == ATTACK_LEVEL || param == SUSTAIN) return value * 100.0f;
    return value;
  }
  if (page == ADSR_PARAM_PAGE && param == 6) return synthValue[0][0] * 100.0f;
  if (page == ADSR_PARAM_PAGE && param == 7) return synthValue[0][1] * 100.0f;
  if (page < SOUND_PARAM_PAGES) return synthValue[page][param];
  return (float)filePageParams[param].value;
}

int getParamStep(uint8_t page, uint8_t param) {
  if (page == FILE_PARAM_PAGE) return max(1, filePageParams[param].step);
  return 0;
}

const char* getNameValue (uint8_t param, int value){ // page * PARAMS_PER_PAGE + param
  switch(param){
    case 6:   return TABLE_SIZES[value]; break;
    case 8:  return LFO_SHAPE_NAMES[value]; break;
    case 11:  return LFO_TARGET_NAMES[value]; break;
    case 18:  return WAVE_NAMES[value];break;
    case 19:  return WAVE_NAMES[value];break;
    case 21:  return MORPH_MODE_NAMES[value];break;
    case 25:  return FX_MODE_NAMES[value];break;
    case 33:  return CHORD_TYPE_NAMES[value]; break;
    case 40:  return SEQ_STATE_NAMES[value]; break;
    case 42:  return SEQ_MODE_NAMES[value]; break;
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

            int acc = (diff < 80) ? (1 + (80 - diff) / 8) : 1;
            if (acc > 10) acc = 10;

            
            int direccion = (subSteps[i] > 0) ? -1 : 1;

            if(currentPage == FILE_PARAM_PAGE){
              int step = getParamStep(currentPage, i);
              filePageParams[i].value = constrain(
                filePageParams[i].value + direccion * step,
                filePageParams[i].min,
                filePageParams[i].max
              );
              refreshValue(currentPage, i);
            }
            else if(currentPage == ADSR_PARAM_PAGE){
              float uiValue = constrain(
                getParamValue(currentPage, i) + (direccion * acc * ADSRpage[i].step),
                ADSRpage[i].min,
                ADSRpage[i].max
              );
              if (i < TOTAL_ADSR) ADSRvalues[oscSelect][i] = uiValue;
              else if (i == 6 || i == 7) synthValue[0][(i == 6) ? 0 : 1] = uiValue;
              refreshValue(currentPage, i);
            }
            
            else {
              synthValue[currentPage][i] = constrain(
                synthValue[currentPage][i] + (direccion * acc * parametroPages[currentPage][i].step),
                parametroPages[currentPage][i].min,
                parametroPages[currentPage][i].max
              );
              refreshValue(currentPage, i);
            }

            subSteps[i] = 0;
          }
        }
      }

      lastEncState = current;
    }
  }
}

void applyPageDefaultsToggle(uint8_t page, const float defaults[PARAMS_PER_PAGE], float edited[PARAMS_PER_PAGE], bool &usingDefaults) {
  applyingPageDefaultsToggle = true;
  if (!usingDefaults) {
    for (uint8_t i = 0; i < PARAMS_PER_PAGE; i++) {
      edited[i] = synthValue[page][i];
      synthValue[page][i] = defaults[i];
      refreshValue(page, i);
      if (currentPage == page) drawValue(i);
    }
    usingDefaults = true;
    Serial.printf("Page %d defaults ON\n", page + 1);
  }
  else {
    for (uint8_t i = 0; i < PARAMS_PER_PAGE; i++) {
      synthValue[page][i] = edited[i];
      refreshValue(page, i);
      if (currentPage == page) drawValue(i);
    }
    usingDefaults = false;
    Serial.printf("Page %d defaults OFF\n", page + 1);
  }
  applyingPageDefaultsToggle = false;
}

void refreshValue(uint8_t page, uint8_t param){
  
  if(page < SOUND_PARAM_PAGES) {
    const float value = synthValue[page][param];
    uint8_t pageParam = page * PARAMS_PER_PAGE + param;
    switch (pageParam) {
      //pagina 1 CONFIG currentPage = 0
      case 0:  oscMix = value; break;
      case 1:  detuneAmount = value * 0.01f; break;
      case 2:  varPulse = value; 
        regeneratePulseTable(varPulse); 
        break;
      case 3:  masterGain = value; break;
      case 4:  velocityExponent = value; break;
      case 5:
        maxReleaseVoices = (uint8_t)constrain((int)roundf(value), 1, 6);
        synthValue[0][5] = (float)maxReleaseVoices;
        break;
      case 6:  { int selectedIndex = constrain((int)roundf(value), 0, TABLE_SIZE_COUNT - 1);
                uint16_t selectedSize = tableSizeFromIndex(selectedIndex);
                bool sizeChanged = selectedSize != tableSize;
                bool saved = !sizeChanged;
                if (sizeChanged) {
                  saved = saveTableSizeToNvs(selectedSize);
                  drawExtraValue();
                  Serial.printf("[WAV] Cambio TABLE_SIZE -> %u (%s)\n",
                                selectedSize,
                                saved ? "guardado, requiere reset HW" : "error NVS");
                }
                synthValue[0][6] = saved ? (float)selectedIndex : (float)tableSizeToIndex(tableSize);
                break;
              }
      case 7: { MemoryMode selectedMode = value >= 0.5f ? MEMORY_INTERNAL : MEMORY_PSRAM;
                bool modeChanged = selectedMode != memoryMode;
                if (modeChanged) {
                  bool saved = saveMemoryModeToNvs(selectedMode);
                  drawExtraValue();
                  Serial.printf("[MEM] Cambio modo memoria -> %s (%s)\n",
                                selectedMode == MEMORY_INTERNAL ? "RAM interna primero" : "AUTO (PSRAM primero)",
                                saved ? "guardado, requiere reset HW" : "error NVS");
                  
                }
                synthValue[0][7] = (memoryMode == MEMORY_INTERNAL) ? 1.0f : 0.0f;
                break;
              } 
          
      //pagina 2 LFO currentPage = 1
      case 8:  lfoWaveform = (LfoWaveform)roundf(value); break;
      case 9:  lfoRateHz = value; break;
      case 10: lfoDepth = value; break;
      case 11: lfoTarget = (LfoTarget)roundf(value); break;
      case 12: lfoAttackTime = value * 0.001f; break;
      case 13: cutoffControl = value; filterCutoffHz = cutoffControlToHz(cutoffControl); break;
      case 14:
        lfoPitchUpdateSamples = (uint8_t)constrain((int)roundf(value), 1, 32);
        synthValue[1][6] = (float)lfoPitchUpdateSamples;
        break;
      case 15: filterResonance = value; break;

      //pagina 3 GLIDE/MORPH currentPage = 2
      case 16: glideTime = value; break;
      case 17: morphEnabled = value >= 0.5f ? 1.0f : 0.0f; 
               Serial.printf("OSC A WaveForm start %s end %s\n", WAVE_NAMES[oscWaveCacheType[0]], WAVE_NAMES[oscWaveCacheEndType[0]]);
               Serial.printf("OSC B WaveForm start %s end %s\n", WAVE_NAMES[oscWaveCacheType[1]], WAVE_NAMES[oscWaveCacheEndType[1]]);
               break;
      case 18: oscWaveformEnd[0] = (Waveform)roundf(value);
               drawWaveAudioIcon(oscWaveformEnd[0], 255, 159, TFT_YELLOW); 
               leds.setColor(currentPage, RED);
               leds.show(); 
               break;
      case 19: oscWaveformEnd[1] = (Waveform)roundf(value);
               drawWaveAudioIcon(oscWaveformEnd[1], 255, 214, TFT_CYAN); 
               leds.setColor(currentPage, RED);
               leds.show();
               break;
      case 20: morphBase = value; break;
      case 21: morphMode = (MorphMode)roundf(value); break; 
      case 22: morphRateHz = value; break; 
      case 23: morphDepth = value; break;
      
      //pagina 4 FX CHORUS currentPage = 3
      case 24: modFxEnabled = value >= 0.5f ? 1.0f : 0.0f; break;
      case 25: modFxMode = (FxMode)roundf(value); break;
      case 26: modFxRateHz = value; break;
      case 27: modFxDepthMs = value; break;
      case 28: modFxBaseMs = value; break;
      case 29: modFxFeedback = value; break;
      case 30: modFxMix = value; break;
      case 31: modFxStereo = value * 0.01f; break;
  
      //pagina 5 CHORD  currentPage = 4
      case 32: chordAssistantEnabled = value >= 0.5f; break;
      case 33: chordType = (ChordType)constrain((int)roundf(value), 0, CHORD_PLAYED - 1); break;
      case 34:
        if (sequencerState != SEQ_STATE_OFF) sequencerSteps[sequencerEditStep].chordInversion = (uint8_t)constrain((int)roundf(value), 0, 2);
        else chordInversion = (uint8_t)constrain((int)roundf(value), 0, 2);
        break;
      case 35:
        if (sequencerState != SEQ_STATE_OFF) sequencerSteps[sequencerEditStep].chordOctaveShift = (int8_t)constrain((int)roundf(value), -1, 1);
        else chordOctaveShift = (int8_t)constrain((int)roundf(value), -1, 1);
        break;
      case 36:
        if (sequencerState != SEQ_STATE_OFF) sequencerSteps[sequencerEditStep].chordVelocityScale = constrain(value / 100.0f, 0.25f, 1.25f);
        else chordVelocityScale = constrain(value / 100.0f, 0.25f, 1.25f);
        break;
      case 37:
        if (sequencerState != SEQ_STATE_OFF) sequencerSteps[sequencerEditStep].chordSpread = constrain(value, 0.0f, 1.0f);
        else chordSpread = constrain(value, 0.0f, 1.0f);
        break;
      case 38:
        if (sequencerState != SEQ_STATE_OFF) sequencerSteps[sequencerEditStep].chordStrumMs = (uint8_t)constrain((int)roundf(value), 0, 60);
        else chordStrumMs = (uint8_t)constrain((int)roundf(value), 0, 60);
        break;
      case 39:
        if (sequencerState != SEQ_STATE_OFF) sequencerSteps[sequencerEditStep].chordDensity = (uint8_t)constrain((int)roundf(value), 2, 4);
        else chordDensity = (uint8_t)constrain((int)roundf(value), 2, 4);
        break;
      
      //pagina 6 SEQUENCER currentPage = 5
      case 40:
        sequencerState = (SequencerState)constrain((int)roundf(value), 0, SEQ_STATE_COUNT - 1);
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
        sequencerEditStep = (uint8_t)constrain((int)roundf(value) - 1, 0, stepsForSeq - 1);
        if (sequencerState != SEQ_STATE_OFF) syncSequencerScopedValues(sequencerEditStep);
        synthValue[5][2] = (float)sequencerSteps[sequencerEditStep].mode;
        synthValue[5][3] = (float)sequencerSteps[sequencerEditStep].root;
        synthValue[5][4] = (float)sequencerSteps[sequencerEditStep].chord;
        synthValue[5][5] = (float)sequencerSteps[sequencerEditStep].bars;
        synthValue[5][6] = (float)sequencerSteps[sequencerEditStep].velocity;
        synthValue[5][7] = (float)sequencerBpm;
        if (!suppressUiRefresh && page == 5) {
          for (uint8_t j = 2; j < 8; j++) drawValue(j);
        }
        break;
      case 42:
        sequencerSteps[sequencerEditStep].mode = (SequencerMode)constrain((int)roundf(value), 0, SEQ_MODE_COUNT - 1);
        break;
      case 43:
        sequencerSteps[sequencerEditStep].root = (uint8_t)constrain((int)roundf(value), 0, 127);
        break;
      case 44:
        sequencerSteps[sequencerEditStep].chord = (ChordType)constrain((int)roundf(value), 0, CHORD_TYPE_COUNT - 1);
        if (sequencerSteps[sequencerEditStep].chord != CHORD_PLAYED) sequencerSteps[sequencerEditStep].playedCount = 0;
        break;
      case 45:
        sequencerSteps[sequencerEditStep].bars = (uint8_t)constrain((int)roundf(value), 1, 8);
        break;
      case 46:
        sequencerSteps[sequencerEditStep].velocity = (uint8_t)constrain((int)roundf(value), 20, 120);
        break;
      case 47:
        sequencerBpm = (uint16_t)constrain((int)roundf(value), 60, 200);
        break;

      //pagina 7 ARPPEGIATOR currentPage = 6
      case 48: arpEnabled = value >= 0.5f; if (!arpEnabled) arpClearNotes(); break;
      case 49:
        if (sequencerState != SEQ_STATE_OFF) sequencerSteps[sequencerEditStep].arpRateHz = value;
        else arpRateHz = value;
        break;
      case 50:
        if (sequencerState != SEQ_STATE_OFF) sequencerSteps[sequencerEditStep].arpMode = (ArpMode)roundf(value);
        else arpMode = (ArpMode)roundf(value);
        break;
      case 51:
        if (sequencerState != SEQ_STATE_OFF) sequencerSteps[sequencerEditStep].arpOctaves = (int)roundf(value);
        else arpOctaves = (int)roundf(value);
        break;
      case 52:
        if (sequencerState != SEQ_STATE_OFF) sequencerSteps[sequencerEditStep].arpGate = value;
        else arpGate = value;
        break;
      case 53:
        arpHold = (ArpHold)constrain((int)roundf(value), (int)HOLD_ORDER, (int)HOLD_STACK);
        synthValue[6][5] = (float)arpHold;
        break;
      case 54:
        if (sequencerState != SEQ_STATE_OFF) sequencerSteps[sequencerEditStep].arpSwing = value;
        else arpSwing = value;
        break;
      case 55:
        if (sequencerState != SEQ_STATE_OFF) sequencerSteps[sequencerEditStep].arpPatternMask = (uint8_t)constrain((int)roundf(value), 1, 255);
        else arpPatternMask = (int)roundf(value);
        drawExtraValue();
        break;
    }
  }
  
  //pagina 8 FILES  currentPage = 7
  else if(page == FILE_PARAM_PAGE) {
    switch (param) {
      case 0: presetSelectFileByIndex(filePageParams[param].value); break;
      case 1:
        if (filePageParams[param].value == 1) {
          finalizePresetName();
          bool ok = presetLoadByName(presetEditName);
          Serial.printf("Load preset %s -> %s\n", presetEditName, ok ? "OK" : "FAIL");
          setPresetStatus(ok ? "LOAD OK" : "LOAD FAIL");
          setPresetActionFeedback(1, ok ? "OK" : "FAIL");
          filePageParams[param].value = 0;
        }
        break;

      case 2:
        if (filePageParams[param].value == 1) {
          finalizePresetName();
          bool ok = presetSaveByName(presetEditName);
          Serial.printf("Save preset %s -> %s\n", presetEditName, ok ? "OK" : "FAIL");
          setPresetStatus(ok ? "SAVE OK" : "SAVE FAIL");
          setPresetActionFeedback(2, ok ? "OK" : "FAIL");
          filePageParams[param].value = 0;
          refreshPresetFileList(true);
        }
        break;
      case 3:
        if (filePageParams[param].value == 1) {
          finalizePresetName();
          bool ok = presetDeleteByName(presetEditName);
          Serial.printf("Delete preset %s -> %s\n", presetEditName, ok ? "OK" : "FAIL");
          setPresetStatus(ok ? "DEL OK" : "DEL FAIL");
          setPresetActionFeedback(3, ok ? "OK" : "FAIL");
          filePageParams[param].value = 0;
          refreshPresetFileList(false);
        }
        break;
      case 4:
        if (filePageParams[param].value == 1) {
          memset(presetEditName, ' ', PN_LEN);
          presetEditName[PN_LEN] = '\0';
          setPresetStatus("NAME CLEAR");
          setPresetActionFeedback(4, "CLEAR");
          filePageParams[param].value = 0;
        }
        break;
      case 5:
        if (filePageParams[param].value == 1) {
          presetInsertSelectedChar(true);
          setPresetStatus("CHAR OK", 800);
          setPresetActionFeedback(5, "WRITE");
          filePageParams[param].value = 0;
        }
        break;
      case 6: break;
      case 7: break;
    }

  }

  //pagina 9 ADSR currentPage = 8
  else if(page == ADSR_PARAM_PAGE) { 
    if (param < TOTAL_ADSR) {
      float uiValue = constrain(ADSRvalues[oscSelect][param], ADSRpage[param].min, ADSRpage[param].max);
      if (param == ADSR_DELAY || param == ATTACK || param == DECAY || param == RELEASE) {
        ADSRvalues[oscSelect][param] = uiValue * 0.001f;
      }
      else if (param == ATTACK_LEVEL || param == SUSTAIN) {
        ADSRvalues[oscSelect][param] = uiValue * 0.01f;
      }
      else {
        ADSRvalues[oscSelect][param] = uiValue;
      }
      ADSRedited[oscSelect][param] = ADSRvalues[oscSelect][param];
      adsrUsingDefaults[oscSelect] = false;
      updateEnvelopeRates();
    }
    else if (param == 6 || param == 7) {
      uint8_t oscParam = (param == 6) ? 0 : 1;
      float uiValue = constrain(synthValue[0][oscParam], ADSRpage[param].min, ADSRpage[param].max);
      synthValue[0][oscParam] = uiValue * 0.01f;
      if (oscParam == 0) oscMix = synthValue[0][0];
      else detuneAmount = synthValue[0][1] * 0.01f;
    }
  }
  if (!applyingPageDefaultsToggle) {
    if (page == 1 && page1UsingDefaults) {
      for (uint8_t i = 0; i < PARAMS_PER_PAGE; i++) page1EditedValues[i] = synthValue[1][i];
      page1UsingDefaults = false;
      if (!suppressUiRefresh && currentPage == 1) drawExtraValue();
    }
    
    else if (page == 3 && page3UsingDefaults) {
      for (uint8_t i = 0; i < PARAMS_PER_PAGE; i++) page3EditedValues[i] = synthValue[3][i];
      page3UsingDefaults = false;
      if (!suppressUiRefresh && currentPage == 3) drawExtraValue();
    }
  }


  if (!suppressUiRefresh) {
    drawValue(param);
    if (page == FILE_PARAM_PAGE) {
      drawExtraValue();
      drawMainVisualization();
    }
    else if (page == ADSR_PARAM_PAGE) {
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
  updateEnvelopeRates();
  drawMainVisualization();
}

void setPage(uint8_t page){
  if(page >= PARAM_PAGES) return;
  if(page != currentPage){
    currentPage = page;
    if (sequencerState != SEQ_STATE_OFF && (currentPage == 4 || currentPage == 6)) {
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
      case 2:
        syncActiveWaveCache(0, oscWaveform[0], WAVE_START);
        syncActiveWaveCache(1, oscWaveform[1], WAVE_START);
        syncActiveWaveCache(0, oscWaveformEnd[0], WAVE_END);
        syncActiveWaveCache(1, oscWaveformEnd[1], WAVE_END);
        
        Serial.printf("OSC A WaveForm start %s end %s\n", WAVE_NAMES[oscWaveCacheType[0]], WAVE_NAMES[oscWaveCacheEndType[0]]);
        Serial.printf("OSC B WaveForm start %s end %s\n", WAVE_NAMES[oscWaveCacheType[1]], WAVE_NAMES[oscWaveCacheEndType[1]]);
        leds.setColor(currentPage, GREEN);
        leds.show();
      case 5:
        seqCopyStep(sequencerEditStep);
      break;
      case 7:
        presetInsertSelectedChar(true);
        setPresetStatus("CHAR OK", 800);
      case 8:
        syncActiveWaveCache(0, oscWaveform[oscSelect], WAVE_START);
        syncActiveWaveCache(1, oscWaveform[!oscSelect], WAVE_START);
        leds.setColor(currentPage, GREEN);
        leds.show();
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
      if (currentPage == FILE_PARAM_PAGE && i == 7) {
        presetInsertSelectedChar(true);
        setPresetStatus("CHAR OK", 800);
      }
      else if(i < PARAM_PAGES) {
        setPage(i);
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
        case 0:
          
        break;
        case 1:
          applyPageDefaultsToggle(1, PAGE1_DEFAULTS, page1EditedValues, page1UsingDefaults);
        break;
        case 2:
          oscWaveform[oscSelect] = (Waveform)constrain((int)oscWaveform[oscSelect] + enc, 0, (int)WAVE_COUNT);
          if(oscWaveform[oscSelect] == WAVE_COUNT && enc == 1) oscWaveform[oscSelect] = WAVE_SAW;
          if(oscWaveform[oscSelect] == WAVE_COUNT && enc == -1) oscWaveform[oscSelect] = WAVE_NOISE;
          drawWaveAudioIcon(oscWaveform[oscSelect], 185, oscSelect ? 214 : 159, oscSelect ? TFT_CYAN : TFT_YELLOW);
          leds.setColor(currentPage, RED);
          leds.show();
        break;
        case 3:
          applyPageDefaultsToggle(3, PAGE3_DEFAULTS, page3EditedValues, page3UsingDefaults);
        break;
        case 4:
        break;
        case 5:
          sequencerTransitionMode = (SeqTransitionMode)constrain((int)sequencerTransitionMode + enc, 0, SEQ_TRANS_COUNT - 1);
        break;
        case 6:
          
        break;
      
      }
      drawExtraValue();
    }
    else if(currentPage == ADSR_PARAM_PAGE){
      oscWaveform[oscSelect] = (Waveform)constrain((int)oscWaveform[oscSelect] + enc, 0, (int)WAVE_COUNT);
      if(oscWaveform[oscSelect] == WAVE_COUNT && enc == 1) oscWaveform[oscSelect] = WAVE_SAW;
      if(oscWaveform[oscSelect] == WAVE_COUNT && enc == -1) oscWaveform[oscSelect] = WAVE_NOISE;
      drawWaveAudioIcon(oscWaveform[oscSelect], 255, oscSelect ? 214 : 159, oscSelect ? TFT_CYAN : TFT_YELLOW); 
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
      setPage(ADSR_PARAM_PAGE);
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
      oscSelect = 1;
      if (currentPage == ADSR_PARAM_PAGE) drawMainVisualization();
      if (currentPage == 2) {
        leds.setColor(9, oscSelect ? CYAN : YELLOW);
        leds.show();
        drawExtraValue();
      }
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
      oscSelect = 0;
      if (currentPage == ADSR_PARAM_PAGE) drawMainVisualization();
      if (currentPage == 2) {
        leds.setColor(9, oscSelect ? CYAN : YELLOW);
        leds.show();
        drawExtraValue();
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
      if (currentPage == 6) drawValue(5);
      Serial.printf("Latch -> %s\n", latchEnabled ? "ON" : "OFF");
    }
  }
  botBlue_ant = botBlue;
}

void drawValue(uint8_t i){
  if (!isSequencerScopedControl(currentPage, i)) {
    tft.fillRect((i&3)*80, 28 + ((i>>2)*60), 80, 22, TFT_BLACK);
    return;
  }

  char buf[12];
  char charBuf[2] = {0};
  const char* textValue = nullptr;
  //SyntParam &p = parametroPages[currentPage][i];
  uint8_t pos = 40;
  float value = getParamValue(currentPage, i);
  switch(getParamType(currentPage, i)){
    
    case FLOAT:
      snprintf(buf, sizeof(buf), "%5.2f", value);
      textValue = buf;
      if(value < 100.00f) pos = 35;
      break;
    
    case INT:    
      if (currentPage == FILE_PARAM_PAGE && i == 0 && presetFileCount > 0) {
        snprintf(buf, sizeof(buf), "%d", ((int)roundf(value)) + 1);
      }
      else {
        snprintf(buf, sizeof(buf), "%d", (int)roundf(value));
      }
      textValue = buf;
      break;
    
    case F01:
      snprintf(buf, sizeof(buf), "%d", (int)roundf(value*100));
      textValue = buf;
      break;

    case ONOFF:
      if (currentPage == 0 && i == 7) {
        textValue = (bool)roundf(value) ? "RAM" : "PSRAM";
      } else {
        textValue = (bool)roundf(value) ? "ON" : "OFF";
      }
      break;
    
    case NAME:
      textValue = getNameValue((currentPage * PARAMS_PER_PAGE) + i, (int)roundf(value));
      break;

    case CHARSEL:
      charBuf[0] = PRESET_CHARS[(int)roundf(value)];
      textValue = charBuf;
      break;

    case NULO:
      return;
      break;

    case FFILE:
      if (currentPage == FILE_PARAM_PAGE &&
          (i == 1 || i == 2 || i == 3 || i == 4 || i == 5) &&
          presetActionParam == (int8_t)i &&
          (int32_t)(presetActionUntil - millis()) > 0) {
        textValue = presetActionLabel;
      }
      else {
        textValue = "GO";
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
  char buf[12] = "";
  const int x = 10;
  const int y = 130;
  tft.setFreeFont(NUM_TEXT);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_WHITE);
  tft.fillRect(x, y, 170, 20, TFT_BLACK);
  switch(currentPage){
    case 0: 
      
      break;
    case 1: 
      snprintf(buf, sizeof(buf), "%s", page1UsingDefaults ? "DEFAULT" : "EDIT"); 
      break;
    case 2: 
      snprintf(buf, sizeof(buf), "%s",  WAVE_LONG_NAMES[oscWaveform[oscSelect]]);
      break;
    case 3: 
      snprintf(buf, sizeof(buf), "%s", page3UsingDefaults ? "DEFAULT" : "EDIT"); 
      break;
    case 4:
      snprintf(buf, sizeof(buf), "%s I%d", CHORD_TYPE_NAMES[(int)chordType], chordInversion);
      break;
    case 5:
      snprintf(buf, sizeof(buf), "%s S%d B%d", SEQ_TRANSITION_NAMES[(int)sequencerTransitionMode], sequencerPlayStep + 1, sequencerPlayBar);
      break;
    case 6: 
      uint8ToBinaryStr(arpPatternMask, buf);
      break;
    case 7: {
      char nameBuf[PN_LEN + 1];
      strncpy(nameBuf, presetEditName, PN_LEN);
      nameBuf[PN_LEN] = '\0';
      int yy = 143;
      tft.fillRect(x, yy, 150, 20, TFT_BLACK);//148, 150, 22
      int pos = constrain(filePageParams[6].value, 0, PN_LEN - 1);
      char prefix[PN_LEN + 1];
      strncpy(prefix, nameBuf, pos);
      prefix[pos] = '\0';
      char selected[2] = {nameBuf[pos], '\0'};
      if (selected[0] == '\0') selected[0] = ' ';
      const char* suffix = (nameBuf[pos] != '\0') ? &nameBuf[pos + 1] : "";

      tft.setCursor(x, yy);
      tft.setTextColor(TFT_WHITE);
      tft.print(prefix);
      tft.setTextColor(TFT_RED);
      tft.print(selected);
      tft.setTextColor(TFT_WHITE);
      tft.print(suffix);
      break;
    }
    case 8:
      snprintf(buf, sizeof(buf), "%s: %s", OSC_NAME[oscSelect], WAVE_LONG_NAMES[oscWaveform[oscSelect]]);
      break;
  }
  if (currentPage != 7) {
    tft.drawString(buf, x, y);
  }
}

void drawLinesADSR(uint8_t osc){
  int x0 = 5;
  int y0 = 238;
  int width = 235;
  int height = 79;

  float L = ADSRvalues[osc][ADSR_DELAY];
  float A = ADSRvalues[osc][ATTACK];
  float AL = ADSRvalues[osc][ATTACK_LEVEL];
  float D = ADSRvalues[osc][DECAY];
  float S = ADSRvalues[osc][SUSTAIN];
  float R = ADSRvalues[osc][RELEASE];
  uint16_t color = (osc == 0) ? TFT_YELLOW : TFT_CYAN;

  float total = L + A + D + R + 0.2f;

  int xL = x0 + (L / total) * width;
  int xA = xL + (A / total) * width;
  int xD = xA + (D / total) * width;
  int xS = xD + (width / 4);
  int xR = xS + (R / total) * width;

  if (xR > (x0 + width - 1)) xR = x0 + width - 1;

  int yTop = y0 - height;
  int yAtk = y0 - (height * AL);
  int ySus = y0 - (height * S);

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
  const int w = (currentPage == 2) ? 160 : 220;
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

  uint8_t selected = (uint8_t)constrain(filePageParams[0].value, 0, presetFileCount - 1);
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
      tft.fillRect(tx - 2, ty - 1, colW - 4, rowH, TFT_DARKGREY);
      tft.setTextColor(TFT_GREEN);
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

void drawMainVisualization(){
  if (currentPage == ADSR_PARAM_PAGE) {
    for(byte i=0;i<8;i++) drawValue(i);
    drawADSR();
    drawWaveAudioIcon(oscWaveform[0], 255, 159, oscSelect ? TFT_DARKGREY : TFT_YELLOW);
    drawWaveAudioIcon(oscWaveform[1], 255, 214, oscSelect ? TFT_CYAN : TFT_DARKGREY);
    drawExtraValue();
    SimpleColor color = oscSelect ? CYAN : YELLOW;
    leds.setColor(9, color);
    leds.show();
  }
  else if (currentPage == FILE_PARAM_PAGE) {
    drawPresetFileList();
  }

  else if (currentPage == 2){  //Morphing
    drawAudioWaveform();
    drawWaveAudioIcon(oscWaveform[0], 185, 159, oscSelect ? TFT_DARKGREY : TFT_YELLOW);
    drawWaveAudioIcon(oscWaveform[1], 185, 214, oscSelect ? TFT_CYAN : TFT_DARKGREY);
    drawWaveAudioIcon(oscWaveformEnd[0], 255, 159, TFT_YELLOW);
    drawWaveAudioIcon(oscWaveformEnd[1], 255, 214, TFT_CYAN);
  }
  else {
    drawAudioWaveform();
    drawWaveAudioIcon(oscWaveform[0], 255, 159, TFT_YELLOW);
    drawWaveAudioIcon(oscWaveform[1], 255, 214, TFT_CYAN);
  }
}

void refreshAudioScope(){
  static uint32_t lastDrawMs = 0;
  static uint32_t lastSerial = 0;

  if (currentPage == ADSR_PARAM_PAGE || currentPage == FILE_PARAM_PAGE) return;
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
