const char* getParamName(Page page, uint8_t param) {
  if (page == PAGE_ADSR) return ADSRpage[param].name;
  else if(page == PAGE_ARP && customArpEditorState == CUSTOM_ARP_EDIT) return arpCustom[param].name;
  else return pageParam[page][param].name;
}

Type getParamType(Page page, uint8_t param) {
  if (page == PAGE_ADSR) return ADSRpage[param].type;
  //else if(page == PAGE_ARP && customArpEditorState == CUSTOM_ARP_EDIT) return arpCustom[param].type;
  else return pageParam[page][param].type;
}

int getParamValue(Page page, uint8_t param) {
  if (page == PAGE_ADSR) {
    if (param < TOTAL_ADSR) return ADSRvalues[oscSelect][param];
    return ADSRmixValues[param - TOTAL_ADSR];
  }
  else if(page == PAGE_ARP && customArpEditorState == CUSTOM_ARP_EDIT) return arpCustom[param].value;
  else return pageParam[page][param].value;
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
            else if (customArpEditorState == CUSTOM_ARP_EDIT && currentPage == PAGE_ARP) rango = 1;
            else rango = pageParam[currentPage][i].max - pageParam[currentPage][i].min;

            if(rango > 15){
              
              if (diff < 80) {
                  int32_t factor = 80 - diff; 
                 
                  int32_t saltoDinamico = (factor * factor * rango) >> 16;
                  acc += saltoDinamico;
              }

              // Límite de seguridad individual (15% máximo de su propio rango)
              int32_t accMaximo = (rango * 15) / 100; 
              acc = _min(acc, accMaximo);
            }
            
            int direccion = (subSteps[i] > 0) ? -1 : 1;

            currentParam = i;
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
      edited[i] = pageParam[page][i].value;
      pageParam[page][i].value = defaults[i];
      refreshValue(page, i, 0);
      if (currentPage == page) drawValue(i);
    }
    usingDefaults = true;
    Serial.printf("Page %d defaults ON\n", page + 1);
  }
  else {
    for (uint8_t i = 0; i < PARAMS_PER_PAGE; i++) {
      pageParam[page][i].value = edited[i];
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
  // NUEVO: Si estamos en editor de arpegio personalizado
  if (page == PAGE_ARP && customArpEditorState == CUSTOM_ARP_EDIT) {
    processCustomArpEditor(param, dirAcc);
    
    return;
  }

  if(page < MAIN_PARAM_PAGES) {
    pageParam[page][param].value = constrain(pageParam[page][param].value + dirAcc,
                pageParam[page][param].min,pageParam[page][param].max);
    const int value = pageParam[page][param].value;
    uint8_t parametroRaw = page * PARAMS_PER_PAGE + param;
    switch (parametroRaw) {
    //pagina 1 CONFIG currentPage = 0
      case 0:  velocityExponent = value * 0.1f; break;
      case 1:  sequencerBpm = (uint16_t)value; break;
      case 2:  varPulse = value * 0.01; 
        regeneratePulseTable(varPulse); 
        break;
      case 3:  masterGain = value * 0.01; break;
      case 4:  {uint8_t targetVoices = value;
                bool changed = targetVoices != numVoices;
                bool saved = !changed;
                if (changed) {
                  saved = saveNumVoicesToNvs(targetVoices);
                  Serial.printf("[NVS] Cambio Numero de Voces -> %u (%s)\n", targetVoices,
                                saved ? "guardado, requiere reset HW" : "error NVS");
                }
                pageParam[0][4].value = saved ? targetVoices : numVoices;
                if(changed && saved) bitSet(confFlagRed, parametroRaw);
                else bitClear(confFlagRed, parametroRaw);
                drawExtraValue();
                break;
              }
      case 5:  maxReleaseVoices = (uint8_t)value; break;
      case 6:  { int selectedIndex = value;
                uint16_t selectedSize = TABLE_SIZE_VALUES[selectedIndex];
                bool changed = selectedSize != tableSize;
                bool saved = !changed;
                if (changed) {
                  saved = saveTableSizeToNvs(selectedSize);
                  Serial.printf("[NVS] Cambio TABLE_SIZE -> %u (%s)\n",
                                selectedSize,
                                saved ? "guardado, requiere reset HW" : "error NVS");
                }
                pageParam[0][6].value = saved ? selectedIndex : tableSizeToIndex(tableSize);
                if(changed && saved) bitSet(confFlagRed, parametroRaw);
                else bitClear(confFlagRed, parametroRaw);
                drawExtraValue();
                break;
              }
      /*case 7: { MemoryMode selectedMode = (value > 0.5f) ? MEMORY_INTERNAL : MEMORY_PSRAM;
                bool changed = selectedMode != memoryMode;
                bool saved = !changed;
                if (changed) {
                  saved = saveMemoryModeToNvs(selectedMode);
                  
                  Serial.printf("[NVS] Cambio modo memoria -> %s (%s)\n",
                                selectedMode == MEMORY_INTERNAL ? "RAM interna primero" : "AUTO (PSRAM primero)",
                                saved ? "guardado, requiere reset HW" : "error NVS");
                }
                pageParam[0][7].value = (memoryMode == MEMORY_INTERNAL) ? 1 : 0;
                if(changed && saved) bitSet(confFlagRed,parametroRaw);
                else bitClear(confFlagRed, parametroRaw);
                drawExtraValue();
                break;
              } */
      case 7:  pitchBendRangeSemis = value; break;
          
    //pagina 2 LFO currentPage = 1
      case 8:  lfoWaveform = (LfoWaveform)value; break;
      case 9:  lfoRateHz = value * 0.1f; break;
      case 10: lfoDepth = value * 0.01f; break;
      case 11: valueSelectLFO = (LfoTarget)value;
               leds.setColor(currentPage, RED);
               leds.show();
               break;
      case 12: lfoAttackTime = value * 0.001f; break;
      case 13: cutoffControl = value * 0.1f; 
               filterCutoffHz = cutoffControlToHz(cutoffControl); break;
      case 14: lfoPitchUpdateSamples = (uint8_t)value; break;
      case 15: filterResonance = value * 0.01f; break;

    //pagina 3 GLIDE/MORPH currentPage = 2
      case 16: glideTime = value * 0.001f; break;
      case 17: morphEnabled = value * 1.0f;
               if (!suppressUiRefresh) {
                  color = morphEnabled ? TFT_YELLOW : TFT_DARKGREY;
                  drawWaveAudioIcon(oscWaveformEnd[0], 255, 175, color);
                  color = morphEnabled ? TFT_CYAN : TFT_DARKGREY;
                  drawWaveAudioIcon(oscWaveformEnd[1], 255, 217, color);
                  Serial.printf("OSC A WaveForm start %s end %s\n", WAVE_NAMES[oscWaveCacheType[0]], WAVE_LONG_NAMES[oscWaveCacheEndType[0]]);
                  Serial.printf("OSC B WaveForm start %s end %s\n", WAVE_NAMES[oscWaveCacheType[1]], WAVE_LONG_NAMES[oscWaveCacheEndType[1]]);
               }
               break;
      case 18: oscWaveformEnd[0] = (Waveform)value;
               if (!suppressUiRefresh) {
                 color = morphEnabled ? TFT_YELLOW : TFT_DARKGREY;
                 drawWaveAudioIcon(oscWaveformEnd[0], 255, 175, color); 
                 leds.setColor(currentPage, RED);
                 leds.show(); 
               }
               break;
      case 19: oscWaveformEnd[1] = (Waveform)value;
               if (!suppressUiRefresh) {
                 color = morphEnabled ? TFT_CYAN : TFT_DARKGREY;
                 drawWaveAudioIcon(oscWaveformEnd[1], 255, 217, color); 
                 leds.setColor(currentPage, RED);
                 leds.show(); 
               }
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
        pageParam[5][2].value = sequencerSteps[sequencerEditStep].mode;
        if (sequencerSteps[sequencerEditStep].mode == SEQ_MODE_MELODY){
          pageParam[5][3].value = sequencerSteps[sequencerEditStep].transpose;
          redrawParam(3,{"TRANS", sequencerSteps[sequencerEditStep].transpose, -24, 24, INT});
        } else {
          pageParam[5][3].value = sequencerSteps[sequencerEditStep].root;
          redrawParam(3,{"ROOT", sequencerSteps[sequencerEditStep].root, 24, 96, INT});
        }
        pageParam[5][4].value = sequencerSteps[sequencerEditStep].chord;Serial.printf("Chord %d\n",pageParam[5][4].value);
        pageParam[5][5].value = sequencerSteps[sequencerEditStep].bars;
        pageParam[5][6].value = sequencerSteps[sequencerEditStep].velocity;
        pageParam[5][7].value = selectedDurationIdx;
        if (!suppressUiRefresh && page == PAGE_SEQ) {
          for (uint8_t j = 2; j < 8; j++) drawValue(j);
        }
        drawMainVisualization();
        drawExtraValue();
        break;
      case 42: sequencerSteps[sequencerEditStep].mode = (SequencerMode)value;
              if(value == SEQ_MODE_MELODY) {
                drawMelody(sequencerEditStep);
                pageParam[5][3].value = sequencerSteps[sequencerEditStep].transpose;
                redrawParam(3,{"TRANS", sequencerSteps[sequencerEditStep].transpose, -24, 24, INT});
              } else {
                pageParam[5][3].value = sequencerSteps[sequencerEditStep].root;
                redrawParam(3,{"ROOT", sequencerSteps[sequencerEditStep].root, 24, 96, INT});
              }
              break;  
      case 43: if (sequencerSteps[sequencerEditStep].mode == SEQ_MODE_MELODY){
                sequencerSteps[sequencerEditStep].transpose = value;
              } else {
                sequencerSteps[sequencerEditStep].root = value;
              }
      case 44: sequencerSteps[sequencerEditStep].chord = (ChordType)value;
                if (sequencerSteps[sequencerEditStep].chord != CHORD_PLAYED) sequencerSteps[sequencerEditStep].playedCount = 0;
                break;
      case 45: sequencerSteps[sequencerEditStep].bars = value; break;
      case 46: if(sequencerState != SEQ_STATE_ON) sequencerSteps[sequencerEditStep].velocity = map(value,0,100,0,127); 
              else sequencerMainVelocity = constrain(value, 0, 100) * 0.01f;
              break;
      case 47: selectedDurationIdx = value;
                if (sequencerSteps[sequencerEditStep].melodyCount > 0) {
                  uint8_t lastNoteIdx = sequencerSteps[sequencerEditStep].melodyCount - 1;
                  sequencerSteps[sequencerEditStep].melodyDurations[lastNoteIdx] = DURATION_PRESETS[selectedDurationIdx];
                }
                break;

    //pagina 7 ARPPEGIATOR currentPage = 6
      case 48: if (sequencerState != SEQ_STATE_OFF) break;
                else{
                  arpEnabled = value >= 0.5f; 
                  if (!arpEnabled) arpClearNotes(); break;
                }
      case 49: if (sequencerState != SEQ_STATE_OFF) sequencerSteps[sequencerEditStep].arpRateHz = value * 0.1f;
                else {
                  arpRateHz = value  * 0.1f;
                }
                arpCustom[param].value = value;
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
                  pageParam[6][5].value = arpHold; break;
                }
      case 54: if (sequencerState != SEQ_STATE_OFF) sequencerSteps[sequencerEditStep].arpSwing = value * 0.01;
                else arpSwing = value * 0.01;
                break;
      case 55: if (sequencerState != SEQ_STATE_OFF) sequencerSteps[sequencerEditStep].arpPatternMask = (uint8_t)value;
                else arpPatternMask = value;
                drawExtraValue();
                break;
  
    //Pagina 8 FILE currentPage 7
      case 56: presetSelectFileByIndex(value); break;
      case 57:
        if (value == 1) {
          finalizePresetName();
          bool ok = presetLoadByName(presetEditName);
          Serial.printf("Load preset %s -> %s\n", presetEditName, ok ? "OK" : "FAIL");
          setFeedbackPreset(1, ok ? "OK" : "FAIL");
          pageParam[PAGE_FILE][param].value = 0;
        }
        break;

      case 58:
        if (pageParam[PAGE_FILE][param].value == 1) {
          finalizePresetName();
          bool ok = presetSaveByName(presetEditName);
          Serial.printf("Save preset %s -> %s\n", presetEditName, ok ? "OK" : "FAIL");
          setFeedbackPreset(2, ok ? "OK" : "FAIL");
          pageParam[PAGE_FILE][param].value = 0;
          refreshPresetFileList(true);
        }
        break;
      case 59:
        if (pageParam[PAGE_FILE][param].value == 1){
          setFeedbackPreset(3, "¿?");
          
        }
        if (pageParam[PAGE_FILE][param].value > 4) {
          finalizePresetName();
          bool ok = presetDeleteByName(presetEditName);
          Serial.printf("Delete preset %s -> %s\n", presetEditName, ok ? "OK" : "FAIL");
          setFeedbackPreset(3, ok ? "OK" : "FAIL");
          pageParam[PAGE_FILE][param].value = 0;
          refreshPresetFileList(false);
        }
        break;
      case 60:
        if (pageParam[PAGE_FILE][param].value == 1){
          setFeedbackPreset(4, "¿?");
          
        }
        if (pageParam[PAGE_FILE][param].value > 4) {
          memset(presetEditName, ' ', PN_LEN);
          presetEditName[PN_LEN] = '\0';
          pageParam[PAGE_FILE][6].value = 0;
          setFeedbackPreset(4, "CLEAR");
          pageParam[PAGE_FILE][param].value = 0;
          drawExtraValue();
        }
        break;
      case 61:
        if (pageParam[PAGE_FILE][param].value == 1) {
          presetInsertSelectedChar(true);
          setFeedbackPreset(5, "WRITE");
          pageParam[PAGE_FILE][param].value = 0;
        }
        else if (pageParam[PAGE_FILE][param].value == -1) {
          int pos = pageParam[PAGE_FILE][6].value;

          // 1. Desplazamos todos los caracteres que están a la derecha del cursor
          // una posición hacia la izquierda para borrar la letra actual.
          for (int i = pos; i < PN_LEN - 1; i++) {
            presetEditName[i] = presetEditName[i + 1];
          }
          // 2. Al desplazar todo, el último carácter queda duplicado, así que 
          // insertamos un espacio al final para mantener limpia la cadena.
          presetEditName[PN_LEN - 1] = ' ';
          // 3. Retrocedemos el cursor una posición (si no estamos ya en el límite)
          // para que, si sigues girando a la izquierda, actúe como un borrador continuo.
          if (pos > 0) {
            pos--;
            pageParam[PAGE_FILE][6].value = pos;
            drawValue(6);
          }
          
          // 4. Feedback visual
          setFeedbackPreset(5, "DEL");
          pageParam[PAGE_FILE][param].value = 0;
          drawExtraValue();
        }
        break;
      case 62: drawExtraValue(); break;
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
      for (uint8_t i = 0; i < PARAMS_PER_PAGE; i++) page1EditedValues[i] = pageParam[1][i].value;
      page1UsingDefaults = false;
      if (!suppressUiRefresh && currentPage == PAGE_LFO) drawExtraValue();
    }
    
    else if (page == PAGE_CHORUS && page3UsingDefaults) {
      for (uint8_t i = 0; i < PARAMS_PER_PAGE; i++) page3EditedValues[i] = pageParam[3][i].value;
      page3UsingDefaults = false;
      if (!suppressUiRefresh && currentPage == PAGE_CHORUS) drawExtraValue();
    }

  }

  if (!suppressUiRefresh) {
    drawValue(param);
    if (page == PAGE_ADSR || page == PAGE_FILE) {
      drawMainVisualization();
    }
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

void setPage(Page page){ // & Button Page
  if(page >= PARAM_PAGES) return;
  if (customArpEditorState == CUSTOM_ARP_EDIT) customArpEditorState == CUSTOM_ARP_IDLE;
  
  if(page != currentPage){ //Change page
    currentPage = page;
    valueSelectLFO = lfoTarget;// vuelve al valor no confirmado
    pageParam[PAGE_LFO][3].value = (LfoTarget)lfoTarget;
    if (sequencerState != SEQ_STATE_OFF && (currentPage == PAGE_CHORD || currentPage == PAGE_ARP)) {
      syncSequencerScopedValues(sequencerEditStep);
    }
    if(currentPage == PAGE_SEQ){
      if (sequencerSteps[sequencerEditStep].mode == SEQ_MODE_MELODY){
        pageParam[PAGE_SEQ][3] = {"TRANS", sequencerSteps[sequencerEditStep].transpose, -24, 24, INT};    
      }
      else {
        pageParam[PAGE_SEQ][3] = {"ROOT", sequencerSteps[sequencerEditStep].root, 24, 96, INT};
      }
    }
    drawUI();
    leds.setColor(lastPage, BLACK);
    leds.setColor(currentPage, GREEN);
    leds.show();
    lastPage = currentPage;
  }
  else{  // Extra function
    switch(page){
      case PAGE_LFO:
        lfoTarget = (LfoTarget)valueSelectLFO;
        pageParam[PAGE_LFO][3].value = (LfoTarget)lfoTarget;
        leds.setColor(currentPage, GREEN);
        leds.show();
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
        seqCopyNextStep(sequencerEditStep);
        
      break;
      case PAGE_FILE:
        presetInsertSelectedChar(true);
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

void processBotControl(){
  botAm.poll();  botonAmarillo();
  botAz.poll();  botonAzul();
  botEnc.poll(); botonEncoder();
  botTctl.poll();botonTactil();

}

void processControl(){
  enc = encoder.read();
  if (enc){ //Encoder
    if(currentPage < MAIN_PARAM_PAGES){
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
          drawWaveAudioIcon(oscWaveform[oscSelect], 185, oscSelect ? 217 : 175, oscSelect ? TFT_CYAN : TFT_YELLOW);
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
        
      }

      drawExtraValue();
    }
    else if(currentPage == PAGE_ADSR){
      oscWaveform[oscSelect] = (Waveform)constrain((int)oscWaveform[oscSelect] + enc, 0, (int)WAVE_COUNT);
      if(oscWaveform[oscSelect] == WAVE_COUNT && enc == 1) oscWaveform[oscSelect] = WAVE_SAW;
      if(oscWaveform[oscSelect] == WAVE_SAW && enc == -1) oscWaveform[oscSelect] = WAVE_NOISE;
      drawWaveAudioIcon(oscWaveform[oscSelect], 255, oscSelect ? 217 : 175, oscSelect ? TFT_CYAN : TFT_YELLOW); 
      drawExtraValue();
      leds.setColor(currentPage, RED);
      leds.show();
    }
  }
}

void botonTactil(){
  if (botTctl.switched()){
    if(botTctl.on()){
      latchEnabled = true;
    }
    else {
      latchEnabled = false;
      if (arpEnabled) arpReleaseLatchedNotes();
      else if (midiKeysPressedCount == 0) releaseAllVoices();
    }
    if (currentPage == PAGE_ARP) drawValue(5);
    Serial.printf("Latch -> %s\n", latchEnabled ? "ON" : "OFF");
  }
}

void botonEncoder(){
  if(botEnc.singleClick()){
    setPage(PAGE_ADSR);
  }
    
}

void botonAmarillo(){
  if(botAm.singleClick()){
    if(currentPage == PAGE_SEQ){
      uint8_t step = sequencerEditStep & 0x07;
      sequencerSteps[step].layerChord = !sequencerSteps[step].layerChord; 
      drawExtraValue();
     
    }
    else if(currentPage == PAGE_ARP && customArpEditorState == CUSTOM_ARP_IDLE){
      enterCustomArpEditor();
    }
    

    else if(currentPage == PAGE_ADSR || currentPage == PAGE_MORPH){
      if (oscSelect != 0) {
        oscSelect = 0;
        drawMainVisualization();
      }
      else toggleADSRDefaults(0);
    }

    else if(currentPage == PAGE_FILE){
      //modeFileSave += 1; if (modeFileSave == FILE_COUNT) modeFileSave = FILE_SOUND;
      modeFileSave = static_cast<ModeFileSave>((modeFileSave + 1) % FILE_COUNT);
      
      drawSqrBots(MODE_FILE_NAMES[modeFileSave], "MOD", "DEL");
      refreshPresetFileList(false);
    }

  }
  if(botAm.longPress()){
    if(currentPage == PAGE_ARP && customArpEditorState == CUSTOM_ARP_EDIT){
      resetCustomArpPattern(); // Resetea el patrón
    }

    else if(currentPage == PAGE_CONF){
      resetMidiMappings();
      resetPitchBend();
      resetAfterChannel();
    }
  }
}

void botonAzul(){
  if(botAz.singleClick()){
    if(midiLearnActive){
      midiLearnActive = false;
      leds.setColor(9, colorAnt);
      leds.show();
      return;
    }
    
    else if(currentPage == PAGE_ARP && customArpEditorState == CUSTOM_ARP_EDIT){
      exitCustomArpEditor();
    }
    else if(currentPage == PAGE_CONF){
      saveMidiMappingsToFS();
    }
    
    else if(currentPage == PAGE_ADSR || currentPage == PAGE_MORPH){
      if(oscSelect != 1){  
        oscSelect = 1;
        drawMainVisualization();
      }
      else toggleADSRDefaults(1);
    }

  }

  if(botAz.longPress()){
    if(currentPage == PAGE_SEQ){
      uint8_t step = sequencerEditStep & 0x07;
      seqDeleteStep(step);
    }
    else if(currentPage == PAGE_LFO || currentPage == PAGE_MORPH || currentPage == PAGE_CHORUS) {
      leds.setColor(9, MAGENTA);
      leds.show();
      triggerMidiLearn(currentPage, currentParam);
    }
    else if(currentPage == PAGE_FILE){
      bool ok = presetDeleteByName(presetEditName);
      Serial.printf("Delete preset %s -> %s\n", presetEditName, ok ? "OK" : "FAIL");
      refreshPresetFileList(false);
    }
  }
}

void drawValue(uint8_t i){
  if (!isSequencerScopedControl(currentPage, i)) {
    tft.fillRect((i&3)*80, 28 + ((i>>2)*60), 80, 22, TFT_BLACK);
    return;
  }

  char buf[12];
  char charBuf[2] = {0};
  const char* textValue = nullptr;
  
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
      if (flagTime) {
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
  if(currentPage == PAGE_CONF && bitRead(confFlagRed, i)) tft.setTextColor(TFT_RED);
  tft.fillRect((i&3)*80, 28 + ((i>>2)*60), 80, 22, TFT_BLACK);
  tft.drawString(textValue, 40 + ((i&3)*80), 30 + ((i>>2)*60));
}

void uint8ToBinaryStr(uint8_t num, char *buffer) {
  for (int i = 7; i >= 0; i--) {
    // Verificamos cada bit y guardamos '1' o '0'
    buffer[7 - i] = (num & (1 << i)) ? '1' : '0';
  }
  buffer[8] = '\0'; // Terminador de cadena
}

void drawExtraValue(){
  char buf[25] = "";
  const int x = 5;
  const int y = 130;
  tft.setFreeFont(NUM_TEXT);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_WHITE);
  tft.fillRect(x, y, 180, 20, TFT_BLACK);
  switch(currentPage){
    case PAGE_CONF: 
      snprintf(buf, sizeof(buf), " %s", confFlagRed ? "RESET HW" : "");
      break;
    case PAGE_LFO: 
      snprintf(buf, sizeof(buf), " %s", page1UsingDefaults ? "DEFAULT" : "EDIT"); 
      break;
    case PAGE_MORPH: 
      snprintf(buf, sizeof(buf), "%s",  WAVE_LONG_NAMES[oscWaveform[oscSelect]]);
      break;
    case PAGE_CHORUS: 
      snprintf(buf, sizeof(buf), " %s", page3UsingDefaults ? "DEFAULT" : "EDIT"); 
      break;
    case PAGE_CHORD:
      //snprintf(buf, sizeof(buf), "%s I%d", CHORD_TYPE_NAMES[(int)chordType], chordInversion);
      break;
    case PAGE_SEQ: {
      uint8_t currentStepIdx;
      if (sequencerState == SEQ_STATE_ON) {
        currentStepIdx = sequencerPlayStep & 0xF;
      } else {
        currentStepIdx = sequencerEditStep & 0xF;
      }
      snprintf(buf, sizeof(buf), "%s S%d B%d %s", SEQ_TRANSITION_NAMES[(int)sequencerTransitionMode], currentStepIdx + 1, 
      sequencerPlayBar, sequencerSteps[currentStepIdx].layerChord ? "CHD" : "");
      break;
    }
    case PAGE_ARP: 
      if(customArpEditorState != CUSTOM_ARP_EDIT) uint8ToBinaryStr(arpPatternMask, buf);
      else return;
      break;
    case PAGE_FILE: {
      char nameBuf[PN_LEN + 1];
      strncpy(nameBuf, presetEditName, PN_LEN);
      nameBuf[PN_LEN] = '\0';
      int pos = pageParam[PAGE_FILE][6].value;
      char prefix[PN_LEN + 1];
      strncpy(prefix, nameBuf, pos);
      prefix[pos] = '\0';

      char selected[2] = {nameBuf[pos], '\0'};
      if (selected[0] == '\0' || selected[0] == ' ') selected[0] = '_';
      
      const char* suffix = (nameBuf[pos] != '\0') ? &nameBuf[pos + 1] : "";

      tft.setCursor(x, y + 18);
      tft.setTextColor(TFT_WHITE);
      tft.print(prefix);
      tft.setTextColor(TFT_RED);
      tft.print(selected);
      tft.setTextColor(TFT_WHITE);
      tft.print(suffix);
      return;
      break;
    }
    case PAGE_ADSR:
      snprintf(buf, sizeof(buf), "%s: %s", OSC_NAME[oscSelect], WAVE_LONG_NAMES[oscWaveform[oscSelect]]);
      break;
  }
  
  tft.drawString(buf, x, y);
  Serial.println(buf);
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
  const int right = x + w - 3;
  const int top = y + 4;//165
  const int bottom = y + h - 6;
  const int midY = y + h / 2;
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
  const int w = 250;
  const int h = 80;
  const uint8_t cols = 2;
  const uint8_t rows = 5;
  const uint8_t pageSize = cols * rows;
  const int colW = w / cols;
  const int rowH = 13;

  tft.fillRect(x, y, w, h, TFT_BLACK);
  tft.drawRect(x-1, y-1, w+2, h+2, TFT_DARKGREY);
  tft.setFreeFont(FILE_TEXT);
  tft.setTextDatum(TL_DATUM);

  if (presetFileCount == 0) {
    tft.setTextColor(TFT_DARKGREY);
    tft.drawString("NO FILES", x + 8, y + 32);
    return;
  }

  uint8_t selected = (uint8_t)constrain(pageParam[PAGE_FILE][0].value, 0, presetFileCount - 1);
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

void drawMelody(uint8_t step){
  // 1. Definir el área de dibujo en drawMainVisualization
  const int x = 5;
  const int y = 159;
  const int w = 310; // Ancho total de la visualización
  const int h = 80;
  
  // Limpiar el área y dibujar el marco
  tft.fillRect(x, y, w, h, TFT_BLACK);
  tft.drawRect(x - 1, y - 1, w + 2, h + 2, TFT_DARKGREY);
  
  SequencerStep &seqStep = sequencerSteps[step];
  
  // Si no hay notas, mostramos un mensaje
  if (seqStep.melodyCount == 0) {
     tft.setFreeFont(LAB_TEXT);
     tft.setTextDatum(MC_DATUM);
     tft.setTextColor(TFT_DARKGREY);
     tft.drawString("EMPTY STEP", x + w / 2, y + h / 2);
     return;
  }

  // 2. Escanear notas para calcular el rango dinámico del eje Y
  uint8_t minNote = 127;
  uint8_t maxNote = 0;
  
  for(uint8_t i = 0; i < seqStep.melodyCount; i++) {
    uint8_t n = seqStep.melodyNotes[i];
    if (seqStep.melodyVelocities[i] > 0 && n != 255) {
      if (n < minNote) minNote = n;
      if (n > maxNote) maxNote = n;
    }
  }
  
  // Si todas las notas son iguales, damos un margen artificial para centrarla
  if (minNote == maxNote) {
    maxNote = minNote + 12; // Octava superior
    minNote = (minNote > 12) ? minNote - 12 : 0; // Octava inferior
  }

  // 3. Variables para el cálculo del compás (4/4 = 4.0 beats)
  float totalBeats = 4.0f; 
  float currentBeat = 0.0f;
  
  // 4. Iterar y dibujar cada nota
  for(uint8_t i = 0; i < seqStep.melodyCount; i++) {
    float dur = seqStep.melodyDurations[i];
    
    // Si ya completamos el compás, dejamos de dibujar
    if (currentBeat >= totalBeats) break; 
    
    // Si la nota excede el final del compás, la recortamos visualmente
    float drawDur = dur;
    if (currentBeat + drawDur > totalBeats) {
      drawDur = totalBeats - currentBeat;
    }
    
    // Calcular posiciones X y anchos
    int noteX = x + (int)((currentBeat / totalBeats) * w);
    // CALCULO PRECISO: Punto de inicio (x1) y punto final (x2) en píxeles
    int x1 = x + (int)roundf((currentBeat / totalBeats) * (float)w);
    int x2 = x + (int)roundf(((currentBeat + drawDur) / totalBeats) * (float)w);
    
    int noteW = x2 - x1;
    
    // Dejar 1px de separación entre notas adyacentes si hay espacio suficiente
    int drawW = (noteW > 3) ? noteW - 1 : noteW;
    if (drawW < 1) drawW = 1; // Garantizar al menos 1px de ancho visible
    
    uint8_t note = seqStep.melodyNotes[i];
    bool isRest = (seqStep.melodyVelocities[i] == 0 || note == 255);
    
    // Calcular altura y posición Y
    int rectH = 5;
    
    if (isRest) {
      // Dibujar silencios como una simple línea horizontal oscura en el centro
      int restY = y + (h / 2);
      tft.drawFastHLine(noteX, restY, drawW, TFT_DARKGREY);
    } else {
      // Calcular la posición Y interpolando entre la nota mínima y máxima
      int yTop = y + 5; 
      int yBottom = y + h - rectH - 5; 
      
      float rangoNotas = (float)(maxNote - minNote);
      float offsetNota = (float)(note - minNote);
      
      // Se resta de yBottom para invertir el eje: notas agudas (alto número MIDI) van arriba
      int rectY = yBottom - (int)((offsetNota / rangoNotas) * (yBottom - yTop));
      
      // Dibujar la nota
      tft.fillRect(noteX, rectY, drawW, rectH, TFT_GREEN);
    }
    
    // Avanzar el cursor de tiempo
    currentBeat += dur;
  }
  
  // 5. Dibujar líneas verticales grises para marcar los 4 tiempos (beats)
  for (int b = 1; b < 4; b++)  tft.drawFastVLine(x + (b * w / 4), y, h, TFT_DARKGREY);

}

void drawArp(uint8_t step){
  tft.setFreeFont(NUM_TEXT);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_GREEN);
  char buf[20] = "";
  snprintf(buf, sizeof(buf), "ARP: %s",  ARP_MODE_NAMES[sequencerSteps[step].arpMode]);
  tft.drawString(buf, 20, 190);
}

void drawChord(uint8_t step){
  tft.setFreeFont(NUM_TEXT);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_GREEN);
  char buf[20] = "";
  snprintf(buf, sizeof(buf), "CHORD: %s",  CHORD_TYPE_NAMES[sequencerSteps[step].chord]);
  tft.drawString(buf, 20, 190);
}

void drawSqrBots(const char* txt, const char* am, const char* az, int y){
  int x1 = 241;
  int x2 = 281;
  int w = 35;
  int h = 24;

  tft.fillRect(x1 - ((currentPage == PAGE_ARP) ? 0 : 46) ,y,(w * 2) + 5, h, TFT_BLACK);
  
  tft.setFreeFont(FILE_TEXT);
  tft.setTextDatum(TC_DATUM);

  if(txt != ""){
    tft.setTextColor(TFT_GREEN);
    tft.drawString(txt, x1 - 23, y + 7);
  }
  if(am != ""){
    tft.setTextColor(TFT_YELLOW);
    tft.drawRect(x1,y,w,h, TFT_YELLOW);
    tft.drawString(am, x1 + 18, y + 7);
  }
  if(az != ""){
    tft.setTextColor(TFT_CYAN);
    tft.drawRect(x2,y,w,h, TFT_CYAN);
    tft.drawString(az, x2 + 18, y + 7);
  }
  if(currentPage == PAGE_SEQ){
    tft.drawRect(x1 - 40,y,w,h, TFT_GREEN);
  }

} 

void  drawMainVisualization(){
  leds.setColor(9, BLACK);
  colorAnt = BLACK;
  SimpleColor color = oscSelect ? CYAN : YELLOW;
  switch (currentPage){
    case PAGE_ADSR:
      for(byte i=0;i<8;i++) drawValue(i);
      drawADSR();
      drawWaveAudioIcon(oscWaveform[0], 255, 175, oscSelect ? TFT_DARKGREY : TFT_YELLOW);
      drawWaveAudioIcon(oscWaveform[1], 255, 217, oscSelect ? TFT_CYAN : TFT_DARKGREY);
      drawExtraValue();
      drawSqrBots("RSET", "A","B");
      leds.setColor(9, color);
      colorAnt = color;

      break;

    case PAGE_FILE:
      drawSqrBots(MODE_FILE_NAMES[modeFileSave], "MOD", "DEL");
      drawPresetFileList();
      break;
    
    case PAGE_SEQ: {
      drawSqrBots("CPY", "CHD","DEL");    
      uint8_t currentStepIdx;
      if (sequencerState == SEQ_STATE_ON) {
        currentStepIdx = sequencerPlayStep & 0xF;
      } else {
        currentStepIdx = sequencerEditStep & 0xF;
      }
      if(sequencerSteps[currentStepIdx].mode == SEQ_MODE_MELODY) drawMelody(currentStepIdx);
      else if(sequencerSteps[currentStepIdx].mode == SEQ_MODE_ARP) drawArp(currentStepIdx);
      else drawChord(currentStepIdx);
      break;
    }

    case PAGE_MORPH:
      //drawAudioWaveform();
      drawWaveAudioIcon(oscWaveform[0], 185, 175, oscSelect ? TFT_DARKGREY : TFT_YELLOW);
      drawWaveAudioIcon(oscWaveform[1], 185, 217, oscSelect ? TFT_CYAN : TFT_DARKGREY);
      drawWaveAudioIcon(oscWaveformEnd[0], 255, 175, morphEnabled ? TFT_YELLOW : TFT_DARKGREY);
      drawWaveAudioIcon(oscWaveformEnd[1], 255, 217, morphEnabled ? TFT_CYAN : TFT_DARKGREY);
      drawExtraValue();
      drawSqrBots("INIT", "A","B");
      leds.setColor(9, color);
      colorAnt = color;
      break;

    case PAGE_ARP:
      drawSqrBots("MOD","CST","");
      break;

    case PAGE_CONF:
      drawSqrBots("MAPS", "CLR","SAV");
      break;

    default:
      drawWaveAudioIcon(oscWaveform[0], 255, 175, TFT_YELLOW);
      drawWaveAudioIcon(oscWaveform[1], 255, 217, TFT_CYAN);
      drawExtraValue();
      break;
  }
  leds.show();
}

void refreshAudioScope(){
  static uint32_t lastDrawMs = 0;
  static uint32_t lastSerial = 0;

  if (millis() - lastDrawMs < 80) return;
  if (audioScopeSerial == lastSerial) return;

  lastDrawMs = millis();
  lastSerial = audioScopeSerial;
  drawAudioWaveform();
}

void redrawParam(uint8_t p, const Param& param){
  tft.setFreeFont(LAB_TEXT);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(TFT_GREEN);
  tft.fillRect((p&3)*80, 5 + ((p>>2)*60), 80, 10, TFT_BLACK);
  pageParam[currentPage][p] = param;
  tft.drawString(getParamName(currentPage, p), 40 + ((p&3)*80), 5 + ((p>>2)*60));
  drawValue(p);
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
  iconSprite.fillSprite(TFT_BLACK);
  
  int centroY = ICON_H / 2;
  int ultimoX = 0;
  int ultimoY = centroY;
  
  // B. Dibujamos la geometría dentro del Sprite leyendo la caché ultrarrápida
  for (int x = 1; x < ICON_W - 1; x++) {
    int yPixel = cacheGraficaOndas[idWave][x];
    iconSprite.drawLine(ultimoX, ultimoY, x, yPixel, color);
    ultimoX = x;
    ultimoY = yPixel;
  }
  iconSprite.drawLine(ultimoX, ultimoY, ICON_W - 1, centroY, color);

  // C. Estampamos el Sprite en la pantalla ST7789. 
  // El Sprite se libera inmediatamente para poder ser reutilizado.
  iconSprite.pushSprite(posX, posY);
  // texto
  tft.fillRect(posX-1,posY-18,62,17,TFT_BLACK);
  tft.setFreeFont(LAB_TEXT);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(color);
  tft.drawString(WAVE_NAMES[idWave], posX + 30, posY -18); 
}

void drawUI(){
  if (customArpEditorState == CUSTOM_ARP_EDIT && currentPage == PAGE_ARP) {
    drawCustomArpEditor();
    return;
  }

  drawLabels(); //borra la pantalla
  for(byte i=0;i<8;i++){
    drawValue(i);
  }
  drawExtraValue(); 
  drawMainVisualization();
}
