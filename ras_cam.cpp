#include <iostream>
#include <pigpio.h>
#include <unistd.h>
int main(){
    if(gpioInitialise() < 0){}
    else{
        const int pin = 33;
        gpioSetMode(pin , PI_INPUT);
        int level = 0;
        int new_level;
        int i=0;
        while(1){
             new_level= gpioRead(pin);
             if(new_level != level){
                printf("GPIO33 is level %d", new_level);
                level = new_level;
                i=i+1;
                printf("total reçu %d " , i);
             }
            
        }    
    }
    return 0 ;
}