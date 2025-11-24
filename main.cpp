#include <iostream>
#include <pigpio.h>
#include <unistd.h>
#include <iomanip> 
using namespace std;
int nbImpulsions; // compteur d'impulsion 
int clk_externe; // la clock externe donné par le GPS 
bool impulsion; // True si il y a eu une impulsion False sinon 
 
// Fonction 

string prendre_photo(){
    // Génère un nom de fichier avec le format : photo_nbImpulsions_clk_externe_clk_interne
    string nomFichier = "photo_" + to_string(nbImpulsions) + "_" + 
                        to_string(clk_externe) + "_" + 
                        to_string(clk_interne) + ".jpg";
    
    // Construit la commande libcamera-jpeg
    string commande = "libcamera-jpeg -o " + nomFichier + " --nopreview -t 1";
    
    // Exécute la commande
    int resultat = system(commande.c_str());
    
    if (resultat == 0) {
        cout << "Photo prise : " << nomFichier << endl;
        return nomFichier;
    } else {
        cerr << "Erreur lors de la prise de photo" << endl;
        return "";
    }
}

int*int*int tag_photo(){
    // tager la photo avec (nbImpulsion,clk_externe,clk_interne)
    return(1,1,1)
}

void interrupt_impuls(){
    
}

void interrupt_clk{

}

bool stock_photo(){
//prend une photo  en paramétre et la stock sur la carte SD renvoie True quand c'est terminé 
return True 
}

void compresser_photo(){
    // prend une photo en paramétre la compresse et return la photo
}


//main 
int main(){
    return 1
}
