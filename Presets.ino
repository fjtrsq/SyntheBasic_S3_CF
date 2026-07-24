String presetPathFromName(const char* rawName) {
  char trimmed[PN_LEN + 1];
  strncpy(trimmed, rawName, PN_LEN);
  trimmed[PN_LEN] = '\0';

  int end = PN_LEN - 1;
  while (end >= 0 && trimmed[end] == ' ') end--;
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
  snprintf(path, sizeof(path), "/preset_%s.bin", safeName);
  return String(path);
}

bool presetSaveByName(const char* name) {
  StoredPreset preset = {};
  
  strncpy(preset.name, name, PN_LEN);
  preset.name[PN_LEN] = '\0';

  for (uint8_t page = 0; page < SOUND_PARAM_PAGES; page++) {
    for (uint8_t param = 0; param < PARAMS_PER_PAGE; param++) {
      preset.values[page][param] = synthValue[page][param];
    }
  }
  for (uint8_t osc = 0; osc < N_OSC; osc++) {
    for (uint8_t param = 0; param < TOTAL_ADSR; param++) {
      preset.oscAdsr[osc][param] = ADSRvalues[osc][param];
    }
  }

  fs::File file = LittleFS.open(presetPathFromName(name), "w");
  if (!file) return false;
  size_t written = file.write((uint8_t*)&preset, sizeof(preset));
  file.close();
  return written == sizeof(preset);
}

bool presetLoadByName(const char* name) {
  fs::File file = LittleFS.open(presetPathFromName(name), "r");
  if (!file) return false;

  StoredPreset preset = {};
  size_t readBytes = file.read((uint8_t*)&preset, sizeof(preset));
  file.close();

  
  if (readBytes != sizeof(preset)) return false;

  strncpy(presetEditName, preset.name, PN_LEN);
  presetEditName[PN_LEN] = '\0';

  suppressUiRefresh = true;
  for (uint8_t page = 0; page < SOUND_PARAM_PAGES; page++) {
    for (uint8_t param = 0; param < PARAMS_PER_PAGE; param++) {
      synthValue[page][param] = preset.values[page][param];
      refreshValue((Page)page, param, 0);
    }
  }
  suppressUiRefresh = false;

  for (uint8_t osc = 0; osc < N_OSC; osc++) {
    for (uint8_t param = 0; param < TOTAL_ADSR; param++) {
      ADSRvalues[osc][param] = preset.oscAdsr[osc][param];
    }
    updateEnvelopeRates(osc);
  }

  return true;
}

bool presetDeleteByName(const char* name) {
  String path = presetPathFromName(name);
  if (!LittleFS.exists(path)) return false;
  return LittleFS.remove(path);
}

bool presetNameFromPath(const char* path, char* outName) {
  if (!path || !outName) return false;
  const char* prefix = "preset_";
  const char* suffix = ".bin";
  if (path[0] == '/') path++;
  size_t pathLen = strlen(path);
  size_t prefixLen = strlen(prefix);
  size_t suffixLen = strlen(suffix);
  if (pathLen <= (prefixLen + suffixLen)) return false;
  if (strncmp(path, prefix, prefixLen) != 0) return false;
  if (strcmp(path + pathLen - suffixLen, suffix) != 0) return false;

  size_t nameLen = pathLen - prefixLen - suffixLen;
  if (nameLen > PN_LEN) nameLen = PN_LEN;
  for (size_t i = 0; i < nameLen; i++) {
    char c = path[prefixLen + i];
    outName[i] = (c == '_') ? ' ' : c;
  }
  outName[nameLen] = '\0';
  return true;
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
}

void refreshPresetFileList(bool keepCurrentSelection) {
  char previousName[PN_LEN + 1];
  strncpy(previousName, presetEditName, PN_LEN);
  previousName[PN_LEN] = '\0';

  presetFileCount = 0;
  fs::File root = LittleFS.open("/");
  if (!root || !root.isDirectory()) {
    filePageParams[0].min = 0;
    filePageParams[0].max = 0;
    filePageParams[0].value = 0;
    presetSelectFileByIndex(0);
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
    filePageParams[0].min = 0;
    filePageParams[0].max = 0;
    filePageParams[0].value = 0;
    presetSelectFileByIndex(0);
    return;
  }

  filePageParams[0].min = 0;
  filePageParams[0].max = presetFileCount - 1;

  int selectedIndex = (int)roundf(filePageParams[0].value);
  if (keepCurrentSelection) {
    selectedIndex = -1;
    for (uint8_t i = 0; i < presetFileCount; i++) {
      if (strncmp(presetFileNames[i], previousName, PN_LEN) == 0) {
        selectedIndex = i;
        break;
      }
    }
    if (selectedIndex < 0) selectedIndex = constrain(filePageParams[0].value, 0, presetFileCount - 1);
  }
  else {
    selectedIndex = constrain(filePageParams[0].value, 0, presetFileCount - 1);
  }

  filePageParams[0].value = selectedIndex;
  presetSelectFileByIndex(selectedIndex);
}


void setPresetActionFeedback(uint8_t param, const char* label, uint16_t ms = 1200) {
  presetActionParam = (int8_t)param;
  strncpy(presetActionLabel, label, sizeof(presetActionLabel) - 1);
  presetActionLabel[sizeof(presetActionLabel) - 1] = '\0';
  presetActionUntil = millis() + ms;
}

void presetInsertSelectedChar(bool autoAdvance) {
  int pos = constrain(filePageParams[6].value, 0, PN_LEN - 1);
  int charIdx = constrain(filePageParams[7].value, 0, strlen(PRESET_CHARS) - 1);
  presetEditName[pos] = PRESET_CHARS[charIdx];

  if (autoAdvance && pos < PN_LEN - 1) {
    pos++;
    filePageParams[6].value = pos;
    drawValue(6);
  }
  drawExtraValue();
}

void finalizePresetName() {
  int end = PN_LEN - 1;
  while (end >= 0 && presetEditName[end] == ' ') {
    end--;
  }
  presetEditName[end + 1] = '\0';
}
