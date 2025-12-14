#include <iostream>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <fstream>
#include <chrono>
#include <atomic>
#include <iomanip>
#include <cstring>
#include <sstream>
#include <cstdlib> // Pour system()

// Structure pour une image en mémoire
struct ImageData {
    std::vector<unsigned char> data;
    std::string filename;
    size_t size;
    std::chrono::system_clock::time_point timestamp;
};

// Système de double buffering pour capture continue
class ContinuousCaptureSystem {
private:
    // File d'attente thread-safe pour images à écrire
    std::queue<ImageData> write_queue;
    std::mutex queue_mutex;
    std::condition_variable queue_cv;
    
    // Contrôle threads
    std::atomic<bool> should_stop{false};
    std::atomic<int> images_captured{0};
    std::atomic<int> images_written{0};
    
    // Configuration
    std::string output_dir;
    int target_images;
    int capture_interval_ms;
    
    // Stats
    std::chrono::system_clock::time_point start_time;
    size_t total_bytes_written = 0;
    
public:
    ContinuousCaptureSystem(const std::string& dir, int num_images, int interval_ms = 2000)
        : output_dir(dir), target_images(num_images), capture_interval_ms(interval_ms) {}
    
    // Thread de capture - stocke en RAM
    void captureThread() {
        std::cout << "[CAPTURE] Thread démarré" << std::endl;
        
        while (!should_stop && images_captured < target_images) {
            auto capture_start = std::chrono::high_resolution_clock::now();
            
            int current_index = images_captured;
            // Utiliser un nom de fichier temporaire pour le DNG
            std::string temp_dng_path = "/tmp/shm/" + generateFilename(current_index, ".dng");
            
            // Commande pour capture DNG (RAW) vers un fichier temporaire
            // -r (--raw) force le format RAW
            std::string cmd = "libcamera-still -r -o " + temp_dng_path + 
                " --width 4608 --height 2592 -t 1 --nopreview "
                "--exposure sport --shutter 20000 --gain 1.0 --autofocus-mode manual --lens-position 0.0 2>/dev/null";

                //--exposure sport --awb auto
            // 1. Exécuter la capture (bloquante: DNG est écrit sur le disque)
            int result = system(cmd.c_str());
            
            if (result != 0) {
                std::cerr << "[CAPTURE] Erreur lors de l'exécution de libcamera-still (" 
                         << current_index + 1 << "): " << result << std::endl;
                continue;
            }
            
            // 2. Lecture du fichier DNG temporaire en RAM
            ImageData img;
            img.timestamp = std::chrono::system_clock::now();
            img.filename = generateFilename(current_index, ".dng"); // Nom permanent pour l'écriture
            
            std::ifstream file(temp_dng_path, std::ios::binary | std::ios::ate);
            if (!file) {
                std::cerr << "[CAPTURE] Erreur lecture fichier DNG temporaire: " << temp_dng_path << std::endl;
                // Tentative de suppression même en cas d'erreur de lecture
                std::string rm_cmd = "rm -f " + temp_dng_path;
                system(rm_cmd.c_str());
                continue;
            }
            
            img.size = file.tellg();
            img.data.resize(img.size);
            file.seekg(0, std::ios::beg);
            file.read(reinterpret_cast<char*>(img.data.data()), img.size);
            file.close();

            // 3. Suppression du fichier temporaire du disque
            std::string rm_cmd = "rm " + temp_dng_path;
            system(rm_cmd.c_str());

            // 4. Ajouter à la file d'écriture (mémoire -> disque final)
            {
                std::lock_guard<std::mutex> lock(queue_mutex);
                write_queue.push(std::move(img));
            }
            queue_cv.notify_one();
            
            int current = ++images_captured;
            auto capture_end = std::chrono::high_resolution_clock::now();
            auto capture_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                capture_end - capture_start);
            
            std::cout << "[CAPTURE] Image " << current << "/" << target_images 
                     << " capturée (" << img.size/1024 << " KB) en " 
                     << capture_duration.count() << "ms" << std::endl;
            
            // Attendre avant prochaine capture
            auto wait_time = std::chrono::milliseconds(capture_interval_ms) - capture_duration;
            if (wait_time.count() > 0) {
                std::this_thread::sleep_for(wait_time);
            }
        }
        
