#include <Arduino.h>
#include <heltec.h>

#include "tasks.h"
#include "helpers.h"

#define LORA_BAND 915E6        // 915 MHz
#define TIME_TO_SLEEP 15       // 15 Seconds
#define uS_TO_S_FACTOR 1000000 // uS to S factor conversion

// General functions declarations
void print_wakeup_reason();
void setupLoRa();

// Tasks handler
TaskHandle_t TasksHandler = NULL;

// Control variables
bool jobDone = false;
float phValue = 0.0;

void setup()
{
    // Initialize UART and LoRa
    Heltec.begin(false, true, true, true, LORA_BAND);
    print_wakeup_reason();

    // Setup the timer as wake up source
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    delay(10);

    // Initialize peripherals
    setupLoRa();

    // Initialize control variables
    jobDone = false;
    phValue = 0.0;

    // Create tasks
    xTaskCreate(TaskReadProbe, "TaskReadProbe", 2048, NULL, 2, &TasksHandler);
    xTaskCreate(TaskSendData, "TaskSendData", 2048, NULL, 2, &TasksHandler);

    Serial.println("All settings done!");
}

void loop()
{
    while (1)
    {
        if (jobDone)
        {
            Serial.println("Entering on sleep mode...");

            // End LoRa transmission and put on sleep mode
            LoRa.end();
            LoRa.sleep();

            // Enter on deep sleep mode (<= 800uA)
            esp_deep_sleep_start();
        }

        delay(1000);
    }
}

/*** General Functions ***/
void print_wakeup_reason()
{
    esp_sleep_wakeup_cause_t wakeup_reason;
    wakeup_reason = esp_sleep_get_wakeup_cause();

    switch (wakeup_reason)
    {
    case ESP_SLEEP_WAKEUP_EXT0:
        Serial.println("Wakeup caused by external signal using RTC_IO");
        break;
    case ESP_SLEEP_WAKEUP_EXT1:
        Serial.println("Wakeup caused by external signal using RTC_CNTL");
        break;
    case ESP_SLEEP_WAKEUP_TIMER:
        Serial.println("Wakeup caused by timer");
        break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
        Serial.println("Wakeup caused by touchpad");
        break;
    case ESP_SLEEP_WAKEUP_ULP:
        Serial.println("Wakeup caused by ULP program");
        break;
    default:
        Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason);
        break;
    }
}

void setupLoRa()
{
    Serial.println("Setting LoRa...");

    LoRa.setSpreadingFactor(12);    // Fator de espalhamento
    LoRa.setSignalBandwidth(250E3); // Largura de banda
    LoRa.setCodingRate4(5);         // Codding rate
    LoRa.setPreambleLength(6);      // Comprimento do preâmbulo
    LoRa.setSyncWord(0x12);         // Palavra de sincronização
    LoRa.crc();

    Serial.println("LoRa done!");
}
/*************************/

/*** Tasks implementations ***/
void TaskReadProbe(void *pvParams)
{
    (void)pvParams;

    // seeds the generator
    srand(time(0));
    phValue = randomFloat(0, 14);

    Serial.println("Reading pH probe...");
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    xTaskNotifyGive(TasksHandler);

    delay(100);
    vTaskDelete(NULL);
}

void TaskSendData(void *pvParams)
{
    (void)pvParams;
    ulTaskNotifyTake(pdFALSE, portMAX_DELAY);

    Serial.printf("Sending pH value: %f\n", phValue);

    LoRa.beginPacket();
    LoRa.print(phValue);
    LoRa.endPacket();

    Serial.println("Sending done!");

    jobDone = true;
    vTaskDelete(NULL);
}
/*****************************/