#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

class StatusLED {
  private:
    Adafruit_NeoPixel strip;
    uint8_t pin;
    uint8_t r_base, g_base, b_base;
    unsigned long lastUpdate;
    int brightness;
    int increment; 

  public:
    StatusLED(uint8_t _pin) : strip(1, _pin, NEO_GRB + NEO_KHZ800) {
      pin = _pin;
      lastUpdate = 0;
      brightness = 10;
      increment = 1; // Controla la suavidad (1 es el más suave)
      // Color inicial (Verde suave)
      r_base = 0; g_base = 255; b_base = 0;
    }

    void begin() {
      strip.begin();
      strip.setBrightness(255); // Dejamos el brillo de la librería al máximo
      strip.show();
    }

    void setColor(uint8_t r, uint8_t g, uint8_t b) {
      r_base = r;
      g_base = g;
      b_base = b;
    }

    void update() {
      // Actualizamos cada 50ms para una animación fluida a 20fps
      if (millis() - lastUpdate >= 50) {
        lastUpdate = millis();

        // Cambiamos el brillo
        brightness += increment;

        // Si llega a los límites, invertimos la dirección
        // Usamos un rango de 10 a 200 para que nunca se apague del todo 
        // y no consuma demasiada energía en el máximo.
        if (brightness <= 2 || brightness >= 20) {
          increment *= -1;
        }

        // CALCULAMOS EL COLOR ESCALADO
        // Es vital multiplicar antes de dividir para no perder precisión
        uint8_t r = (r_base * brightness) / 255;
        uint8_t g = (g_base * brightness) / 255;
        uint8_t b = (b_base * brightness) / 255;

        strip.setPixelColor(0, strip.Color(r, g, b));
        strip.show();
      }
    }
};