        std::cout << "[CAPTURE] Thread terminé - " << images_captured 
                 << " images capturées" << std::endl;
    }
    
    // Thread d'écriture - transfert continu vers SD
    void writeThread() {
        std::cout << "[WRITE] Thread démarré" << std::endl;
        
        while (!should_stop || !write_queue.empty()) {
            std::unique_lock<std::mutex> lock(queue_mutex);
            
            // Attendre qu'une image soit disponible
            queue_cv.wait(lock, [this]{ 
                return !write_queue.empty() || should_stop; 
            });
            
            if (write_queue.empty()) {
                continue;
            }
            
            // Récupérer image à écrire
            ImageData img = std::move(write_queue.front());
            write_queue.pop();
            lock.unlock();
            
            // Écriture sur carte SD (hors du mutex)
            auto write_start = std::chrono::high_resolution_clock::now();
            
            std::string filepath = output_dir + "/" + img.filename;
            std::ofstream file(filepath, std::ios::binary);
            
            if (!file) {
                std::cerr << "[WRITE] Erreur écriture: " << filepath << std::endl;
                continue;
            }
            
            file.write(reinterpret_cast<const char*>(img.data.data()), img.size);
            file.close();
            
            auto write_end = std::chrono::high_resolution_clock::now();
            auto write_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                write_end - write_start);
            
            total_bytes_written += img.size;
            int current = ++images_written;
            
            std::cout << "[WRITE] Image " << current << "/" << target_images 
                     << " écrite (" << img.size/1024 << " KB) en " 
                     << write_duration.count() << "ms - Queue: " << write_queue.size() 
                     << std::endl;
        }
        
        std::cout << "[WRITE] Thread terminé - " << images_written 
                 << " images écrites" << std::endl;
    }
    
    // Lancer la capture continue
    void start() {
        start_time = std::chrono::system_clock::now();
        should_stop = false;
        
        std::cout << "\n=== CAPTURE CONTINUE DÉMARRÉE ===" << std::endl;
        std::cout << "Images: " << target_images << std::endl;
        std::cout << "Intervalle: " << capture_interval_ms << "ms" << std::endl;
        std::cout << "Résolution: 4608x2592 (DNG RAW SANS PERTE)" << std::endl;
        std::cout << "Destination: " << output_dir << std::endl;
        std::cout << "===================================\n" << std::endl;
        
        // Lancer les deux threads en parallèle
        std::thread capture_t(&ContinuousCaptureSystem::captureThread, this);
        std::thread write_t(&ContinuousCaptureSystem::writeThread, this);
        
        // Attendre fin capture
        capture_t.join();
        
        // Signaler fin au thread d'écriture
        should_stop = true;
        queue_cv.notify_all();
        
        // Attendre fin écriture
        write_t.join();
        
        printStats();
    }
    
private:
    // Utiliser .dng ou .dng.tmp comme extension
    std::string generateFilename(int index, const std::string& extension = ".dng") {
        std::ostringstream oss;
        oss << "image_" << std::setfill('0') << std::setw(5) << index << extension;
        return oss.str();
    }
    
    void printStats() {
        auto end_time = std::chrono::system_clock::now();
        auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);
        
        std::cout << "\n=== STATISTIQUES ===" << std::endl;
        std::cout << "Images capturées: " << images_captured << std::endl;
        std::cout << "Images écrites: " << images_written << std::endl;
        std::cout << "Temps total: " << total_duration.count() / 1000.0 << "s" << std::endl;
        std::cout << "Données écrites: " << total_bytes_written / 1024 / 1024 << " MB" << std::endl;
        
        if (images_captured > 0) {
            double avg_time = total_duration.count() / (double)images_captured;
            double fps = 1000.0 / avg_time;
            std::cout << "Temps moyen/image: " << avg_time << "ms" << std::endl;
            std::cout << "Vitesse: " << std::fixed << std::setprecision(2) 
                     << fps << " img/s" << std::endl;
        }
        std::cout << "===================\n" << std::endl;
    }
};

int main(int argc, char* argv[]) {
    // Paramètres par défaut
    int num_images = 30;           // 30 images (1 minute à 1 photo/2s)
    int interval_ms = 2000;        // 2 secondes entre chaque photo
    std::string output_dir = "/home/rpi0/photos";
    
    // Arguments en ligne de commande
    if (argc > 1) num_images = std::atoi(argv[1]);
    if (argc > 2) interval_ms = std::atoi(argv[2]);
    if (argc > 3) output_dir = argv[3];
    
    // Vérifier/créer le dossier de sortie
    std::string mkdir_cmd = "mkdir -p " + output_dir;
    system(mkdir_cmd.c_str());
    
    // Avertissement RAM
    std::cout << "\n⚠️  AVERTISSEMENT: Capture DNG RAW (4608x2592)" << std::endl;
    std::cout << "Chaque image DNG = ~18-25 MB" << std::endl;
    std::cout << "RAM limitée sur Pi Zero 2 W (512 MB). La capture inclut une double opération disque (écrire temp, lire temp)." << std::endl;
    std::cout << "Si erreur allocation mémoire, réduire résolution.\n" << std::endl;
    
    // Lancer le système de capture continue
    ContinuousCaptureSystem capture_system(output_dir, num_images, interval_ms);
    
    try {
        capture_system.start();
        std::cout << "✓ Capture terminée avec succès!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "✗ Erreur: " << e.what() << std::endl;
        return 1;
    }
}