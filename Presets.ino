const char* getFileSuffix() {
  switch (modeFileSave) {
    case FILE_SEQ:   return ".seq";
    case FILE_C_ARP: return ".c_arp";
    default:         return ".sound";
  }
}

String presetPathFromName(const char* rawName) {
  char trimmed[PN_LEN + 1];
  strncpy(trimmed, rawName, PN_LEN);
  trimmed[PN_LEN] = '\0';

  int end = PN_LEN - 1;
  while (end >= 0 && (trimmed[end] == ' ' || trimmed[end] == '_')) end--;
  trimmed[end + 1] = '\0';

  if (trimmed[0] == '\0') {
    strncpy(trimmed, "NONAME", sizeof(trimmed) - 1);
    trimmed[sizeof(trimmed) - 1] = '\0';
  }

  char safeName[PN_LEN + 1];
  size_t len = strlen(trimmed);
  for (size_t i = 0; i < len; i++) {
    safeName[i] = (trimmed[i] == ' ') ? '_' : trimmed[i];
  }
  safeName[len] = '\0';

  char path[32];
  // Construye la ruta directamente con el sufijo activo
  snprintf(path, sizeof(path), "/%s%s", safeName, getFileSuffix());
  return String(path);
}

bool presetNameFromPath(const char* path, char* outName) {
  if (!path || !outName) return false;
  const char* suffix = getFileSuffix();
  
  if (path[0] == '/') path++;
  size_t pathLen = strlen(path);
  size_t suffixLen = strlen(suffix);

  // Comprueba únicamente si el archivo termina con la extensión actual
  if (pathLen <= suffixLen) return false;
  if (strcmp(path + pathLen - suffixLen, suffix) != 0) return false;

  size_t nameLen = pathLen - suffixLen;
  if (nameLen > PN_LEN) nameLen = PN_LEN;
  for (size_t i = 0; i < nameLen; i++) {
    char c = path[i];
    outName[i] = (c == '_') ? ' ' : c;
  }
  outName[nameLen] = '\0';
  return true;
}

bool rawFileSave(const String& path, const void* buffer, size_t size) {
  fs::File file = LittleFS.open(path, "w");
  if (!file) return false;
  size_t written = file.write((const uint8_t*)buffer, size);
  file.close();
  Serial.print(written);
  return written == size;
}

bool rawFileLoad(const String& path, void* buffer, size_t size) {
  fs::File file = LittleFS.open(path, "r");
  if (!file) return false;
  size_t readBytes = file.read((uint8_t*)buffer, size);
  file.close();
  Serial.print(readBytes);
  return readBytes == size;
}

bool presetSaveByName(const char* name) {
  String path = presetPathFromName(name);

  switch (modeFileSave) {
    case FILE_SOUND: {
      StoredPreset preset = {};
      strncpy(preset.name, name, PN_LEN);
      preset.name[PN_LEN] = '\0';

      // 1. Guardar solo los parámetros 1 y 2 de la página 0
      preset.values[0][1] = pageParam[0][1].value;
      preset.values[0][2] = pageParam[0][2].value;

      for (uint8_t page = 1; page <= 3; page++) {
        for (uint8_t param = 0; param < PARAMS_PER_PAGE; param++) {
          preset.values[page][param] = pageParam[page][param].value;
        }
      }
      for (uint8_t osc = 0; osc < N_OSC; osc++) {
        for (uint8_t param = 0; param < TOTAL_ADSR; param++) {
          preset.oscAdsr[osc][param] = ADSRvalues[osc][param];
        }
        preset.oscMix[osc] = ADSRmixValues[osc];
      }
      return rawFileSave(path, &preset, sizeof(preset));
    }

    case FILE_SEQ:
      return rawFileSave(path, sequencerSteps, sizeof(SequencerStep) * stepsForSeq);

    case FILE_C_ARP: {
      StoredCustomArp arpData = {};
      // 1. Copiar el patrón de notas
      memcpy(arpData.pattern, customArpPattern, sizeof(customArpPattern));

      // 2. Copiar los valores de arpCustom[2] a arpCustom[6]
      for (uint8_t i = 0; i < 5; i++) {
        arpData.values[i] = arpCustom[2 + i].value;
      }

      return rawFileSave(path, &arpData, sizeof(arpData));
    }
  }

  return false;
}

