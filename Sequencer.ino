bool noteInList(const uint8_t* notes, uint8_t count, uint8_t note) {
  for (uint8_t i = 0; i < count; i++) {
    if (notes[i] == note) return true;
  }
  return false;
}

uint8_t collectMidiHeldNotes(uint8_t* outNotes, uint8_t maxNotes) {
  uint8_t count = 0;
  for (uint8_t n = 0; n < 128 && count < maxNotes; n++) {
    if (midiKeysDown[n]) outNotes[count++] = n;
  }
  return count;
}

void clearCurrentStepMelody() {
  uint8_t step = sequencerEditStep & 0xF;
  sequencerSteps[step].melodyCount = 0;
  lastMelodyEditStep = 255; // Fuerza el reinicio
}

void processSequencerArpPreview() {
  if (sequencerState != SEQ_STATE_REC) return;
  
  uint8_t stepIdx = sequencerEditStep & 0xF;
  SequencerStep &seqStep = sequencerSteps[stepIdx];

  // Solo actuamos si el paso que estamos editando es de tipo ARP
  if (seqStep.mode != SEQ_MODE_ARP) return;

  uint32_t now = millis();

  // 1. Apagar nota activa si venció su tiempo de Gate
  if (sequencerArpNoteOn && (int32_t)(now - sequencerArpOffMs) >= 0) {
    noteOff(sequencerActiveArpNote);
    sequencerArpNoteOn = false;
  }

  // 2. Si no hay teclas pulsadas en el teclado, silenciamos y reseteamos el índice
  if (midiKeysPressedCount == 0) {
    if (sequencerArpNoteOn) {
      noteOff(sequencerActiveArpNote);
      sequencerArpNoteOn = false;
    }
    sequencerArpIndex = 0;
    return;
  }

  // 3. Calcular los tiempos de tick según la velocidad del paso (arpRateHz)
  uint32_t beatMs = 60000UL / max((uint16_t)60, sequencerBpm);
  if (beatMs < 80) beatMs = 80;

  uint32_t arpTickMs = beatMs;
  float stepArpRate = seqStep.arpRateHz;
  if (stepArpRate > 0.01f) {
    arpTickMs = (uint32_t)(1000.0f / stepArpRate);
    if (arpTickMs < 40) arpTickMs = 40;
  }

  // 4. Disparar la siguiente nota del arpegio
  if ((int32_t)(now - sequencerArpNextMs) >= 0) {
    uint8_t stepNotes[SEQUENCER_MAX_PLAYED_NOTES] = {0};
    uint8_t count = buildSequencerStepNotes(stepIdx, stepNotes, SEQUENCER_MAX_PLAYED_NOTES);
    
    if (count > 0) {
      uint8_t expandedCount = count * seqStep.arpOctaves;
      uint8_t arpTone = sequencerArpToneFromStep(
        sequencerArpIndex,
        expandedCount,
        seqStep.arpMode,
        seqStep.arpPatternMask
      );
      
      uint8_t toneInStep = arpTone % count;
      uint8_t octave = arpTone / count;
      int note = (int)stepNotes[toneInStep] + ((int)octave * 12);
      note = constrain(note, 0, 127);

      if (sequencerArpNoteOn) {
        noteOff(sequencerActiveArpNote);
        sequencerArpNoteOn = false;
      }

      uint8_t velocity = (uint8_t)seqStep.velocity * sequencerMainVelocity;
      sequencerActiveArpNote = (uint8_t)note;
      noteOn(sequencerActiveArpNote, velocity);
      sequencerArpNoteOn = true;

      sequencerArpOffMs = now + (uint32_t)(arpTickMs * seqStep.arpGate);
      uint32_t swingOffset = (sequencerArpIndex & 0x01) ? (uint32_t)(arpTickMs * seqStep.arpSwing) : 0;
      sequencerArpNextMs = now + arpTickMs + swingOffset;
      sequencerArpIndex++;
    }
  }
}

