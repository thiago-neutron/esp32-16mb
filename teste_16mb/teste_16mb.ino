// Prova, do lado do firmware, que os 16MB estao visiveis.
// Arduino IDE > Tools:  Board = ESP32 Dev Module | Flash Size = 16MB (128Mb)
//                       Partition Scheme = Custom   (usa o partitions.csv desta pasta)
#include <esp_partition.h>
#include <esp_flash.h>

void setup() {
  Serial.begin(115200);
  delay(1500);

  uint32_t tamanho = 0;
  esp_flash_get_size(NULL, &tamanho);

  Serial.println("\n===== ESP32 — flash trocada 4MB -> 16MB =====");
  Serial.printf("Flash detectada .... %u MB\n", tamanho / (1024 * 1024));
  Serial.printf("Sketch usa ......... %u KB de %u KB da particao do app\n",
                ESP.getSketchSize() / 1024,
                (ESP.getSketchSize() + ESP.getFreeSketchSpace()) / 1024);

  Serial.println("\nParticoes:");
  esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_ANY,
                                                   ESP_PARTITION_SUBTYPE_ANY, NULL);
  while (it) {
    const esp_partition_t *p = esp_partition_get(it);
    Serial.printf("  %-10s  0x%06X  %6u KB\n", p->label, p->address, p->size / 1024);
    it = esp_partition_next(it);
  }
  esp_partition_iterator_release(it);
  Serial.println("=============================================");
}

void loop() { delay(1000); }
