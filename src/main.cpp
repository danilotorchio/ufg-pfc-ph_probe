#include <heltec.h>

#include "driver/adc.h"
#include "nvs_flash.h"

#define LORA_BAND 915E6        // 915 MHz
#define TIME_TO_SLEEP 5        // 15 Seconds
#define uS_TO_S_FACTOR 1000000 // uS to S factor conversion
#define PROBE_GPIO GPIO_NUM_4  // GPIO4 - ADC2_0

// Variáveis de controle
bool jobDone = false;
float phValue = 0.0;

// Declaração das funções
void print_wakeup_reason();
void setup_storage();
void setup_loRa();

// Declaração das tarefas do FreeRTOS
void task_read_probe(void *pvParams);
void task_send_data(void *pvParams);

// Método chamado na inicialização do programa no microcontrolador
void setup()
{
    // Display = false |  LoRa = true | Serial = true
    Heltec.begin(false, true, true, true, LORA_BAND);
    delay(20);

    // Escreve a fonte do despertador
    print_wakeup_reason();

    // Configura o timer como fonte do despertador
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    delay(20);

    // Inicializa o conversor A/D no canal 0
    adc2_config_channel_atten(ADC2_CHANNEL_0, ADC_ATTEN_DB_11);
    // setup_storage();
    setup_loRa();

    // Initialize control variables
    jobDone = false;
    phValue = 0.0;

    // Initial tasks
    delay(20);
    xTaskCreate(task_read_probe, "task_read_probe", 2048, NULL, 3, NULL);
}

// Método chamado após a inicialização do programa no microcontrolador
void loop()
{
    while (1)
    {
        Serial.printf("Job done: %d \n", jobDone);

        if (jobDone)
        {
            Serial.println("Entering on sleep mode...");

            // End LoRa transmission and put on sleep mode
            LoRa.end();
            LoRa.sleep();

            // Enter on deep sleep mode (<= 800uA)
            esp_deep_sleep_start();
        }

        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}

/*** Implementação das funções ***/
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

void setup_storage()
{
    esp_err_t ret;

    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Load configuration here
}

void setup_loRa()
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

/*** Implementação das tarefas do FreeRTOS ***/
void task_read_probe(void *pvParams)
{
    (void)pvParams;

    int value = 0;

    float avg = 0.0;
    float voltage = 0.0;

    for (int i = 0; i < 20; i++)
    {
        adc2_get_raw(ADC2_CHANNEL_0, ADC_WIDTH_12Bit, &value);

        avg += value;
        value = 0;

        vTaskDelay(50 / portTICK_PERIOD_MS);
    }

    voltage = (avg / 10) * (3.3 / 4095);
    phValue = (-8.29 * voltage) + 21.16;

    vTaskDelay(5000 / portTICK_PERIOD_MS);
    xTaskCreate(task_send_data, "task_send_data", 4096, NULL, 2, NULL);

    vTaskDelay(10 / portTICK_PERIOD_MS);
    vTaskDelete(NULL);
}

void task_send_data(void *pvParams)
{
    char payload[10];
    sprintf(payload, "%.2f;%.1f", phValue, 25.0);

    Serial.printf("Sending pH value: %s\n", payload);

    LoRa.beginPacket();
    LoRa.print(payload);
    LoRa.endPacket();

    Serial.println("Sending done!");

    jobDone = true;
    vTaskDelete(NULL);
}
/*************************/