bool presetLoadByName(const char* name) {
  String path = presetPathFromName(name);

  switch (modeFileSave) {
    case FILE_SOUND: {
      StoredPreset preset = {};
      if (!rawFileLoad(path, &preset, sizeof(preset))) return false;

      strncpy(presetEditName, preset.name, PN_LEN);
      presetEditName[PN_LEN] = '\0';

      suppressUiRefresh = true;

      // 1. Restaurar solo los parámetros 1 y 2 de la página 0
      pageParam[0][1].value = preset.values[0][1];
      refreshValue((Page)0, 1, 0);

      pageParam[0][2].value = preset.values[0][2];
      refreshValue((Page)0, 2, 0);

      // 2. Restaurar todos los parámetros de las páginas 1, 2 y 3
      for (uint8_t page = 1; page <= 3; page++) {
        for (uint8_t param = 0; param < PARAMS_PER_PAGE; param++) {
          pageParam[page][param].value = preset.values[page][param];
          refreshValue((Page)page, param, 0);
        }
      }
      suppressUiRefresh = false;

      for (uint8_t osc = 0; osc < N_OSC; osc++) {
        for (uint8_t param = 0; param < TOTAL_ADSR; param++) {
          ADSRvalues[osc][param] = preset.oscAdsr[osc][param];
        }
        ADSRmixValues[osc] = preset.oscMix[osc];
        updateEnvelopeRates(osc);
      }
      return true;
    }

    case FILE_SEQ: {
      bool loaded = rawFileLoad(path, sequencerSteps, sizeof(SequencerStep) * stepsForSeq);
      if (loaded) {
        uint8_t step = sequencerEditStep & 0xF;
        syncSequencerScopedValues(step);
        if (currentPage == PAGE_SEQ) {
          drawExtraValue();
          drawMelody(step);
        }
      }
      return loaded;
    }

    case FILE_C_ARP: {
      StoredCustomArp arpData = {};
      if (!rawFileLoad(path, &arpData, sizeof(arpData))) return false;

      // 1. Restaurar el patrón de notas
      memcpy(customArpPattern, arpData.pattern, sizeof(customArpPattern));

      // 2. Restaurar los valores en arpCustom[2..6]
      suppressUiRefresh = true;
      for (uint8_t i = 0; i < 5; i++) {
        arpCustom[2 + i].value = arpData.values[i];
        processCustomArpEditor(i, 0);
      }
      suppressUiRefresh = false;

      // 3. Redibujar la interfaz si estás en la página del arpegiador
      if (currentPage == PAGE_ARP && customArpEditorState == CUSTOM_ARP_EDIT) {
        for (uint8_t i = 2; i <= 6; i++) {
          drawValue(i);
        }
        drawCustomArpPattern();
        drawExtraValue();
      }
      return true;
    }
  }
  return false;
}

bool presetDeleteByName(const char* name) {
  String path = presetPathFromName(name);
  if (!LittleFS.exists(path)) return false;
  return LittleFS.remove(path);
}

void presetSelectFileByIndex(int index) {
  if (presetFileCount == 0) {
    presetFileSelection = -1;
    memset(presetEditName, ' ', PN_LEN);
    presetEditName[PN_LEN] = '\0';
    return;
  }

  index = constrain(index, 0, presetFileCount - 1);
  presetFileSelection = index;

  memset(presetEditName, ' ', PN_LEN);
  strncpy(presetEditName, presetFileNames[index], PN_LEN);
  presetEditName[PN_LEN] = '\0';
  drawExtraValue();

}

