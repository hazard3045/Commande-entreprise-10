#include <pigpio.h>
#include <iostream>
#include <thread>

bool bascule = false;
uint32_t global_tick = 0;

void rising_callback(int gpio, int level, uint32_t tick) {
    bascule = true;
    global_tick = tick;
}

int main() {

    if (gpioInitialise() < 0) {
        std::cerr << "Erreur : Pigpio init failed\n";
        return 1;
    }

    int gpio = 17;
    std::cout << "Programme démarré, test ISR sur GPIO 17\n";

    gpioSetMode(gpio, PI_INPUT);
    gpioSetPullUpDown(gpio, PI_PUD_DOWN);

    // ALERT func = en mode pollé ultra-rapide, déclenché à chaque changement
    gpioSetAlertFunc(gpio, rising_callback);

    while (true) {
        if (bascule) {
            bascule = false;
            std::cout << "Front détecté, tick = " << global_tick << "\n";
        }
        gpioDelay(1000);  // 1ms
    }

    gpioTerminate();
    return 0;
}
