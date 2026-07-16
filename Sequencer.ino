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

void processSequencer() {
  if (sequencerState != SEQ_STATE_ON || arpEnabled) return;

  uint32_t now = millis();
  uint32_t beatMs = 60000UL / max((uint16_t)60, sequencerBpm);
  if (beatMs < 80) beatMs = 80;

  if (sequencerArpNoteOn && (int32_t)(now - sequencerArpOffMs) >= 0) {
    noteOff(sequencerActiveArpNote);
    sequencerArpNoteOn = false;
  }

  if (sequencerGateActive && (int32_t)(now - sequencerStepEndMs) >= 0) {
    bool keepForLegatoTransition =
      (sequencerActiveMode == SEQ_MODE_CHORD && sequencerTransitionMode == SEQ_TRANS_LEGATO);
    if (!keepForLegatoTransition) {
      sequencerStopActiveNote();
    }
    sequencerPlayStep = (sequencerPlayStep + 1) & 0x07;
    sequencerPlayBar = 1;
    sequencerNextStepMs = now;
  }

  if (sequencerGateActive) {
    uint8_t stepIdx = sequencerPlayStep & 0x07;
    SequencerStep &seqStep = sequencerSteps[stepIdx];
    uint32_t barMs = 4UL * beatMs;
    uint8_t maxBars = max((uint8_t)1, seqStep.bars);
    uint32_t elapsed = now - sequencerStepStartMs;
    uint8_t bar = (uint8_t)(elapsed / max((uint32_t)1, barMs)) + 1;
    sequencerPlayBar = constrain(bar, (uint8_t)1, maxBars);
  }

  if (sequencerGateActive && sequencerActiveMode == SEQ_MODE_ARP) {
    uint8_t stepIdx = sequencerPlayStep & 0x07;
    SequencerStep &seqStep = sequencerSteps[stepIdx];
    uint8_t velocity = constrain((int)seqStep.velocity, 1, 127);
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
      uint8_t octaves = constrain(seqStep.arpOctaves, 1, 3);
      uint8_t expandedCount = count * octaves;
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
      sequencerArpOffMs = now + (uint32_t)(arpTickMs * constrain(seqStep.arpGate, 0.1f, 0.95f));
      uint32_t swingOffset = (sequencerArpIndex & 0x01) ? (uint32_t)(arpTickMs * constrain(seqStep.arpSwing, 0.0f, 0.45f)) : 0;
      sequencerArpNextMs += arpTickMs + swingOffset;
      sequencerArpIndex++;
      now = millis();
    }
  }

  if (currentPage == 5 &&
      (sequencerUiStepShown != sequencerPlayStep || sequencerUiBarShown != sequencerPlayBar)) {
    drawExtraValue();
    sequencerUiStepShown = sequencerPlayStep;
    sequencerUiBarShown = sequencerPlayBar;
  }

  if ((int32_t)(now - sequencerNextStepMs) < 0) return;

  uint8_t stepIdx = sequencerPlayStep & 0x07;
  SequencerStep &seqStep = sequencerSteps[stepIdx];
  if (seqStep.chord == CHORD_END) {
    sequencerStopActiveNote();
    sequencerPlayStep = 0;
    sequencerPlayBar = 1;
    sequencerNextStepMs = now + 1;
    return;
  }

  uint32_t stepDurMs = (uint32_t)max((uint8_t)1, seqStep.bars) * 4UL * beatMs;
  sequencerStepStartMs = now;
  sequencerPlayBar = 1;
  sequencerStepEndMs = now + stepDurMs;
  sequencerNextStepMs = sequencerStepEndMs;

  SequencerMode mode = seqStep.mode;
  uint8_t root = seqStep.root;
  uint8_t velocity = constrain((int)seqStep.velocity, 1, 127);

  if (seqStep.chord == CHORD_REST) {
    sequencerStopActiveNote();
    sequencerActiveMode = SEQ_MODE_CHORD;
    sequencerActiveRoot = root;
    sequencerGateActive = true;
  }
  else {
    if (sequencerTransitionMode == SEQ_TRANS_LEGATO &&
        sequencerActiveMode == SEQ_MODE_CHORD &&
        mode != SEQ_MODE_CHORD) {
      stopChordFromRoot(sequencerActiveRoot);
    }

    if (mode == SEQ_MODE_CHORD) {
      if (sequencerTransitionMode == SEQ_TRANS_RETRIG) {
        sequencerActiveRoot = root;
        sequencerActiveMode = SEQ_MODE_CHORD;
        stopChordFromRoot(sequencerActiveRoot);
        playSequencerChordNotes(stepIdx, velocity);
      }
      else {
        sequencerActiveMode = SEQ_MODE_CHORD;
        playSequencerChordNotes(stepIdx, velocity);
        sequencerActiveRoot = root;
      }
      sequencerGateActive = true;
    }
    else {
      sequencerActiveMode = SEQ_MODE_ARP;
      sequencerGateActive = true;
      sequencerArpIndex = 0;
      sequencerArpNextMs = now;
    }
  }

  if (currentPage == 5 &&
      (sequencerUiStepShown != sequencerPlayStep || sequencerUiBarShown != sequencerPlayBar)) {
    drawExtraValue();
    sequencerUiStepShown = sequencerPlayStep;
    sequencerUiBarShown = sequencerPlayBar;
  }
}