void processSequencer() {
  // Ejecutar preescucha de Arp si estamos grabando
  if (sequencerState == SEQ_STATE_REC) {
    processSequencerArpPreview();
    return;
  }

  if (sequencerState != SEQ_STATE_ON || arpEnabled) return;
  uint32_t now = millis();
  uint32_t beatMs = 60000UL / max((uint16_t)60, sequencerBpm);
  if (beatMs < 80) beatMs = 80;

  if (sequencerArpNoteOn && (int32_t)(now - sequencerArpOffMs) >= 0) {
    noteOff(sequencerActiveArpNote);
    sequencerArpNoteOn = false;
  }
  if (sequencerMelodyNoteOn && (int32_t)(now - sequencerMelodyOffMs) >= 0) {
    noteOff(sequencerActiveMelodyNote);
    sequencerMelodyNoteOn = false;
  }

  if (sequencerGateActive && (int32_t)(now - sequencerStepEndMs) >= 0) {
    bool keepForLegatoTransition =
      (sequencerActiveMode == SEQ_MODE_CHORD && sequencerTransitionMode == SEQ_TRANS_LEGATO);
    if (!keepForLegatoTransition) {
      sequencerStopActiveNote();
    }
    sequencerPlayStep = (sequencerPlayStep + 1) & 0xF;
    sequencerPlayBar = 1;
    sequencerNextStepMs = now;
  }

  if (sequencerGateActive) {
    uint8_t stepIdx = sequencerPlayStep & 0xF;
    SequencerStep &seqStep = sequencerSteps[stepIdx];
    uint32_t barMs = 4UL * beatMs;
    uint8_t maxBars = max((uint8_t)1, seqStep.bars);
    uint32_t elapsed = now - sequencerStepStartMs;
    uint8_t bar = (uint8_t)(elapsed / max((uint32_t)1, barMs)) + 1;
    sequencerPlayBar = constrain(bar, (uint8_t)1, maxBars);
  }

  if (sequencerGateActive && sequencerActiveMode == SEQ_MODE_MELODY) {
    uint8_t stepIdx = sequencerPlayStep & 0xF;
    SequencerStep &seqStep = sequencerSteps[stepIdx];

    if (seqStep.melodyCount > 0) {
      
      // NUEVO: Si la melodía llegó a la última nota pero el paso sigue activo (quedan Bars por reproducir),
      // reiniciamos el índice a 0 para que la melodía vuelva a sonar desde el principio en el nuevo compás.
      if (sequencerMelodyIndex >= seqStep.melodyCount) {
        sequencerMelodyIndex = 0;
      }

      if ((int32_t)(now - sequencerMelodyNextMs) >= 0 && (int32_t)(sequencerStepEndMs - now) > 0) {
        
        // 1. Apagar nota previa si estaba encendida
        if (sequencerMelodyNoteOn) {
          noteOff(sequencerActiveMelodyNote);
          sequencerMelodyNoteOn = false;
        }

        // 2. Obtener nota, velocidad y duración actuales
        uint8_t note = seqStep.melodyNotes[sequencerMelodyIndex];
        uint8_t vel  = (uint8_t)seqStep.melodyVelocities[sequencerMelodyIndex] * sequencerMainVelocity;
        float durFactor = seqStep.melodyDurations[sequencerMelodyIndex];

        // 3. Encender nota
        if (note != MELODY_NOTE_REST && note < 128) {
          int transposed = (int)note + seqStep.transpose;
          sequencerActiveMelodyNote = (uint8_t)constrain(transposed, 0, 127);
          noteOn(sequencerActiveMelodyNote, vel);
          sequencerMelodyNoteOn = true;
        } else {
          sequencerMelodyNoteOn = false;
        }

        // 4. Calcular tiempos de apagado y de la siguiente nota
        uint32_t noteDurationMs = (uint32_t)(beatMs * durFactor);
        sequencerMelodyOffMs = now + (uint32_t)(noteDurationMs * seqStep.arpGate); // 90% gate
        sequencerMelodyNextMs += noteDurationMs;

        sequencerMelodyIndex++;
      }
    }
  }

  if (sequencerGateActive && sequencerActiveMode == SEQ_MODE_ARP) {
    uint8_t stepIdx = sequencerPlayStep & 0xF;
    SequencerStep &seqStep = sequencerSteps[stepIdx];
    uint8_t velocity = (uint8_t)seqStep.velocity * sequencerMainVelocity;
    uint32_t arpTickMs = beatMs;
    float stepArpRate = seqStep.arpRateHz;
    if (stepArpRate > 0.01f) {
      arpTickMs = (uint32_t)(1000.0f / stepArpRate);
      if (arpTickMs < 40) arpTickMs = 40;
    }
    while ((int32_t)(now - sequencerArpNextMs) >= 0 &&
           (int32_t)(sequencerStepEndMs - now) > 0) {
      uint8_t stepNotes[SEQUENCER_MAX_PLAYED_NOTES] = {0};
      uint8_t count = buildSequencerStepNotes(stepIdx, stepNotes, SEQUENCER_MAX_PLAYED_NOTES);
      if (count == 0) break;
      uint8_t expandedCount = count * seqStep.arpOctaves;
      uint8_t arpTone = sequencerArpToneFromStep(
        sequencerArpIndex,
        expandedCount,
        seqStep.arpMode,
        seqStep.arpPatternMask
      );
      uint8_t toneInStep = arpTone % count;
      uint8_t octave = arpTone / count;
      int note = (int)stepNotes[toneInStep] + ((int)octave * 12);
      note = constrain(note, 0, 127);
      if (sequencerArpNoteOn) {
        noteOff(sequencerActiveArpNote);
        sequencerArpNoteOn = false;
      }
      sequencerActiveArpNote = (uint8_t)note;
      noteOn(sequencerActiveArpNote, velocity);
      sequencerArpNoteOn = true;
      sequencerArpOffMs = now + (uint32_t)(arpTickMs * seqStep.arpGate);
      uint32_t swingOffset = (sequencerArpIndex & 0x01) ? (uint32_t)(arpTickMs * seqStep.arpSwing) : 0;
      sequencerArpNextMs += arpTickMs + swingOffset;
      sequencerArpIndex++;
      now = millis();
    }
  }

  if (currentPage == PAGE_SEQ &&
      (sequencerUiStepShown != sequencerPlayStep || sequencerUiBarShown != sequencerPlayBar)) {
    drawExtraValue();
    sequencerUiStepShown = sequencerPlayStep;
    sequencerUiBarShown = sequencerPlayBar;
    uint8_t stepIdx = sequencerPlayStep & 0xF;
    if (sequencerSteps[stepIdx].mode == SEQ_MODE_MELODY) {
      drawMelody(stepIdx);
    }
    else if (sequencerSteps[stepIdx].mode == SEQ_MODE_ARP) {
      drawArp(stepIdx);
    }
    else if (sequencerSteps[stepIdx].mode == SEQ_MODE_CHORD) {
      drawChord(stepIdx);
    }
  }
  

  // =========================================================================
  // 1. INICIO DE UN NUEVO PASO (Se dispara cuando vence sequencerNextStepMs)
  // =========================================================================
  if ((int32_t)(now - sequencerNextStepMs) < 0) return;

  uint8_t stepIdx = sequencerPlayStep & 0xF;
  SequencerStep &seqStep = sequencerSteps[stepIdx];

  // Si encontramos la marca de FIN de secuencia
  if (seqStep.chord == CHORD_END) {
    sequencerStopActiveNote();
    sequencerPlayStep = 0;
    sequencerPlayBar = 1;
    sequencerNextStepMs = now + 1;
    return;
  }

  // Actualizamos tiempos del nuevo paso
  uint32_t stepDurMs = (uint32_t)max((uint8_t)1, seqStep.bars) * 4UL * beatMs;
  sequencerStepStartMs = now;
  sequencerPlayBar = 1;
  sequencerStepEndMs = now + stepDurMs;
  sequencerNextStepMs = sequencerStepEndMs;

  SequencerMode mode = seqStep.mode;
  uint8_t root = seqStep.root;
  uint8_t velocity = (uint8_t)seqStep.velocity * sequencerMainVelocity;

  // Si el paso es un SILENCIO total
  if (seqStep.chord == CHORD_REST) {
    sequencerStopActiveNote();
    sequencerActiveMode = SEQ_MODE_CHORD;
    sequencerActiveRoot = root;
    sequencerGateActive = true;
  }
  else {
    // Manejo de transiciones Legato
    if (sequencerTransitionMode == SEQ_TRANS_LEGATO &&
        sequencerActiveMode == SEQ_MODE_CHORD &&
        mode != SEQ_MODE_CHORD) {
      stopChordFromRoot(sequencerActiveRoot);
    }

    // A) ¿DEBE SONAR UN ACORDE DE FONDO?
    if (mode == SEQ_MODE_CHORD || seqStep.layerChord) {
      if (sequencerTransitionMode == SEQ_TRANS_RETRIG) {
        stopChordFromRoot(sequencerActiveRoot);
        sequencerActiveRoot = root;
        playSequencerChordNotes(stepIdx, velocity);
      } else {
        playSequencerChordNotes(stepIdx, velocity);
        sequencerActiveRoot = root;
      }
      // Marcar si este acorde es una capa de fondo
      sequencerHasLayeredChord = (mode != SEQ_MODE_CHORD && seqStep.layerChord);
    } else {
      // NUEVO: Si el paso NO tiene acorde ni layerChord, silenciar el acorde que venía del paso anterior
      if (sequencerHasLayeredChord || sequencerActiveMode == SEQ_MODE_CHORD) {
        stopChordFromRoot(sequencerActiveRoot);
      }
      sequencerHasLayeredChord = false;
    }

    // B) ¿QUÉ MODO DE NAVEGACIÓN TEMPORAL TIENE EL PASO?
    // Ajustamos las variables de control para las notas que irán sonando en tiempo real
    if (mode == SEQ_MODE_MELODY) {
      sequencerActiveMode = SEQ_MODE_MELODY;
      sequencerGateActive = true;
      sequencerMelodyIndex = 0;
      sequencerMelodyNextMs = now; // Dispara la primera nota inmediatamente
    }
    else if (mode == SEQ_MODE_ARP) {
      sequencerActiveMode = SEQ_MODE_ARP;
      sequencerGateActive = true;
      sequencerArpIndex = 0;
      sequencerArpNextMs = now; // Dispara el arpegio inmediatamente
    }
    else if (mode == SEQ_MODE_CHORD) {
      sequencerActiveMode = SEQ_MODE_CHORD;
      sequencerGateActive = true;
    }
  }

  // Refrescar pantalla si estamos en la página del secuenciador
  if (currentPage == PAGE_SEQ &&
      (sequencerUiStepShown != sequencerPlayStep || sequencerUiBarShown != sequencerPlayBar)) {
    drawExtraValue();
    sequencerUiStepShown = sequencerPlayStep;
    sequencerUiBarShown = sequencerPlayBar;
  }
}

