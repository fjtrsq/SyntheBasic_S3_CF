// ============================================================================
// CUSTOM ARPEGGIO EDITOR - Modo de edición de arpegio personalizado
// ============================================================================


void initCustomArpPattern() {
  customArpLength = 8;
  customArpPattern[0] = 0;
  customArpPattern[1] = 2;
  customArpPattern[2] = 4;
  customArpPattern[3] = 1;
  customArpPattern[4] = 3;
  customArpPattern[5] = 0;
  customArpPattern[6] = 2;
  customArpPattern[7] = 1;
  
  for (int i = 8; i < C_ARP_MAX_STEPS; i++) {
    customArpPattern[i] = 0;
  }
}

void enterCustomArpEditor() {
  if (customArpEditorState != CUSTOM_ARP_IDLE) return;
  if (sequencerState != SEQ_STATE_OFF) return;
  
  if (currentPage != PAGE_ARP) setPage(PAGE_ARP);
  arpEnabled = true; pageParam[6][0].value = 1;
  arpMode = ARP_CUSTOM; pageParam[6][2].value = ARP_CUSTOM;
  customArpEditorState = CUSTOM_ARP_EDIT;
  customArpEditStep = 0;
  
  // Sincronizar parámetros
  arpCustom[0].value = customArpEditStep;
  arpCustom[1].value = customArpEditStep;
  arpCustom[2].value = customArpLength;
  arpCustom[3].value = (int)(arpRateHz * 10);
  arpCustom[4].value = arpOctaves;
  arpCustom[5].value = (int)(arpGate * 100);
  arpCustom[6].value = (int)(arpSwing * 100);
  Serial.println("[CUSTOM_ARP] Entrando en editor");
  drawCustomArpEditor();
}

void exitCustomArpEditor() {
  if (customArpEditorState != CUSTOM_ARP_EDIT) return;
  
  customArpEditorState = CUSTOM_ARP_IDLE;
   // Sincronizar parámetros
  pageParam[6][1].value = arpCustom[3].value;
  pageParam[6][3].value = arpCustom[4].value;
  pageParam[6][4].value = arpCustom[5].value;
  pageParam[6][6].value = arpCustom[6].value;
  
  Serial.print("[CUSTOM_ARP] Patrón guardado (longitud ");
  Serial.println(customArpLength);
  Serial.print("[CUSTOM_ARP] Patrón: ");
  for (int i = 0; i < customArpLength; i++) {
    Serial.printf("%d ", customArpPattern[i]);
  }
  Serial.println();
  
  drawUI();
}

void resetCustomArpPattern() {
  initCustomArpPattern();
  customArpEditStep = 0;
  arpCustom[0].value = 0;
  Serial.println("[CUSTOM_ARP] Reseteado");
  drawCustomArpEditor();
}

void drawCustomArpEditor() {
  tft.fillScreen(TFT_BLACK);
  
  tft.setFreeFont(LAB_TEXT);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(TFT_GREEN);

  for(byte i=0;i<MAX_CUSTOM_PARAM;i++) tft.drawString(arpCustom[i].name, 40 + ((i&3)*80), 5 + ((i>>2)*60));

  for(byte i=0;i<MAX_CUSTOM_PARAM;i++) drawCustomValue(i);

  drawCustomArpPattern();
  
  drawSqrBots("", "RST", "EXIT", 100);
}

void processCustomArpEditor(uint8_t param, int dirAcc) {
  if (customArpEditorState != CUSTOM_ARP_EDIT) return;

  arpCustom[param].value = constrain(arpCustom[param].value + dirAcc,
                arpCustom[param].min, arpCustom[param].max);

  const int value = arpCustom[param].value;
  switch(param) {
    case 0:
      customArpEditStep = value;
      drawCustomArpPattern();
      drawCustomValue(1);
      Serial.printf("[CUSTOM_ARP] Paso: %d\n", value);
      break;

    case 1:
      if (customArpEditStep < customArpLength) {
        customArpPattern[customArpEditStep] = value;
        drawCustomArpStepBox(customArpEditStep);
        Serial.printf("[CUSTOM_ARP] Paso %d -> Índice %d\n", customArpEditStep, value);
      }
      break;

    case 2:
      customArpLength = value;
      drawCustomArpPattern();
      break;

    case 3:
      arpRateHz = value  * 0.1f;
      pageParam[6][1].value = value;
      break;

    case 4:
      arpOctaves = value;
      pageParam[6][3].value = value;
      break;

    case 5:
      arpGate = value * 0.01f;
      pageParam[6][4].value = value;
      break;

    case 6:
      arpSwing = value * 0.01;
      pageParam[6][6].value = value;
      break;

    default:
      return;
      break;
  }
  drawCustomValue(param);
}