void refreshPresetFileList(bool keepCurrentSelection) {
  char previousName[PN_LEN + 1];
  strncpy(previousName, presetEditName, PN_LEN);
  previousName[PN_LEN] = '\0';

  presetFileCount = 0;
  fs::File root = LittleFS.open("/");
  if (!root || !root.isDirectory()) {
    pageParam[PAGE_FILE][0].min = 0;
    pageParam[PAGE_FILE][0].max = 0;
    pageParam[PAGE_FILE][0].value = 0;
    presetSelectFileByIndex(0);
    drawPresetFileList();
    return;
  }

  fs::File file = root.openNextFile();
  while (file && presetFileCount < MAX_PRESET_FILES) {
    if (!file.isDirectory()) {
      char parsedName[PN_LEN + 1];
      if (presetNameFromPath(file.name(), parsedName)) {
        strncpy(presetFileNames[presetFileCount], parsedName, PN_LEN);
        presetFileNames[presetFileCount][PN_LEN] = '\0';
        presetFileCount++;
      }
    }
    file = root.openNextFile();
  }
  root.close();

  for (uint8_t i = 0; i < presetFileCount; i++) {
    for (uint8_t j = i + 1; j < presetFileCount; j++) {
      if (strcmp(presetFileNames[j], presetFileNames[i]) < 0) {
        char tmp[PN_LEN + 1];
        strncpy(tmp, presetFileNames[i], sizeof(tmp));
        strncpy(presetFileNames[i], presetFileNames[j], PN_LEN + 1);
        strncpy(presetFileNames[j], tmp, PN_LEN + 1);
      }
    }
  }

  if (presetFileCount == 0) {
    pageParam[PAGE_FILE][0].min = 0;
    pageParam[PAGE_FILE][0].max = 0;
    pageParam[PAGE_FILE][0].value = 0;
    presetSelectFileByIndex(0);
    drawPresetFileList();
    return;
  }

  pageParam[PAGE_FILE][0].min = 0;
  pageParam[PAGE_FILE][0].max = presetFileCount - 1;

  int selectedIndex = pageParam[PAGE_FILE][0].value;
  if (keepCurrentSelection) {
    selectedIndex = -1;
    for (uint8_t i = 0; i < presetFileCount; i++) {
      if (strncmp(presetFileNames[i], previousName, PN_LEN) == 0) {
        selectedIndex = i;
        break;
      }
    }
    if (selectedIndex < 0) selectedIndex = constrain(pageParam[PAGE_FILE][0].value, 0, presetFileCount - 1);
  }
  else {
    selectedIndex = constrain(pageParam[PAGE_FILE][0].value, 0, presetFileCount - 1);
  }

  pageParam[PAGE_FILE][0].value = selectedIndex;
  presetSelectFileByIndex(selectedIndex);
  drawPresetFileList();
}

void endFeedbackPreset(uint32_t time){
  if(millis() - presetActionTime > time){
    flagTime = false;
    pageParam[PAGE_FILE][presetActionParam].value = 0;
    drawValue(presetActionParam);
    presetActionParam = -1;
    memset(presetActionLabel, 0, sizeof(presetActionLabel));
    presetActionTime = 0;
    leds.setColor(currentPage, GREEN);
    leds.show();
  }
}

void setFeedbackPreset(uint8_t param, const char* label) {
  flagTime = true;
  presetActionParam = param;
  strncpy(presetActionLabel, label, sizeof(presetActionLabel) - 1);
  presetActionLabel[sizeof(presetActionLabel) - 1] = '\0';
  presetActionTime = millis();
  leds.setColor(currentPage, RED);
  leds.show();
}

void presetInsertSelectedChar(bool autoAdvance) {
  int pos = pageParam[PAGE_FILE][6].value;
  int charIdx = pageParam[PAGE_FILE][7].value;

  for (int i = 0; i < pos; i++) {
    if (presetEditName[i] == '\0') {
      presetEditName[i] = '_'; // Aquí puedes poner '_' si prefieres el guion bajo
    }
  }

  presetEditName[pos] = PRESET_CHARS[charIdx];

  if (autoAdvance && pos < PN_LEN - 1) {
    pos++;
    pageParam[PAGE_FILE][6].value = pos;
    drawValue(6);
  }
  drawExtraValue();
}

void finalizePresetName() {
  int end = PN_LEN - 1;
  while (end >= 0 && (presetEditName[end] == ' ' || presetEditName[end] == '_')) {
    end--;
  }
  presetEditName[end + 1] = '\0';
}
