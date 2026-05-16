// Minimal MPU6050 reader -> Serial (115200) as CSV:
// micros,ax,ay,az,gx,gy,gz,tempC
//
// I2C: A4=SDA, A5=SCL. Serial goes out USB AND HC-05 (TX1/RX0 are shared).

#include <Wire.h>

static const uint8_t MPU = 0x68;  // AD0 low

static void wr(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(MPU);
  Wire.write(reg); Wire.write(val);
  Wire.endTransmission();
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  Wire.setClock(400000);
  wr(0x6B, 0x00);  // PWR_MGMT_1: wake
  wr(0x1B, 0x10);  // GYRO_CONFIG  ±1000 dps  (mouse use; raise to 0x18 for ±2000)
  wr(0x1C, 0x00);  // ACCEL_CONFIG ±2 g
  wr(0x1A, 0x03);  // CONFIG: DLPF ~44 Hz
  delay(50);
  Serial.println(F("# t_us,ax,ay,az,gx,gy,gz,tC"));
}

void loop() {
  Wire.beginTransmission(MPU);
  Wire.write(0x3B);
  if (Wire.endTransmission(false) != 0) return;
  Wire.requestFrom((int)MPU, 14, (int)true);
  if (Wire.available() < 14) return;

  int16_t ax = (Wire.read() << 8) | Wire.read();
  int16_t ay = (Wire.read() << 8) | Wire.read();
  int16_t az = (Wire.read() << 8) | Wire.read();
  int16_t t  = (Wire.read() << 8) | Wire.read();
  int16_t gx = (Wire.read() << 8) | Wire.read();
  int16_t gy = (Wire.read() << 8) | Wire.read();
  int16_t gz = (Wire.read() << 8) | Wire.read();

  float tC = t / 340.0f + 36.53f;

  Serial.print(micros());     Serial.print(',');
  Serial.print(ax); Serial.print(','); Serial.print(ay); Serial.print(','); Serial.print(az); Serial.print(',');
  Serial.print(gx); Serial.print(','); Serial.print(gy); Serial.print(','); Serial.print(gz); Serial.print(',');
  Serial.println(tC, 2);

  delay(10);  // ~100 Hz
}
