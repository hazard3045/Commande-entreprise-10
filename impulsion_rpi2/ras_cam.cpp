#include <iostream>
#include <pigpio.h>
#include <unistd.h>
#include <iomanip> 

using namespace std;
int main(){
    if(gpioInitialise() < 0){cout<<"can't connect to pigpiodaemon"<<endl;}
    else{
        const int pin = 13;
        gpioSetMode(pin , PI_INPUT);
        int level = 0;
        int new_level;
        int nbImpulsions = 0;
        while(1){
             new_level= gpioRead(pin);
             cout<<gpioRead(pin)<<endl;
             if(new_level != level){
                cout<<"GPIO33 is level %d"<<new_level<<endl;
                level = new_level;
                nbImpulsions=nbImpulsions+1;
                cout<<"total reçu"<<nbImpulsions<<endl;
             }
            
        }    
    }
    gpioTerminate();
    return 0 ;
}
