void noteOn(uint8_t note, uint8_t velocity) {

  float baseFreq = 440.0f * pow(2.0f, (note - 69) / 12.0f);
  float velocityNorm = velocity / 127.0f;
  float velocityGain = powf(velocityNorm, velocityExponent);
  int voiceIndex = -1;

  limitReleaseVoices(maxReleaseVoices);
  for (int i = 0; i < NUM_VOICES; i++) {
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
}

void noteOff(uint8_t note) {

  for (int i = 0; i < NUM_VOICES; i++) {
    if (voices[i].active && voices[i].midiNote == note) {
      for (uint8_t osc = 0; osc < N_OSC; osc++) voices[i].envState[osc] = ENV_RELEASE;
    }
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
          noteOn(data1, data2); // Modo MELODÍA o notas simples
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
        noteOff(data1);
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
}

void handleMidiUsb() {
  #if SYNTH_USB_MIDI_ENABLED
    midiEventPacket_t packet = {0, 0, 0, 0};
    while (usbMidi.readPacket(&packet)) {
      midi_code_index_number_t cin = MIDI_EP_HEADER_CIN_GET(packet.header);
      if (cin == MIDI_CIN_NOTE_ON || cin == MIDI_CIN_NOTE_OFF) {
        processMidiMessage(packet.byte1, packet.byte2, packet.byte3);
      }
    }
  #endif
}

/*void handleMIDI() {

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

        uint8_t command = midiStatus & 0xF0;

        if (command == 0x90) {   // Note On
          if (midiData2 > 0) {
            bool wasPressed = midiKeysDown[midiData1];
            if (!wasPressed) {
              midiKeysDown[midiData1] = true;
              if (midiKeysPressedCount < 127) midiKeysPressedCount++;
            }
            captureSequencerStepFromMidi(midiData2);

            if (arpEnabled) {
              // 👉 Si HOLD está activo y esta es la primera tecla nueva
              ArpHold holdMode = effectiveArpHold();
              if (holdMode != HOLD_OFF && holdMode != HOLD_STACK && midiKeysPressedCount == 1) {
                arpClearNoteSoft();   // limpia acorde anterior pero no el index
              }
              arpAddNote(midiData1, midiData2);
              if (arpSamplesToNextStep == 0) arpSamplesToNextStep = 1;
            }
            else {
              if (latchEnabled && midiKeysPressedCount == 1) {
                releaseAllVoices();
                resetLfoAttackRequested = true;
                
              }
              if (chordAssistantEnabled) playChordFromRoot(midiData1, midiData2);
              else noteOn(midiData1, midiData2);
            }
          }
          else {
            if (midiKeysDown[midiData1]) {
              midiKeysDown[midiData1] = false;
              if (midiKeysPressedCount > 0) midiKeysPressedCount--;
            }

            if (arpEnabled) {
              if (effectiveArpHold() == HOLD_OFF) arpRemoveNote(midiData1);
            }
            else if (!latchEnabled) {
              if (chordAssistantEnabled) stopChordFromRoot(midiData1);
              else noteOff(midiData1);
            }
          }
        }

        else if (command == 0x80) {  // Note Off
          if (midiKeysDown[midiData1]) {
            midiKeysDown[midiData1] = false;
            if (midiKeysPressedCount > 0) midiKeysPressedCount--;
          }

          if (arpEnabled) {
            if (effectiveArpHold() == HOLD_OFF) arpRemoveNote(midiData1);
          }
          else if (!latchEnabled) {
            if (chordAssistantEnabled) stopChordFromRoot(midiData1);
            else noteOff(midiData1);
          }
        }
      }
    }
  }

  handleMidiUsb();
}*/
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



