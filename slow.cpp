#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <sstream>
#include <chrono>
#include <thread>
#include <iomanip>
#include <cstdlib> // Pour system() et popen()

// Structure simplifiée pour une image en mémoire
struct ImageData {
    std::vector<unsigned char> data;
    std::string filename;
    size_t size;
    // timestamp est conservé mais non utilisé pour le séquencement
    std::chrono::system_clock::time_point timestamp;
};

// Système de capture séquentiel
class SequentialCaptureSystem {
private:
    // Configuration
    std::string output_dir;
    int target_images;
    int capture_interval_ms;
    
    // Stats
    int images_processed = 0;
    std::chrono::system_clock::time_point start_time;
    size_t total_bytes_written = 0;
    
public:
    SequentialCaptureSystem(const std::string& dir, int num_images, int interval_ms = 2000)
        : output_dir(dir), target_images(num_images), capture_interval_ms(interval_ms) {}
    
    // Capture et écriture séquentielles
    void start() {
        start_time = std::chrono::system_clock::now();
        
        std::cout << "\n=== CAPTURE SÉQUENTIELLE DÉMARRÉE ===" << std::endl;
        std::cout << "Images: " << target_images << std::endl;
        std::cout << "Intervalle cible: " << capture_interval_ms << "ms" << std::endl;
        std::cout << "Destination: " << output_dir << std::endl;
        std::cout << "======================================\n" << std::endl;
        
        for (int i = 0; i < target_images; ++i) {
            auto cycle_start = std::chrono::high_resolution_clock::now();
            
            // 1. Capture de l'image (Simulation de la fonction captureThread)
            ImageData img = captureImage(i);
            
            // 2. Écriture sur disque (Simulation de la fonction writeThread)
            if (!img.data.empty()) {
                writeImage(img);
            }
            
            images_processed++;
            
            auto cycle_end = std::chrono::high_resolution_clock::now();
            auto cycle_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                cycle_end - cycle_start);
            
            std::cout << "[SÉQUENTIEL] Cycle " << images_processed << "/" << target_images 
                     << " terminé en " << cycle_duration.count() << "ms" << std::endl;
            
            // 3. Attente avant la prochaine capture
            auto wait_time = std::chrono::milliseconds(capture_interval_ms) - cycle_duration;
            if (wait_time.count() > 0) {
                std::this_thread::sleep_for(wait_time);
            }
        }
        
        printStats();
    }
    
private:
    ImageData captureImage(int index) {
        auto capture_start = std::chrono::high_resolution_clock::now();
        
        // Commande pour capture
        std::string cmd = "libcamera-still -r -o - --width 4608 --height 2592 "
            "-t 1 --nopreview --exposure sport --awb auto 2>/dev/null";
            
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) {
            std::cerr << "[CAPTURE] Erreur ouverture pipe" << std::endl;
            return {}; // Retourne une structure vide
        }
        
        // Lecture dans buffer mémoire
        ImageData img;
        img.timestamp = std::chrono::system_clock::now();
        img.filename = generateFilename(index);
        
        const size_t chunk_size = 65536; // 64KB chunks
        unsigned char chunk[chunk_size];
        size_t bytes_read;
        
        while ((bytes_read = fread(chunk, 1, chunk_size, pipe)) > 0) {
            img.data.insert(img.data.end(), chunk, chunk + bytes_read);
        }
        
        pclose(pipe);
        img.size = img.data.size();
        
        auto capture_end = std::chrono::high_resolution_clock::now();
        auto capture_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            capture_end - capture_start);
        
        std::cout << "  [INFO] Image " << index + 1 << " capturée (" << img.size/1024 << " KB) en " 
                 << capture_duration.count() << "ms" << std::endl;
        
        return img;
    }
    
    void writeImage(const ImageData& img) {
        auto write_start = std::chrono::high_resolution_clock::now();
        
        std::string filepath = output_dir + "/" + img.filename;
        std::ofstream file(filepath, std::ios::binary);
        
        if (!file) {
            std::cerr << "[WRITE] Erreur écriture: " << filepath << std::endl;
            return;
        }
        
        file.write(reinterpret_cast<const char*>(img.data.data()), img.size);
        file.close();
        
        auto write_end = std::chrono::high_resolution_clock::now();
        auto write_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            write_end - write_start);
        
        total_bytes_written += img.size;
        
        std::cout << "  [INFO] Image écrite (" << img.size/1024 << " KB) en " 
                 << write_duration.count() << "ms" << std::endl;
    }

    std::string generateFilename(int index) {
        std::ostringstream oss;
        oss << "seq_image_" << std::setfill('0') << std::setw(5) << index << ".jpg";
        return oss.str();
    }
    
    void printStats() {
        auto end_time = std::chrono::system_clock::now();
        auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);
        
        std::cout << "\n=== STATISTIQUES SÉQUENTIELLES ===" << std::endl;
        std::cout << "Images traitées: " << images_processed << std::endl;
        std::cout << "Temps total: " << total_duration.count() / 1000.0 << "s" << std::endl;
        std::cout << "Données écrites: " << total_bytes_written / 1024 / 1024 << " MB" << std::endl;
        
        if (images_processed > 0) {
            double avg_time = total_duration.count() / (double)images_processed;
            double fps = 1000.0 / avg_time;
            std::cout << "Temps moyen/image: " << avg_time << "ms" << std::endl;
            std::cout << "Vitesse: " << std::fixed << std::setprecision(2) 
                     << fps << " img/s" << std::endl;
        }
        std::cout << "==================================\n" << std::endl;
    }
};

int main(int argc, char* argv[]) {
    // Paramètres par défaut
    int num_images = 5;           // Réduit le nombre par défaut pour un test rapide
    int interval_ms = 2000;        
    std::string output_dir = "/home/rpi0/photos";
    
    // Arguments en ligne de commande
    if (argc > 1) num_images = std::atoi(argv[1]);
    if (argc > 2) interval_ms = std::atoi(argv[2]);
    if (argc > 3) output_dir = argv[3];
    
    // Vérifier/créer le dossier de sortie
    std::string mkdir_cmd = "mkdir -p " + output_dir;
    system(mkdir_cmd.c_str());
    
    // Avertissement RAM (moins critique car une seule image est en mémoire à la fois)
    std::cout << "\n⚠️  INFO: Capture qualité maximale (4608x2592)" << std::endl;
    std::cout << "Ce code est séquentiel et ne bufferise pas les images en RAM." << std::endl;
    std::cout << "Cependant, la capture et l'écriture sont bloquantes.\n" << std::endl;
    
    // Lancer le système de capture séquentielle
    SequentialCaptureSystem capture_system(output_dir, num_images, interval_ms);
    
    try {
        capture_system.start();
        std::cout << "✓ Capture séquentielle terminée avec succès!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "✗ Erreur: " << e.what() << std::endl;
        return 1;
    }
}