uint8_t buildSequencerStepNotes(uint8_t step, uint8_t* notesOut, uint8_t maxNotes) {
  step &= 0xF;
  SequencerStep &seqStep = sequencerSteps[step];
  if (seqStep.chord == CHORD_REST || seqStep.chord == CHORD_END) return 0;
  if (seqStep.chord == CHORD_PLAYED && seqStep.playedCount > 0) {
    uint8_t count = min(maxNotes, seqStep.playedCount);
    for (uint8_t i = 0; i < count; i++) notesOut[i] = seqStep.playedNotes[i];
    return count;
  }

  int8_t intervals[4] = {0, 4, 7, 12};
  uint8_t count = min((uint8_t)4, buildChordIntervalsFromType(seqStep.chord, seqStep.chordDensity, intervals));
  count = min(count, maxNotes);
  if (count == 0) return 0;
  applyChordInversion(intervals, count, min(seqStep.chordInversion, (uint8_t)(count - 1)));

  int spreadSemi = seqStep.chordSpread * 12.0f;
  for (uint8_t i = 0; i < count; i++) {
    int noteNumber = (int)seqStep.root + intervals[i] + (seqStep.chordOctaveShift * 12);
    if (i > 0) noteNumber += ((int)i - 1) * spreadSemi;
    notesOut[i] = (uint8_t)constrain(noteNumber, 0, 127);
  }
  return count;
}

