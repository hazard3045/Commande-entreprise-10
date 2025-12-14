#include <iostream>
#include <pigpio.h>
#include <unistd.h>
#include <iomanip> 
#include <chrono>
#include <string> 
#include <cstdlib> 
#include <fstream> // Ajout pour la lecture du PID
#include <signal.h> // Ajout pour la fonction kill()

using namespace std;

// Variables globales (inchangées)
int nbImpulsions = 1; 
int clk_externe = 1; 
int clk_interne = 5; 
// ... (impulsion, prendre_photo, autres fonctions)

// Variable pour stocker le PID de libcamera-vid
pid_t camera_pid = -1;

// --- MODIFICATION: Adaptation du Lancement pour l'astuce du lien symbolique ---
void launch_camera_stream(const string& target_dir) {
    // La sortie doit aller vers le chemin utilisé par l'astuce du lien symbolique
    string launch_cmd = "libcamera-vid -t 0 --raw --width 4608 --height 2592 -n --signal SIGUSR1 -o /tmp/snapshot.dng &";
                        
    cout << "Lancement du pipeline libcamera-vid..." << endl;
    system(launch_cmd.c_str());
    
    // Attendre 1 seconde que le processus démarre et initialise le capteur
    usleep(1000000); 
    cout << "Pipeline démarré." << endl;
}

// --- NOUVELLE FONCTION: Déclencher la Photo (Utilise pkill) ---
string trigger_photo(int index) {
    // Génère le nom de fichier
    string nomFichier = "photo_" + to_string(nbImpulsions) + "_" + 
                        to_string(clk_externe) + "_" + 
                        to_string(clk_interne) + ".dng";
                        
    string file_path = "/mnt/usb_photo/raw_data/" + nomFichier;
    
    // COMMANDE DE DÉCLENCHEMENT (1) : Créer un lien symbolique pour nommer le fichier de sortie
    // Cela nécessite que libcamera-vid soit lancé avec -o /tmp/snapshot.dng
    string link_cmd = "ln -sf " + file_path + " /tmp/snapshot.dng";
    system(link_cmd.c_str());
    
    // COMMANDE DE DÉCLENCHEMENT (2) : ENVOI DU SIGNAL via pkill
    // pkill trouve le processus "libcamera-vid" et lui envoie le signal SIGUSR1
    string pkill_cmd = "pkill -SIGUSR1 libcamera-vid";

    if (system(pkill_cmd.c_str()) == 0) {
        // Attendre que le fichier soit écrit (latence d'écriture du 23 Mo)
        usleep(100000); // 100ms
        cout << "Photo " << index << " déclenchée: " << nomFichier << endl;
        return nomFichier;
    } else {
        // Si pkill échoue (généralement car le processus n'existe pas ou permission)
        cerr << "Erreur: Impossible d'envoyer le signal SIGUSR1 à libcamera-vid via pkill." << endl;
        return "";
    }
}


// main
int main(){
    cout << "--- TEST 1 : Capture Basique ---" << endl;
    
    // -----------------------------------------------------
    // MODIFICATION 1 : LANCEMENT UNIQUE DU PIPELINE
    // -----------------------------------------------------
    launch_camera_stream("/mnt/usb_photo/raw_data"); 
    
    // Définition des variables
    chrono::milliseconds total_duration = chrono::milliseconds(0); 

    for (int i = 0; i < 5; ++i) {
        auto start_test = chrono::high_resolution_clock::now();
        
        // MODIFICATION 2 : APPEL SANS PID
        string resultat = trigger_photo(i + 1);
        
        auto end_test = chrono::high_resolution_clock::now();

        if (!resultat.empty()) {
            auto duration_test = chrono::duration_cast<chrono::milliseconds>(end_test - start_test);
            total_duration += duration_test;
            
            // La latence mesurée ici est uniquement l'envoi du signal + l'écriture du fichier
            cout << "Temps total (Déclenchement + Écriture) : " << duration_test.count() << " ms" << endl;
            cout << "Test 1 RÉUSSI : Vérifiez le fichier sur la clé USB." << endl;
        } else {
            cout << "Test 1 ÉCHOUÉ : La fonction a renvoyé une chaîne vide." << endl;
        }
        
        clk_externe += 1; // Mise à jour des variables pour le nom de fichier
    }
    
// -----------------------------------------------------
    // MODIFICATION 3 : NETTOYAGE
    // -----------------------------------------------------
    cout << "\nArrêt du pipeline libcamera-vid..." << endl;
    system("pkill libcamera-vid"); // Utiliser pkill pour l'arrêt aussi
    
    return 0; 
}