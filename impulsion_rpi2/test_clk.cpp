#include <pigpio.h>
#include <iostream>
#include <chrono>
#include <thread>

void rising_callback(int gpio, int level, uint32_t tick) {
    std::cout << "Front montant détecté à tick=" << tick << std::endl;
}

void falling_callback(int gpio, int level, uint32_t tick) {
    std::cout << "Front descendant détecté à tick=" << tick << std::endl;
}

int main() {
    // Initialisation pigpio
    if (gpioInitialise() < 0) {
        std::cerr << "Erreur : Pigpio initialization failed!" << std::endl;
        return 1;
    }
    std::cout << "Programme démarré, test ISR sur GPIO 13" << std::endl;

    // Configurer GPIO 13 comme entrée avec pull-down pour signal flottant
    gpioSetMode(13, PI_INPUT);
    gpioSetPullUpDown(13, PI_PUD_DOWN); // ou PI_PUD_UP selon ton signal

    // Installer les callbacks ISR
    gpioSetISRFunc(13, RISING_EDGE, 0, rising_callback);
    gpioSetISRFunc(13, FALLING_EDGE, 0, falling_callback);

    // Boucle principale : ne pas bloquer le thread ISR
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        std::cout << gpioRead(13) << std::endl;
    }

    gpioTerminate(); // jamais atteint ici
    return 0;
}
