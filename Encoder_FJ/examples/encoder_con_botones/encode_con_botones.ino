#include <Encoder_FJ.h>

#define enc_A  10
#define enc_B  11

Encoder_FJ encoder = Encoder_FJ(enc_A, enc_B);

#define boton_enc  5
#define boton_act  25
int8_t enc;
int8_t muestra;


void setup() {
  pinMode(boton_enc, INPUT);
  pinMode(boton_act,INPUT_PULLUP);

  Serial.begin(115200);
  encoder.begin();

}

void loop() {
  if (!digitalRead(boton_enc)){
    Serial.println("Boton_ENC");
    delay(100);
    while(!digitalRead(boton_enc)){}
  }
  if (!digitalRead(boton_act)){
    Serial.println("Boton_ACT");
    delay(100);
    while(!digitalRead(boton_act)){}
  }
  enc = encoder.read();
  if (enc){
    muestra += enc;
    Serial.println(muestra);
  }

}
