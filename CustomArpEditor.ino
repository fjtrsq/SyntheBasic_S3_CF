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
  arpEnabled = true;
  arpMode = ARP_CUSTOM;
  customArpEditorState = CUSTOM_ARP_EDIT;
  customArpEditStep = 0;
  
  Serial.println("[CUSTOM_ARP] Entrando en editor");
  drawCustomArpEditor();
}

void exitCustomArpEditor() {
  if (customArpEditorState != CUSTOM_ARP_EDIT) return;
  
  customArpEditorState = CUSTOM_ARP_IDLE;
  
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
  Serial.println("[CUSTOM_ARP] Reseteado");
  drawCustomArpEditor();
}

void drawCustomArpEditor() {
  
  tft.setFreeFont(LAB_TEXT);
  tft.setTextDatum(TC_DATUM);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_GREEN);
  for(byte i=0;i<4;i++) tft.drawString(arpCustom[i].name, 40 + ((i&3)*80), 5 + ((i>>2)*60));

  for(byte i=0;i<4;i++) drawCustomValue(i);

  for (int i = 0; i < C_ARP_MAX_STEPS; i++)  drawCustomArpStepBox(i);
  
  drawSqrBots("RST", "EXT", 100);
}
void processCustomArpEditor(uint8_t param, int dirAcc) {
  arpCustom[param].value = constrain(arpCustom[param].value + dirAcc,
                arpCustom[param].min, arpCustom[param].max);
    const int value = arpCustom[param].value;
  switch(param) {
    case 0:
      customArpEditStep = value;
      drawCustomValue(1);
      break;

    case 1:
      if (customArpEditStep < customArpLength) {
        customArpPattern[customArpEditStep] = value;
      }
      break;

    case 2:
      customArpLength = value;
      break;

    case 3:
      arpRateHz = value  * 0.1f;
      pageParam[6][1].value = value;
      break;

    default:
      return;
      break;
  }
  drawCustomValue(p); 
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
    case 1: (customArpEditStep < customArpLength) ? 
      snprintf(buf, sizeof(buf), "%d", customArpPattern[customArpEditStep] + 1) :
      snprintf(buf, sizeof(buf), "--");
       break;
    case 2: snprintf(buf, sizeof(buf), "%d", customArpLength);
       break;
    case 3: snprintf(buf, sizeof(buf), "%d", pageParam[6][1].value);
       break; 
  } 
  
  textValue = buf;
  tft.fillRect((p&3)*80, 28 + ((p>>2)*60), 80, 22, TFT_BLACK);
  tft.drawString(textValue, 40 + ((p&3)*80), 30 + ((p>>2)*60));
  
  if(p < 3) drawCustomArpStepBox(customArpEditStep, customArpPattern[customArpEditStep]); 
  
  
}
    
void drawCustomArpStepBox(int step, int Idx) {
  int x = 15 + ((step&7) * 35);
  int y = 140 + ((step>>3) * 35);

  bool isActive = (Idx < customArpLength);
  bool isSelected = (Idx == customArpEditStep);
  
  uint16_t color = TFT_DARKGREY;
  
  if (isSelected && isActive) {
    color = TFT_YELLOW;
  } else if (isSelected && !isActive) {
    color = TFT_RED;
  } else if (isActive) {
    color = TFT_GREEN;
  }
  
  // Dibujar borde
  tft.drawRect(x, y, 30, 30, color);
  
  // Dibujar contenido
  tft.setFreeFont(NUM_TEXT);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(color);
  
  char buf[5];
  if (isActive) {
    snprintf(buf, sizeof(buf), "%d", customArpPattern[customArpEditStep] + 1);
  } else {
    snprintf(buf, sizeof(buf), "--");
  }
  tft.drawString(buf, x + 15, y + 15);
}
    
uint8_t getCustomArpIndex(int step) {
  if (customArpLength == 0 || arpHeldCount == 0) return 0;
  int patternPos = step % customArpLength;
  uint8_t indexInAcorde = customArpPattern[patternPos];
  if (indexInAcorde >= arpHeldCount) {
    indexInAcorde = indexInAcorde % arpHeldCount;
  }
  return indexInAcorde;
}