void seqCopyNextStep(uint8_t step){
  if(step < MAX_MELODY_NOTES) sequencerSteps[step+1] = sequencerSteps[step];
}
void seqDeleteStep(uint8_t step){
  sequencerSteps[step] = SEQUENCER_DEFAULT_STEP;
  drawMelody(step);
}
void seqDefault(uint8_t steps){
  /* //para redimensionar
  if(sequencerSteps != nullptr){
    delete[] sequencerSteps;
  }
  sequencerSteps = new SequencerStep[stepsForSeq];
  */
  for(byte n=0;n<steps;n++){
    sequencerSteps[n] = SEQUENCER_DEFAULT_STEP;
  }
}
/*
void playSequencerChordNotes(uint8_t step, uint8_t velocity) {
  step &= 0xF;
  SequencerStep &seqStep = sequencerSteps[step];
  uint8_t rootNote = seqStep.root;
  uint8_t oldRoot = sequencerActiveRoot;
  uint8_t oldCount = 0;
  if (sequencerGateActive && sequencerActiveMode == SEQ_MODE_CHORD) {
    oldCount = min(MAX_CHORD_NOTES, chordGeneratedCount[oldRoot]);
  }
  uint8_t oldNotes[MAX_CHORD_NOTES] = {0};
  for (uint8_t i = 0; i < oldCount && i < MAX_CHORD_NOTES; i++) oldNotes[i] = chordGeneratedNotes[oldRoot][i];
  clearPendingChordNotesByRoot(oldRoot);

  uint8_t newNotes[SEQUENCER_MAX_PLAYED_NOTES] = {0};
  uint8_t noteCount = buildSequencerStepNotes(step, newNotes, SEQUENCER_MAX_PLAYED_NOTES);
  int velocityScaled = constrain((int)velocity * seqStep.chordVelocityScale, 1, 127);
  int spreadSemi = seqStep.chordSpread * 12.0f;
  uint32_t now = millis();
  
  // Stop old notes that are not in the new chord
  for (uint8_t i = 0; i < oldCount; i++) {
    if (!noteInList(newNotes, noteCount, oldNotes[i])) noteOff(oldNotes[i]);
  }

  // Apply strum timing and spread to new notes (matching playChordFromRoot behavior)
  
  for (uint8_t i = 0; i < noteCount; i++) {
    if (!noteInList(oldNotes, oldCount, newNotes[i])) {
      int strumDelay = (int)i * (int)seqStep.chordStrumMs * 4;
      if (strumDelay <= 0) {
        noteOn(newNotes[i], velocityScaled);
      }
      else {
        scheduleChordNote(rootNote, newNotes[i], velocityScaled, now + (uint32_t)strumDelay);
      }
    }
  }

  if (oldCount > 0) chordGeneratedCount[oldRoot] = 0;
  for (uint8_t i = 0; i < noteCount; i++) chordGeneratedNotes[rootNote][i] = newNotes[i];
  chordGeneratedCount[rootNote] = noteCount;
}*/
void playSequencerChordNotes(uint8_t step, uint8_t velocity) {
  step &= 0xF;
  SequencerStep &seqStep = sequencerSteps[step];
  uint8_t rootNote = seqStep.root;
  uint8_t oldRoot = sequencerActiveRoot;

  // 1. Apagar SIEMPRE el acorde anterior de forma garantizada
  stopChordFromRoot(oldRoot);
  clearPendingChordNotesByRoot(oldRoot);

  uint8_t newNotes[SEQUENCER_MAX_PLAYED_NOTES] = {0};
  uint8_t noteCount = buildSequencerStepNotes(step, newNotes, SEQUENCER_MAX_PLAYED_NOTES);
  int velocityScaled = constrain((int)velocity * seqStep.chordVelocityScale, 1, 127);
  uint32_t now = millis();

  // 2. Generar el acorde del paso actual
  for (uint8_t i = 0; i < noteCount; i++) {
    int strumDelay = (int)i * (int)seqStep.chordStrumMs * 4;
    if (strumDelay <= 0) {
      noteOn(newNotes[i], velocityScaled);
    }
    else {
      scheduleChordNote(rootNote, newNotes[i], velocityScaled, now + (uint32_t)strumDelay);
    }
  }

  chordGeneratedCount[rootNote] = noteCount;
  for (uint8_t i = 0; i < noteCount; i++) chordGeneratedNotes[rootNote][i] = newNotes[i];
}
void sequencerStopActiveNote() {
  if (!sequencerGateActive && !sequencerHasLayeredChord) return;

  // 1. Apagar el acorde de fondo si existía
  if (sequencerHasLayeredChord || sequencerActiveMode == SEQ_MODE_CHORD) {
    stopChordFromRoot(sequencerActiveRoot);
    sequencerHasLayeredChord = false;
  }

  // 2. Apagar la nota del arpegio/melodía activa
  if (sequencerArpNoteOn) {
    noteOff(sequencerActiveArpNote);
    sequencerArpNoteOn = false;
  }

  // 3. Apagar la nota activa de la melodía
  if (sequencerMelodyNoteOn) {
    noteOff(sequencerActiveMelodyNote);
    sequencerMelodyNoteOn = false;
  }

  sequencerGateActive = false;
}

