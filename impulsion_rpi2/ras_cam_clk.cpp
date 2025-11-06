#include <iostream>
#include <pigpio.h>
#include <unistd.h>
#include <iomanip> 
using namespace std;
int main(){
    if(gpioInitialise() < 0){cout<<"can't connect to pigpiodaemon"<<endl;}
    else{
        const int pin = 13;
        const int pin_clk = 12;
        gpioSetMode(pin , PI_INPUT);
        gpioSetMode(pin_clk,PI_INPUT);
        int level = 0;
        int new_level;
        int clk=0;
        int new_clk;
        int horloge=0;
        int nbImpulsions=0;
        while(1){
             new_level = gpioRead(pin);
             new_clk = gpioRead(pin_clk);
             if(new_level != level){
                cout<<"GPIO33 is level %d"<<new_level<<endl;
                level = new_level;
                nbImpulsions=nbImpulsions+1;
                cout<<"total reçu %d "<<nbImpulsions<<endl;
             }
             if(new_clk != clk ){
                horloge=horloge+1;
                clk=new_clk;
             }
            
        }    
    }
    gpioTerminate();
    return 0 ;
}