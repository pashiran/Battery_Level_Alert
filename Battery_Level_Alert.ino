/*전압 측정: readBatteryVoltage() 함수에서 ADC 값을 읽고 전압을 계산.
전압 단계 확인: getVoltageStage() 함수에서 5단계 범위로 구분.
부저 동작: beep() 함수에서 단계별 패턴으로 소리냄.
절전 모드: enterSleepMode() 함수로 3분간 절전 후 깨어남.
5단계(15V 미만)에서는 절전 없이 부저 연속 울림.
*/

#include <avr/sleep.h>     // 절전 모드 설정을 위한 라이브러리
#include <avr/power.h>     // 주변 장치 전원 제어 라이브러리
#include <avr/wdt.h>       // 워치독 타이머(WDT) 사용을 위한 라이브러리

// 핀 설정
#define BUZZER_PIN 5       // 부저 출력 핀 (디지털 5번 핀)
#define VOLTAGE_PIN A0     // 배터리 전압 측정용 아날로그 입력 핀

// 전압 분배 저항 값 설정 (단위: kΩ)
// 전압 분배 회로: 배터리 → R1 → R2 → GND, R2와 GND 사이 전압을 읽음
#define R1 100.0
#define R2 20.0

// 기준 전압 (LM4040-2.5V 사용 기준)
#define VREF 2.5           // 외부 기준 전압 (단위: V)

// ADC 최대값 (10비트 분해능: 0~1023)
#define ADC_MAX 1023.0

// 전압 단계 기준값 배열 (상위 단계부터 순차적으로)
const float THRESHOLDS[5] = {18.9, 17.9, 16.15, 15.0};

// 이전 전압 단계 기억용 변수 (변화를 감지하기 위해 사용)
int prevStage = -1;

// 슬립 모드에서 깨어날 때 사용되는 플래그 (인터럽트에서 변경됨)
volatile bool wakeUpFlag = false;

//
// 부저를 울리는 함수
// count: 울릴 횟수 (예: 2단계면 2번 울림)
//
void beep(int count) {
    for (int i = 0; i < count; i++) {
        digitalWrite(BUZZER_PIN, HIGH); // 부저 ON
        delay(300);
        digitalWrite(BUZZER_PIN, LOW);  // 부저 OFF
        delay(300);
    }
}

//
// 배터리 전압을 측정하는 함수
// - 아날로그 핀에서 측정된 전압을 바탕으로 원래 배터리 전압을 계산
//
float readBatteryVoltage() {
    int adcValue = analogRead(VOLTAGE_PIN); // ADC로 전압 측정
    float measuredVoltage = (adcValue / ADC_MAX) * VREF; // 핀에서 측정된 전압
    float batteryVoltage = measuredVoltage * ((R1 + R2) / R2); // 실제 배터리 전압 복원
    return batteryVoltage;
}

//
// 배터리 전압을 기준으로 현재 단계를 판별하는 함수
// 반환값: 1~5단계 (1단계가 가장 높고, 5단계는 15V 미만)
//
int getVoltageStage(float voltage) {
    for (int i = 0; i < 4; i++) {
        if (voltage >= THRESHOLDS[i]) return i + 1;
    }
    return 5;
}

//
// 절전 모드 진입 함수 (SLEEP_MODE_PWR_DOWN 사용)
// - 모든 불필요한 주변장치 전원을 차단하여 전력 소모 최소화
// - 슬립 후 WDT 인터럽트로 다시 깨어남
//
void enterSleepMode() {
    // 절전을 위해 불필요한 주변장치 OFF
    ADCSRA &= ~(1 << ADEN);       // ADC OFF
    power_adc_disable();
    power_spi_disable();
    power_timer0_disable();
    power_timer1_disable();
    power_timer2_disable();
    power_twi_disable();

    set_sleep_mode(SLEEP_MODE_PWR_DOWN); // 가장 전력 소모가 적은 모드 설정
    sleep_enable();       // 슬립 활성화
    sleep_cpu();          // 슬립 모드 진입 (여기서 대기 상태로 진입)

    // 슬립에서 깨어나면 이 아래 코드 실행
    sleep_disable();      // 슬립 모드 비활성화 (다시 슬립 방지)
    
    // 주변 장치 전원 복구
    power_all_enable();
    ADCSRA |= (1 << ADEN); // ADC 다시 ON
}

//
// 약 3분(176초) 동안 슬립을 반복하는 함수
// - WDT는 최대 8초만 슬립 가능하므로 이를 22회 반복함
//
void sleepFor3Minutes() {
    for (int i = 0; i < 22; i++) {
        wdt_reset(); // WDT 리셋

        // WDT 설정: 8초 슬립 후 인터럽트 발생
        WDTCSR = (1 << WDIE) | (1 << WDP3) | (1 << WDP0); // 8초

        enterSleepMode(); // 슬립 진입 → WDT 인터럽트로 자동 깨어남
    }
}

// 워치독 타이머 인터럽트 핸들러
// 슬립에서 깨우기만 하고, 실제 처리 코드는 없음
ISR(WDT_vect) {
    wakeUpFlag = true; // 슬립 깼다는 플래그 설정
}

//
// 초기 설정 함수 (1회 실행)
// - 부저/입력 핀 설정, 초기 전압 읽고 부저 울림
//
void setup() {
    pinMode(BUZZER_PIN, OUTPUT); // 부저 핀 출력 설정
    pinMode(VOLTAGE_PIN, INPUT); // 전압 측정 핀 입력 설정
    Serial.begin(9600);          // 디버깅용 시리얼 출력

    // 워치독 타이머 초기화 (켜진 상태일 수 있으므로 반드시 초기화)
    MCUSR &= ~(1 << WDRF); // 리셋 플래그 제거
    WDTCSR |= (1 << WDCE) | (1 << WDE); // 설정 변경 허용
    WDTCSR = 0x00; // WDT 완전히 비활성화

    float voltage = readBatteryVoltage();       // 초기 전압 측정
    int stage = getVoltageStage(voltage);       // 단계 판별
    prevStage = stage;                          // 이전 단계 기록

    if (stage < 5) beep(stage);                 // 부저 단계에 맞춰 울림
}

//
// 메인 루프 함수
// - 3분마다 전압 측정 후 단계 변화 감지
// - 변화 있으면 부저 울림, 없으면 다시 슬립
//
void loop() {
    float voltage = readBatteryVoltage();   // 전압 측정
    int stage = getVoltageStage(voltage);   // 단계 계산

    // 디버깅용 출력
    Serial.print("현재 전압: ");
    Serial.print(voltage);