void captureSequencerStepFromMidi(uint8_t velocity) {
  if (currentPage != PAGE_SEQ || sequencerState != SEQ_STATE_REC) return;

  uint8_t held[SEQUENCER_MAX_PLAYED_NOTES] = {0};
  uint8_t count = collectMidiHeldNotes(held, SEQUENCER_MAX_PLAYED_NOTES);
  if (count == 0) return;

  uint8_t step = sequencerEditStep & 0xF;
  SequencerStep &seqStep = sequencerSteps[step];

  // =========================================================
  // MODO MELODÍA: Grabación acumulativa (Paso a Paso)
  // =========================================================
  if (seqStep.mode == SEQ_MODE_MELODY) {
    
    // Si cambiamos de paso o es la primera nota, reiniciamos el contador
    if (lastMelodyEditStep != step) {
      seqStep.melodyCount = 0;
      lastMelodyEditStep = step;
    }

    // Si aún hay espacio en el buffer (máximo 16 notas)
    if (seqStep.melodyCount < MAX_MELODY_NOTES) {
      uint8_t idx = seqStep.melodyCount;
      
      seqStep.melodyNotes[idx]      = held[0]; // Capturamos la primera nota presionada
      seqStep.melodyVelocities[idx] = (int)velocity;
      seqStep.melodyDurations[idx]  = DURATION_PRESETS[selectedDurationIdx];   // Duración seleccionada
      if(sequencerState == SEQ_STATE_REC) Serial.printf("%d = N:%d, V:%d, D:%1.3f, Prst:%d\n", seqStep.melodyCount,
          seqStep.melodyNotes[idx],seqStep.melodyVelocities[idx],seqStep.melodyDurations[idx],selectedDurationIdx);
      seqStep.melodyCount++; // Incrementamos el total de notas de este paso
    }

    // Actualizar la pantalla del sintetizador
    if (!suppressUiRefresh && currentPage == PAGE_SEQ) {
      drawValue(3); // Muestra la última nota ingresada
      drawValue(6); // Muestra la velocidad
      drawExtraValue();
      drawMelody(step);
    }
    return;
  }

  // =========================================================
  // MODOS CHORD / ARP: Lógica original (Captura instantánea)
  // =========================================================
  lastMelodyEditStep = 255; // Resetea el control de melodía al cambiar de modo
  uint8_t root = held[0];
  seqStep.root = root;
  seqStep.velocity = constrain((int)velocity, 20, 120);

  if (count == 1) {
    ChordType selectedType = seqStep.chord;
    if (selectedType == CHORD_PLAYED || selectedType == CHORD_REST || selectedType == CHORD_END) selectedType = chordType;
    seqStep.chord = selectedType;
    seqStep.playedCount = 0;
  }
  else {
    seqStep.chord = CHORD_PLAYED;
    seqStep.playedCount = count;
    for (uint8_t i = 0; i < count; i++) {
      seqStep.playedNotes[i] = held[i];
    }
  }

  pageParam[5][2].value = seqStep.mode;
  pageParam[5][3].value = seqStep.root;
  pageParam[5][4].value = seqStep.chord;
  pageParam[5][6].value = seqStep.velocity;
  if (!suppressUiRefresh &&  currentPage == PAGE_SEQ) {
    drawValue(2);
    drawValue(3);
    drawValue(4);
    drawValue(6);
  }
}