uint8_t buildSequencerStepNotes(uint8_t step, uint8_t* notesOut, uint8_t maxNotes) {
  step &= 0x07;
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

  int spreadSemi = (int)roundf(seqStep.chordSpread * 12.0f);
  for (uint8_t i = 0; i < count; i++) {
    int noteNumber = (int)seqStep.root + intervals[i] + (seqStep.chordOctaveShift * 12);
    if (i > 0) noteNumber += ((int)i - 1) * spreadSemi;
    notesOut[i] = (uint8_t)constrain(noteNumber, 0, 127);
  }
  return count;
}

void seqCopyStep(uint8_t step){
  for(byte n=step;n<stepsForSeq;n++){ 
    if(sequencerSteps[n].chord != CHORD_END){
      sequencerSteps[n] = sequencerSteps[step];
    }
  }
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

void playSequencerChordNotes(uint8_t step, uint8_t velocity) {
  step &= 0x07;
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
  int velocityScaled = constrain((int)roundf(velocity * seqStep.chordVelocityScale), 1, 127);
  int spreadSemi = (int)roundf(seqStep.chordSpread * 12.0f);
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
}

void sequencerStopActiveNote() {
  if (!sequencerGateActive) return;
  if (sequencerActiveMode == SEQ_MODE_CHORD) stopChordFromRoot(sequencerActiveRoot);
  else if (sequencerArpNoteOn) noteOff(sequencerActiveArpNote);
  sequencerArpNoteOn = false;
  sequencerGateActive = false;
}

void captureSequencerStepFromMidi(uint8_t velocity) {
  if (currentPage != 5 || sequencerState != SEQ_STATE_REC) return;

  uint8_t held[SEQUENCER_MAX_PLAYED_NOTES] = {0};
  uint8_t count = collectMidiHeldNotes(held, SEQUENCER_MAX_PLAYED_NOTES);
  if (count == 0) return;

  uint8_t step = sequencerEditStep & 0x07;
  SequencerStep &seqStep = sequencerSteps[step];
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

  synthValue[5][2] = (float)seqStep.mode;
  synthValue[5][3] = (float)seqStep.root;
  synthValue[5][4] = (float)seqStep.chord;
  synthValue[5][6] = (float)seqStep.velocity;
  if (!suppressUiRefresh && currentPage == 5) {
    drawValue(2);
    drawValue(3);
    drawValue(4);
    drawValue(6);
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

    case ARP_UP:
    default:
      return step % toneCount;
  }
}

bool isSequencerScopedControl(uint8_t page, uint8_t param) {
  if (page == ADSR_PARAM_PAGE) return true;
  if (sequencerState == SEQ_STATE_OFF) return true;

  if (page == 6) {
    return (param >= 1 && param <= 4) || param == 6 || param == 7;
  }

  if (page == 4) {
    return param >= 2;
  }

  return true;
}

void syncSequencerScopedValues(uint8_t step) {
  step &= stepsForSeq;
  SequencerStep &seqStep = sequencerSteps[step];

  synthValue[4][2] = (float)seqStep.chordInversion;
  synthValue[4][3] = (float)seqStep.chordOctaveShift;
  synthValue[4][4] = seqStep.chordVelocityScale * 100.0f;
  synthValue[4][5] = seqStep.chordSpread;
  synthValue[4][6] = (float)seqStep.chordStrumMs;
  synthValue[4][7] = (float)seqStep.chordDensity;

  synthValue[6][1] = seqStep.arpRateHz;
  synthValue[6][2] = (float)seqStep.arpMode;
  synthValue[6][3] = (float)seqStep.arpOctaves;
  synthValue[6][4] = seqStep.arpGate;
  synthValue[6][6] = seqStep.arpSwing;
  synthValue[6][7] = (float)seqStep.arpPatternMask;
}
