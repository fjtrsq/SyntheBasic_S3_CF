void scheduleChordNote(uint8_t root, uint8_t note, uint8_t velocity, uint32_t dueMs) {
  for (uint8_t i = 0; i < MAX_PENDING_CHORD_NOTES; i++) {
    if (!pendingChordNotes[i].active) {
      pendingChordNotes[i] = {true, root, note, velocity, dueMs};
      return;
    }
  }
  noteOn(note, velocity); // fallback si cola llena
}

void flushPendingChordNotes() {
  uint32_t now = millis();
  for (uint8_t i = 0; i < MAX_PENDING_CHORD_NOTES; i++) {
    if (pendingChordNotes[i].active && (int32_t)(now - pendingChordNotes[i].dueMs) >= 0) {
      noteOn(pendingChordNotes[i].note, pendingChordNotes[i].velocity);
      pendingChordNotes[i].active = false;
    }
  }
}

void clearPendingChordNotesByRoot(uint8_t root) {
  for (uint8_t i = 0; i < MAX_PENDING_CHORD_NOTES; i++) {
    if (pendingChordNotes[i].active && pendingChordNotes[i].root == root) {
      pendingChordNotes[i].active = false;
    }
  }
}

uint8_t buildChordIntervalsFromType(ChordType type, uint8_t density, int8_t* intervalsOut) {
  switch (type) {
    case CHORD_PLAYED:
    case CHORD_REST:
    case CHORD_END:
      return 0;
    case CHORD_MINOR:
      intervalsOut[0] = 0; intervalsOut[1] = 3; intervalsOut[2] = 7; intervalsOut[3] = 10;
      break;
    case CHORD_SUS2:
      intervalsOut[0] = 0; intervalsOut[1] = 2; intervalsOut[2] = 7; intervalsOut[3] = 12;
      break;
    case CHORD_SUS4:
      intervalsOut[0] = 0; intervalsOut[1] = 5; intervalsOut[2] = 7; intervalsOut[3] = 12;
      break;
    case CHORD_POWER:
      intervalsOut[0] = 0; intervalsOut[1] = 7; intervalsOut[2] = 12; intervalsOut[3] = 19;
      break;
    case CHORD_MAJ7:
      intervalsOut[0] = 0; intervalsOut[1] = 4; intervalsOut[2] = 7; intervalsOut[3] = 11;
      break;
    case CHORD_MIN7:
      intervalsOut[0] = 0; intervalsOut[1] = 3; intervalsOut[2] = 7; intervalsOut[3] = 10;
      break;
    case CHORD_DOM7:
      intervalsOut[0] = 0; intervalsOut[1] = 4; intervalsOut[2] = 7; intervalsOut[3] = 10;
      break;
    case CHORD_MAJOR:
    default:
      intervalsOut[0] = 0; intervalsOut[1] = 4; intervalsOut[2] = 7; intervalsOut[3] = 12;
      break;
  }
  return density;
}

uint8_t buildChordIntervals(int8_t* intervalsOut) {
  return buildChordIntervalsFromType(chordType, chordDensity, intervalsOut);
}

void applyChordInversion(int8_t* intervals, uint8_t count, uint8_t inversion) {
  for (uint8_t n = 0; n < inversion; n++) {
    int8_t moved = intervals[0] + 12;
    for (uint8_t i = 0; i + 1 < count; i++) intervals[i] = intervals[i + 1];
    intervals[count - 1] = moved;
  }
}

//void stopChordFromRoot(uint8_t rootNote);

void stopChordFromRoot(uint8_t rootNote) {
  clearPendingChordNotesByRoot(rootNote);
  uint8_t count = chordGeneratedCount[rootNote];
  for (uint8_t i = 0; i < count; i++) {
    noteOff(chordGeneratedNotes[rootNote][i]);
  }
  chordGeneratedCount[rootNote] = 0;
}

void playChordFromRoot(uint8_t rootNote, uint8_t velocity) {
  stopChordFromRoot(rootNote);

  int8_t intervals[4] = {0, 4, 7, 12};
  uint8_t noteCount = buildChordIntervals(intervals);
  applyChordInversion(intervals, noteCount, min(chordInversion, (uint8_t)(noteCount - 1)));

  int velocityScaled = constrain((int)roundf(velocity * chordVelocityScale), 1, 127);
  int spreadSemi = chordSpread * 12.0f;
  uint32_t now = millis();

  for (uint8_t i = 0; i < noteCount; i++) {
    int noteNumber = (int)rootNote + intervals[i] + (chordOctaveShift * 12);
    if (i > 0) noteNumber += ((int)i - 1) * spreadSemi;
    noteNumber = constrain(noteNumber, 0, 127);
    uint8_t playedNote = (uint8_t)noteNumber;
    if (chordGeneratedCount[rootNote] < MAX_CHORD_NOTES) {
      chordGeneratedNotes[rootNote][chordGeneratedCount[rootNote]++] = playedNote;
    }

    int strumDelay = (int)i * (int)chordStrumMs * 4;
    if (strumDelay <= 0) {
      noteOn(playedNote, velocityScaled);
    }
    else {
      scheduleChordNote(rootNote, playedNote, velocityScaled, now + (uint32_t)strumDelay);
    }
  }
}

ChordType detectChordTypeFromHeld(const uint8_t* notes, uint8_t count, uint8_t root) {
  bool has2 = false, has3 = false, has4 = false, has5 = false, has7 = false, has10 = false, has11 = false;
  for (uint8_t i = 0; i < count; i++) {
    uint8_t interval = (uint8_t)((notes[i] + 12 - root) % 12);
    if (interval == 2) has2 = true;
    if (interval == 3) has3 = true;
    if (interval == 4) has4 = true;
    if (interval == 5) has5 = true;
    if (interval == 7) has7 = true;
    if (interval == 10) has10 = true;
    if (interval == 11) has11 = true;
  }

  if (has4 && has7 && has11) return CHORD_MAJ7;
  if (has3 && has7 && has10) return CHORD_MIN7;
  if (has4 && has7 && has10) return CHORD_DOM7;
  if (has4 && has7) return CHORD_MAJOR;
  if (has3 && has7) return CHORD_MINOR;
  if (has2 && has7) return CHORD_SUS2;
  if (has5 && has7) return CHORD_SUS4;
  if (has7) return CHORD_POWER;
  return CHORD_MAJOR;
}