void drawCustomValue(uint8_t p){
  tft.setFreeFont(NUM_TEXT);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(TFT_WHITE);
  
  char buf[12];
  const char* textValue = nullptr;

  switch(p){
    case 0: snprintf(buf, sizeof(buf), "%d", customArpEditStep + 1);
      break;

    case 1: 
      if(customArpEditStep < customArpLength){
        if(customArpPattern[customArpEditStep] == -1) snprintf(buf, sizeof(buf), "REST");
        else if(customArpPattern[customArpEditStep] == -2) snprintf(buf, sizeof(buf), "LIG");
        else snprintf(buf, sizeof(buf), "%d", customArpPattern[customArpEditStep] + 1);
      }   
      else  snprintf(buf, sizeof(buf), "--");
       break;

    case 2: snprintf(buf, sizeof(buf), "%d", customArpLength);
       break;

    case 3: snprintf(buf, sizeof(buf), "%d", pageParam[6][1].value);
       break; 

    case 4: snprintf(buf, sizeof(buf), "%d", pageParam[6][3].value);
       break;

    case 5: snprintf(buf, sizeof(buf), "%d", pageParam[6][4].value);
       break;

    case 6: snprintf(buf, sizeof(buf), "%d", pageParam[6][6].value);
       break;

  } 
  
  textValue = buf;
  tft.fillRect((p&3)*80, 28 + ((p>>2)*60), 80, 22, TFT_BLACK);
  tft.drawString(textValue, 40 + ((p&3)*80), 30 + ((p>>2)*60));

}

void drawCustomArpPattern() {
  for (int i = 0; i < C_ARP_MAX_STEPS; i++)  drawCustomArpStepBox(i);
}
    
void drawCustomArpStepBox(int stepIdx) {
  int x = 15 + ((stepIdx & 7) * 35);
  int y = 140 + ((stepIdx >> 3) * 35);

  bool isActive = (stepIdx < customArpLength);
  bool isSelected = (stepIdx == customArpEditStep);
  
  uint16_t color = TFT_DARKGREY;
  
  if (isSelected && isActive) {
    color = TFT_YELLOW;
  } else if (isSelected && !isActive) {
    color = TFT_RED;
  } else if (isActive) {
    color = TFT_GREEN;
  }
  
  // Dibujar borde
  tft.fillRect(x, y, 30, 30, TFT_BLACK);
  tft.drawRect(x, y, 30, 30, color);
  
  // Dibujar contenido
  tft.setFreeFont(NUM_TEXT);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(color);
  
  int8_t val = customArpPattern[stepIdx];
  char buf[5];
  switch (val) {
    case -2:
      snprintf(buf, sizeof(buf), "^");  // Ligadura / Sustenido
      break;
    case -1:
      snprintf(buf, sizeof(buf), "X");  // Silencio / Rest
      break;
    default:
      if (isActive) {
        snprintf(buf, sizeof(buf), "%d", customArpPattern[stepIdx] + 1);
      } else {
        snprintf(buf, sizeof(buf), "--");
      }
      break;
  }

  tft.drawString(buf, x + 15, y + 15);
}
    
uint8_t getCustomArpIndex(int step, int total) {
  if (customArpLength == 0 || arpHeldCount == 0) return 0;
  int patternPos = step % customArpLength;
  int rawVal = customArpPattern[patternPos];

  // 🔹 Manejo de valores negativos para funciones especiales
  if (rawVal == -1) return ARP_REST; // Devuelve 255
  if (rawVal == -2) return ARP_TIE;  // Devuelve 254

  // Comportamiento normal para índices de nota (0, 1, 2...)
  if (rawVal >= total) { //arpHeldCount
    rawVal = rawVal % total;
  }

  return (uint8_t)rawVal;
}