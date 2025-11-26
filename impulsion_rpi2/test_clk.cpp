#include <pigpio.h>
#include <chrono>
#include <thread>
#include <iostream>

uint32_t clk_rising = 0;
uint32_t clk_falling = 0;

void interrupt_clk_rising(int gpio, int level, uint32_t tick){
    clk_rising = tick;
    std::cout << "Front montant reçu !" << std::endl;
}

void interrupt_clk_falling(int gpio, int level, uint32_t tick){
    clk_falling = tick;
    std::cout << "Periodo : " << (clk_falling - clk_rising) << " µs" << std::endl;
}

int main() {

    if (gpioInitialise() < 0) {
        std::cerr << "Pigpio initialization failed!" << std::endl;
        return 1;
    }

    std::cout << "Programme démarré" << std::endl;

    gpioSetMode(13, PI_INPUT);
    gpioSetPullUpDown(13, PI_PUD_OFF);  // ou PI_PUD_UP selon ton signal

    gpioSetISRFunc(13, RISING_EDGE, 0, interrupt_clk_rising);
    gpioSetISRFunc(13, FALLING_EDGE, 0, interrupt_clk_falling);

    while (true)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

    gpioTerminate();
    return 0;
}
