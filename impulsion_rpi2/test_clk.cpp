#include <pigpio.h>
#include <iostream>
#include <thread>

bool bascule = true;
uint32_t global_tick = 12;

void rising_callback(int gpio, int level, uint32_t tick) {
    bascule = true;
    global_tick = tick;
}

//void falling_callback(int gpio, int level, uint32_t tick) {}

int main() {
    int pi = 17;

    bascule = false;
    
    // Initialisation pigpio
    if (gpioInitialise() < 0) {
        std::cerr << "Erreur : Pigpio initialization failed!" << std::endl;
        return 1;
    }
    std::cout << "Programme démarré, test ISR sur GPIO 17" << std::endl;

    // Configurer GPIO 17 comme entrée avec pull-down pour signal flottant
    gpioSetMode(pi, PI_INPUT);
    gpioSetPullUpDown(pi, PI_PUD_DOWN); 

    // Installer les callbacks ISR
    gpioSetISRFunc(pi, EITHER_EDGE, 0, rising_callback);
    //gpioSetISRFunc(pi, FALLING_EDGE, 0, falling_callback);

    // Boucle principale : ne pas bloquer le thread ISR
    while (true) {
        if (bascule == true){
            bascule = false;
            std::cout << "rise happened at" << global_tick << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        std::cout << gpioRead(pi) << std::endl;
    }

    gpioTerminate(); // jamais atteint ici
    return 0;
}
