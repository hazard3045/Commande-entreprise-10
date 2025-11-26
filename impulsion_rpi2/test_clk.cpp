#include <pigpio.h>
#include <chrono>
#include <thread>
#include <iostream>

using namespace std;

uint32_t clk_rising = 0;
uint32_t clk_falling = 0;


void interrupt_clk_rising(int gpio, int level, uint32_t tick){
    clk_rising = tick;
    cout << "j'ai reçu un front montant" << endl;
}

void interrupt_clk_falling(int gpio, int level, uint32_t tick){
    clk_falling =tick;
    cout << "Clock Period: " << (clk_falling - clk_rising) << " microseconds" << endl;
}

int main() {
    if (gpioInitialise() < 0) {
        std::cerr << "Pigpio initialization failed!" << std::endl;
        return 1;
    }
    cout << "je lance le programme" << endl;
    gpioSetISRFunc(13, RISING_EDGE, 0, interrupt_clk_rising);
    gpioSetISRFunc(13, FALLING_EDGE, 0, interrupt_clk_falling);
    while(1){}
}
