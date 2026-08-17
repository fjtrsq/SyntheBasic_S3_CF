#include <Encoder_FJ.h>

#define enc_A  11
#define enc_B  10

Encoder_FJ encoder = Encoder_FJ(enc_A, enc_B);

int var = 0;
int8_t enc = 0;

void setup() {
  Serial.begin(115200);
  encoder.begin();
}

void loop() {
  enc = encoder.read();
  if(enc){
    var += enc;
    Serial.println(var);
  }
}