void insertMelodyRest() {
  if (currentPage != PAGE_SEQ || sequencerState != SEQ_STATE_REC) return;
  
  uint8_t step = sequencerEditStep & 0xF;
  SequencerStep &seqStep = sequencerSteps[step];

  if (seqStep.mode == SEQ_MODE_MELODY && seqStep.melodyCount < MAX_MELODY_NOTES) {
    uint8_t idx = seqStep.melodyCount;
    seqStep.melodyNotes[idx]     = MELODY_NOTE_REST;
    seqStep.melodyVelocities[idx]= 0;
    seqStep.melodyDurations[idx] = DURATION_PRESETS[selectedDurationIdx]; // Duración de la pausa
    seqStep.melodyCount++;
  }
}

uint8_t sequencerMaskedArpStep(uint8_t step, uint8_t mask) {
  mask = mask ? mask : 0xFF;
  uint8_t activeSlots[8];
  uint8_t activeCount = 0;

  for (uint8_t i = 0; i < 8; i++) {
    if (mask & (1 << i)) activeSlots[activeCount++] = i;
  }

  if (activeCount == 0) return step;
  return activeSlots[step % activeCount];
}

uint8_t sequencerArpToneFromStep(uint8_t arpStep, uint8_t toneCount, ArpMode mode, uint8_t mask) {
  if (toneCount == 0) return 0;
  uint8_t step = sequencerMaskedArpStep(arpStep, mask);

  switch (mode) {
    case ARP_DOWN:
      return (toneCount - 1) - (step % toneCount);

    case ARP_UPDOWN: {
      if (toneCount <= 1) return 0;
      uint8_t cycle = (toneCount * 2) - 2;
      uint8_t pos = step % cycle;
      return (pos < toneCount) ? pos : (cycle - pos);
    }

    case ARP_DOWNUP: {
      if (toneCount <= 1) return 0;
      uint8_t cycle = (toneCount * 2) - 2;
      uint8_t pos = step % cycle;
      return (pos < toneCount) ? (toneCount - 1 - pos) : (pos - toneCount + 1);
    }

    case ARP_RANDOM:
      return sequencerMaskedArpStep(random(8), mask) % toneCount;

    case ARP_INOUT:
      return arpIndexInOut(step, toneCount);

    case ARP_OUTIN:
      return arpIndexOutIn(step, toneCount);

    case ARP_PATTERN:
      return step % toneCount;

    case ARP_CUSTOM:
      return customArpPattern[step];

    case ARP_UP:
    default:
      return step % toneCount;
  }
}

