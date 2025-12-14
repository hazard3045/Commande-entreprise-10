#include <iostream>
#include <pigpio.h>
#include <unistd.h>
#include <iomanip> 

using namespace std;
int nbImpulsions = 0 ; // compteur d'impulsion 
int clk_externe = 0; // la clock externe donné par le GPS 
bool impulsion = false; // True si il y a eu une impulsion False sinon 
int gpio_imp = 17; // GPIO pour les impulsions
int gpio_clk = 27; // GPIO pour la clock externe

 
// Fonction 

string prendre_photo(){
    // Génère un nom de fichier avec le format : photo_nbImpulsions_clk_externe_clk_interne
    string nomFichier = "/mnt/usb/images/photo_" + to_string(nbImpulsions) + "_" + 
                        to_string(clk_externe) + "_" + 
                        to_string(gpioTick()) + ".dng";
    
    // Construit la commande libcamera-still
    string commande = "libcamera-still -- raw -o " + nomFichier + " --width 4608 --height 2592 --nopreview -t 1"
    "--exposure sport --shutter 20000 --gain 1.0 --autofocus-mode manual --lens-position 0.0 --awb auto 2>/dev/null";

    // Exécute la commande
    int resultat = system(commande.c_str());
    system("sync");
    
    if (resultat == 0) {
        cout << "Photo prise : " << nomFichier << endl;
        return nomFichier;
    } else {
        cerr << "Erreur lors de la prise de photo" << endl;
        return "";
    }
}


void rising_callback_clk(int gpio, int level, uint32_t tick) {
    if (level == 1){
        clk_externe += 1;
    }
}

void rising_callback_impul(int gpio, int level, uint32_t tick) {
    if (level == 1){
        impulsion = true;
        nbImpulsions += 1;
    }
}

//main 
int main(){

    if (gpioInitialise() < 0) {
        std::cerr << "Erreur : Pigpio init failed\n";
        return 1;
    }

    std::cout << "Programme démarré, test ISR sur GPIO 17 et 27\n";

    gpioSetMode(gpio_imp, PI_INPUT);
    gpioSetMode(gpio_clk, PI_INPUT);
    gpioSetPullUpDown(gpio_imp, PI_PUD_DOWN);
    gpioSetPullUpDown(gpio_clk, PI_PUD_DOWN);

    // ALERT func = en mode pollé ultra-rapide, déclenché à chaque changement
    gpioSetAlertFunc(gpio_imp, rising_callback_impul);
    gpioSetAlertFunc(gpio_clk, rising_callback_clk);

    

    while (true){
        if (impulsion == true){
            std::cout << "impulsion reçue\n";
            impulsion = false;
            string photo = prendre_photo();
        }
        usleep(1000); // Attendre 1 ms pour éviter une boucle trop rapide
    }
}
