ArpHold selectedArpHoldMode() {
  return (ArpHold)constrain((int)arpHold, (int)HOLD_ORDER, (int)HOLD_STACK);
}

ArpHold effectiveArpHold() {
  return latchEnabled ? selectedArpHoldMode() : HOLD_OFF;
}

void arpSortHeldNotes() {
  for (int i = 0; i < arpHeldCount - 1; i++) {
    for (int j = i + 1; j < arpHeldCount; j++) {
      if (arpHeldNotes[j] < arpHeldNotes[i]) {
        uint8_t t = arpHeldNotes[i];
        arpHeldNotes[i] = arpHeldNotes[j];
        arpHeldNotes[j] = t;
      }
    }
  }
}

void arpAddNote(uint8_t note, uint8_t velocity) {
  ArpHold holdMode = effectiveArpHold();

  for (int i = 0; i < arpHeldCount; i++){ 
    if (arpHeldNotes[i] == note){
      if (holdMode == HOLD_PLAY) {// mover al final
        for (int j = i; j < arpHeldCount - 1; j++) {
          arpHeldNotes[j] = arpHeldNotes[j + 1];
        }
        arpHeldNotes[arpHeldCount - 1] = note;
        
      }
      else{
        // opcional: actualizar velocity
        arpHeldVelocities[i] = velocity;
      }
      return;
    }
  }
  if (arpHeldCount < 16) {
    arpHeldNotes[arpHeldCount] = note;
    arpHeldVelocities[arpHeldCount] = velocity;
    arpHeldCount++;

    if (holdMode == HOLD_ORDER) {
      arpSortHeldNotes();
    }
  }
}

void arpRemoveNote(uint8_t note) {
  for (int i = 0; i < arpHeldCount; i++) {
    if (arpHeldNotes[i] == note) {
      for (int j = i; j < arpHeldCount - 1; j++) arpHeldNotes[j] = arpHeldNotes[j + 1];
      arpHeldCount--;
      break;
    }
  }
}

void arpClearNotes() {
  arpHeldCount = 0;
  arpStepIndex = 0;
  arpSamplesToNextStep = 0;
  arpGateSamplesLeft = 0;
  if (arpGateActive) {
    noteOff(arpCurrentNote);
    arpGateActive = false;
  }
}

void arpClearNoteSoft() {
  arpHeldCount = 0;
}

void arpReleaseLatchedNotes() {
  for (int i = arpHeldCount - 1; i >= 0; i--) {
    if (!midiKeysDown[arpHeldNotes[i]]) {
      arpRemoveNote(arpHeldNotes[i]);
    }
  }

  if (arpHeldCount == 0) {
    arpClearNotes();
  }
}

int arpMaskedStep(int step) {
  uint8_t mask = arpPatternMask ? arpPatternMask : 0xFF;
  int activeSlots[8];
  int activeCount = 0;

  for (int i = 0; i < 8; i++) {
    if (mask & (1 << i)) activeSlots[activeCount++] = i;
  }

  if (activeCount == 0) return step;
  return activeSlots[step % activeCount];
}

int arpIndexInOut(int step, int total) {
  int pos = step % total;
  int center = (total - 1) / 2;
  if (pos == 0) return center;

  int distance = (pos + 1) / 2;
  int index = (pos & 1) ? center + distance : center - distance;
  while (index < 0) index += total;
  while (index >= total) index -= total;
  return index;
}

int arpIndexOutIn(int step, int total) {
  int pos = step % total;
  return (pos & 1) ? total - 1 - (pos / 2) : pos / 2;
}

ArpNote arpGetNextNote() {

  ArpNote out;
  out.note = 0;
  out.velocity = 0;

  // 🔹 Caso vacío
  if (arpHeldCount == 0) {
    return out;
  }

  int total = arpHeldCount * arpOctaves;
  int rawStep = arpStepIndex++;
  int step = arpMaskedStep(rawStep);

  // 🔹 Caso simple (una sola nota total)
  if (total <= 1) {
    out.note = arpHeldNotes[0];
    out.velocity = arpHeldVelocities[0];
    return out;
  }

  int index;

  // 🔹 Selección de índice (más eficiente)
  switch (arpMode) {

    case ARP_UP:
      index = step % total;
      break;

    case ARP_DOWN:
      index = (total - 1) - (step % total);
      break;

    case ARP_UPDOWN: {
      int cycle = (total * 2) - 2;
      int pos = step % cycle;
      index = (pos < total) ? pos : (cycle - pos);
      break;
    }

    case ARP_DOWNUP: {
      int cycle = (total * 2) - 2;
      int pos = step % cycle;
      index = (pos < total) ? (total - 1 - pos) : (pos - total + 1);
      break;
    }

    case ARP_RANDOM:
      index = random(total);
      break;

    case ARP_INOUT:
      index = arpIndexInOut(step, total);
      break;

    case ARP_OUTIN:
      index = arpIndexOutIn(step, total);
      break;

    case ARP_PATTERN:
    default: {
      index = step % total;
      break;
    }
  }

  // 🔹 Calcular nota base + octava (optimizado)
  int baseIdx = index % arpHeldCount;
  int note = arpHeldNotes[baseIdx] + 12 * (index / arpHeldCount);

  // 🔹 Clamp sin if (ligeramente más rápido)
  if (note > 127) note = 127;

  out.note = (uint8_t)note;
  out.velocity = arpHeldVelocities[baseIdx];

  return out;
}

uint32_t arpStepSamples() {
  float base = SAMPLE_RATE / ((arpRateHz < 0.1f) ? 0.1f : arpRateHz);
  float swingFactor = arpSwingPhase ? (1.0f + arpSwing) : (1.0f - arpSwing);
  arpSwingPhase = !arpSwingPhase;
  uint32_t out = (uint32_t)(base * swingFactor);
  if (out < 32) out = 32;
  return out;
}

void releaseAllVoices() {
  for (int i = 0; i < NUM_VOICES; i++) {
    if (voices[i].active) {
      for (uint8_t osc = 0; osc < N_OSC; osc++) voices[i].envState[osc] = ENV_RELEASE;
    }
  }
}