bool isSequencerScopedControl(uint8_t page, uint8_t param) {
  if (page == PAGE_ADSR) return true;
  if (sequencerState == SEQ_STATE_OFF) return true;

  if (page == PAGE_ARP) {
    return (param >= 1 && param <= 4) || param == 6 || param == 7;
  }

  if (page == PAGE_CHORD) {
    return param >= 2;
  }

  return true;
}

void syncSequencerScopedValues(uint8_t step) {
  step &= 0xF;
  SequencerStep &seqStep = sequencerSteps[step];

  pageParam[4][2].value = seqStep.chordInversion;
  pageParam[4][3].value = seqStep.chordOctaveShift;
  pageParam[4][4].value = seqStep.chordVelocityScale;
  pageParam[4][5].value = seqStep.chordSpread;
  pageParam[4][6].value = seqStep.chordStrumMs;
  pageParam[4][7].value = seqStep.chordDensity;

  pageParam[6][1].value = seqStep.arpRateHz;
  pageParam[6][2].value = seqStep.arpMode;
  pageParam[6][3].value = seqStep.arpOctaves;
  pageParam[6][4].value = seqStep.arpGate;
  pageParam[6][6].value = seqStep.arpSwing;
  pageParam[6][7].value = seqStep.arpPatternMask;
}
