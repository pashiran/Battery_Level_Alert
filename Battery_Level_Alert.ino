/*전압 측정: readBatteryVoltage() 함수에서 ADC 값을 읽고 전압을 계산.
전압 단계 확인: getVoltageStage() 함수에서 5단계 범위로 구분.
부저 동작: beep() 함수에서 단계별 패턴으로 소리냄.
절전 모드: enterSleepMode() 함수로 3분간 절전 후 깨어남.
5단계(15V 미만)에서는 절전 없이 부저 연속 울림.


- 테스트 및 조정 방법
전압 측정 정확성 확인

Serial.print()로 측정된 전압 출력.
실제 배터리 전압과 비교하여 오차가 있으면 R1, R2 값을 조정.
부저 동작 테스트

beep() 함수 내 delay(300) 값을 조정하여 부저 소리 길이 변경.
절전 모드 동작 확인

delay(100)을 delay(180000) (3분)으로 변경하여 실제 절전 시간 반영.
*/

#include <avr/sleep.h>

#define BUZZER_PIN 5   // 부저 출력 핀
#define VOLTAGE_PIN A0 // 배터리 전압 측정 핀

// 전압 분배 저항값 (예시: 100kΩ / 20kΩ 사용 시)
#define R1 100.0  // kΩ
#define R2 20.0   // kΩ
#define VREF 5.0  // 아두이노의 VCC (기준 전압)

// 10비트 ADC 해상도 (0~1023)
#define ADC_MAX 1023.0 

// 전압 단계 임계값 (5셀 배터리 기준)
const float THRESHOLDS[5] = {18.9, 17.9, 16.15, 15.0};

// 현재 전압 상태 저장
int prevStage = -1;

// 부저 울림 함수
void beep(int count) {
    for (int i = 0; i < count; i++) {
        digitalWrite(BUZZER_PIN, HIGH);
        delay(300);
        digitalWrite(BUZZER_PIN, LOW);
        delay(300);
    }
}

// 배터리 전압 측정 함수
float readBatteryVoltage() {
    int adcValue = analogRead(VOLTAGE_PIN);
    float measuredVoltage = (adcValue / ADC_MAX) * VREF;
    float batteryVoltage = measuredVoltage * ((R1 + R2) / R2);
    return batteryVoltage;
}

// 절전 모드 진입 함수
void enterSleepMode() {
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sleep_enable();
    sleep_cpu(); // 절전 모드 진입
}

// 전압 단계 판별 함수
int getVoltageStage(float voltage) {
    for (int i = 0; i < 4; i++) {
        if (voltage >= THRESHOLDS[i]) return i + 1;
    }
    return 5; // 15V 미만 (5단계)
}

void setup() {
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(VOLTAGE_PIN, INPUT);
    Serial.begin(9600);

    float voltage = readBatteryVoltage();
    int stage = getVoltageStage(voltage);
    prevStage = stage;
    
    // 초기 부저 동작
    if (stage < 5) beep(stage);
}

void loop() {
    float voltage = readBatteryVoltage();
    int stage = getVoltageStage(voltage);

    Serial.print("현재 전압: ");
    Serial.print(voltage);
    Serial.print("V, 단계: ");
    Serial.println(stage);

    if (stage != prevStage) {
        prevStage = stage;

        if (stage < 5) {
            beep(stage); // 새 전압 단계에 맞춰 부저 울림
            delay(100);
            enterSleepMode(); // 절전 모드 진입
        } else {
            // 5단계에서는 절전 없이 연속 부저
            while (true) {
                digitalWrite(BUZZER_PIN, HIGH);
                delay(500);
                digitalWrite(BUZZER_PIN, LOW);
                delay(500);
            }
        }
    } else {
        delay(100);
        enterSleepMode(); // 전압 변화 없으면 절전 모드
    }
}
