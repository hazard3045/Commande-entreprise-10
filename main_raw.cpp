#include <iostream>
#include <pigpio.h>
#include <unistd.h>
#include <iomanip> 
#include <chrono>
#include <string> // Ajout pour to_string
#include <cstdlib> // Ajout pour system()
using namespace std;
int nbImpulsions; // compteur d'impulsion 
int clk_externe; // la clock externe donné par le GPS 
int clk_interne; 
bool impulsion; // True si il y a eu une impulsion False sinon 
 
// Fonction 

string prendre_photo(){
    // Génère un nom de fichier avec l'extension .raw
    string nomFichier = "photo_" + to_string(nbImpulsions) + "_" + 
                        to_string(clk_externe) + "_" + 
                        to_string(clk_interne) ; // MODIFICATION 1 : Extension .raw
    
    // Construit la commande libcamera-raw
    // libcamera-raw est utilisé pour la capture des données brutes du capteur
    string commande = "libcamera-still --raw -o /mnt/usb/"+nomFichier+".dng"
    " --width 4608 --height 2592 -t 1 --nopreview "
    "--exposure sport --shutter 20000 --gain 1.0 --autofocus-mode manual --lens-position 0.0 --awb auto 2>/dev/null";
    //--nopreview --exposure sport --immediate  --awb auto
    
    // Exécute la commande
    int resultat = system(commande.c_str());
    
    if (resultat == 0) {
        cout << "Photo prise : " << nomFichier << ".dng" << endl;
        return nomFichier;
    } else {
        cerr << "Erreur lors de la prise de photo" << endl;
        return "";
    }
}

// ... (autres fonctions non modifiées)

//main 
int main(){
    cout << "--- TEST 1 : Capture Basique ---" << endl;
    
    // Définition des variables
    nbImpulsions = 1;
    clk_externe = 1;
    clk_interne = 5;
    // La variable total_duration doit être de type chrono::milliseconds
    chrono::milliseconds total_duration = chrono::milliseconds(0); 

    for (int i = 0; i < 5; ++i) {
        clk_interne +=1;
        auto start_test = chrono::high_resolution_clock::now();
        // clk_externe += 1; // J'ai commenté car clk_externe n'est pas défini ici
        // Assumons que prendre_photo() est une fonction C++ qui retourne une string
        string resultat = prendre_photo(); 
        auto end_test = chrono::high_resolution_clock::now();

        if (!resultat.empty()) {
            auto duration_test = chrono::duration_cast<chrono::milliseconds>(end_test - start_test);
            
            // CORRECTION: Mettre à jour la variable globale sans 'auto' ni cast superflu
            total_duration += duration_test;
            
            cout << "Temps total (Capture + Stockage) : " << duration_test.count() << " ms" << endl;
            cout << "Test 1 RÉUSSI : Vérifiez la présence du fichier." << endl;
        } else {
            cout << "Test 1 ÉCHOUÉ : La fonction a renvoyé une chaîne vide." << endl;
        }
    }
    
    // Correction de la ligne suivante pour utiliser la variable de type chrono::milliseconds
    cout << "Temps total pour les 5 prises de vue :" << total_duration.count() << "ms" <<endl;
    cout << "Temps moyen:" << total_duration.count()/5 << "ms" <<endl;
    // // Testez un deuxième scénario
    // cout << "\n--- TEST 2 : Capture avec incrémentation ---" << endl;
    // nbImpulsions = 2; // Correction, nbImpulsions était oublié dans la version précédente.
    // clk_externe = 1001;
    // clk_interne = 50001;
    // auto start_test2 = chrono::high_resolution_clock::now();
    // resultat = prendre_photo();
    // auto end_test2 = chrono::high_resolution_clock::now();

    // if (!resultat.empty()) {
    //     auto duration_test2 = chrono::duration_cast<chrono::milliseconds>(end_test2 - start_test2);
    //     cout << "Temps total (Capture + Stockage) : " << duration_test2.count() << " ms" << endl;

    //     cout << "Nom de fichier attendu : photo_2_1001_50001.raw" << endl; // MODIFICATION 3 : Vérification .raw
    //     cout << "Test 2 RÉUSSI : Vérifiez la présence du deuxième fichier." << endl;
    // } else {
    //     cout << "Test 2 ÉCHOUÉ : La fonction a renvoyé une chaîne vide." << endl;
    // }

    // gpioTerminate(); // Terminer pigpio si initialisé
    return 0; // Retourne 0 en cas de succès général du programme
}
