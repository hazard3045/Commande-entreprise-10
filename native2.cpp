#include <iomanip>
#include <iostream>
#include <memory>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <chrono>
#include <sstream>
#include <fstream>
#include <sys/mman.h> // Pour mmap (mapping mémoire)
#include <unistd.h>   // Pour ftruncate, close, et lseek
#include <fcntl.h>    // Pour open (gestion des fichiers)

#include <libcamera/libcamera.h>

using namespace libcamera;
using namespace std::chrono_literals;
using namespace std::chrono;

// Variables globales pour la synchronisation
static std::shared_ptr<Camera> camera;
static std::mutex mtx;
static std::condition_variable cv;
static bool photoReady = false;
static int photoCounter = 0; // Compteur pour nommer les fichiers


// Fonction utilitaire pour générer un nom de fichier
static std::string generateFilename(int index) {
    std::ostringstream oss;
    oss << "capture_" << std::setw(4) << std::setfill('0') << index << ".dng";
    return oss.str();
}

// Fonction utilitaire pour sauvegarder le buffer RAW sur disque
// Utilise mmap pour l'accès direct et rapide aux données
static bool saveFrameBuffer(FrameBuffer *buffer, const std::string &filename) {
    // 1. Ouvrir le fichier de destination
    std::string filepath = "/home/rpi0/images/" + filename;
    int fd_out = open(filepath.c_str(), O_WRONLY | O_CREAT, 0666);
    if (fd_out < 0) {
        std::cerr << "Erreur: Impossible d'ouvrir le fichier de destination: " << filepath << std::endl;
        return false;
    }

    // Le format RAW du capteur IMX708 (4608x2592) est généralement stocké sur une seule 'plane'
    // Nous traitons la première (et unique) plane de données brutes
    const FrameBuffer::Plane &plane = buffer->planes()[0];
    
    // Le file descriptor (fd) contient le chemin vers les données brutes allouées en mémoire vive
    int fd_in = plane.fd.get();
    size_t size = plane.length; // La taille totale des données RAW
    
    // 2. Mapping mémoire (mmap) : accès direct aux données brutes
    // Ceci est plus rapide que la lecture/écriture classique
    void *data = mmap(NULL, size, PROT_READ, MAP_SHARED, fd_in, 0);
    if (data == MAP_FAILED) {
        std::cerr << "Erreur: mmap a échoué lors de l'accès aux données." << std::endl;
        close(fd_out);
        return false;
    }

    // 3. Écriture des données sur le fichier de destination (sur la clé USB)
    if (write(fd_out, data, size) != size) {
        std::cerr << "Erreur: Échec de l'écriture complète du buffer." << std::endl;
        munmap(data, size);
        close(fd_out);
        return false;
    }

    // 4. Nettoyage
    munmap(data, size);
    close(fd_out);

    std::cout << "  [SAUVEGARDE] Fichier écrit: " << filename 
              << " (" << size / (1024 * 1024.0) << " MB)" << std::endl;
    return true;
}


// Callback appelé quand une photo est capturée
static void requestComplete(Request *request)
{
    // Verrouillage de la section critique (mémoire partagée)
    std::lock_guard<std::mutex> lock(mtx);
    
    if (request->status() == Request::RequestCancelled) {
        std::cerr << "Requête annulée" << std::endl;
        return;
    }

    if (request->status() == Request::RequestComplete) {
        
        const std::map<const Stream *, FrameBuffer *> &buffers = request->buffers();
        
        for (auto const &bufferPair : buffers) {
            FrameBuffer *buffer = bufferPair.second;
            const FrameMetadata &metadata = buffer->metadata();
            
            // Affichage des métadonnées et de la taille du buffer
            std::cout << "\n[CALLBACK] Photo capturée (seq: " << std::setw(6) << std::setfill('0') 
                      << metadata.sequence << ")";
            
            // Logique de sauvegarde
            if (buffer->planes().empty() || buffer->planes()[0].length == 0) {
                std::cerr << "Erreur: Buffer vide ou taille de plane zéro." << std::endl;
                continue;
            }

            // Incrémenter le compteur de photos pour le nom de fichier
            photoCounter++;
            std::string filename = generateFilename(photoCounter);

            // Appel de la fonction de sauvegarde sur le file descriptor
            saveFrameBuffer(buffer, filename);
        }
        
        // Signalement au thread principal que l'opération est terminée
        photoReady = true;
        cv.notify_one();
    }
}


int main()
{
    // 1. Initialisation du camera manager
    std::unique_ptr<CameraManager> cm = std::make_unique<CameraManager>();
    if (cm->start()) {
        std::cerr << "Échec du démarrage du CameraManager" << std::endl;
        return EXIT_FAILURE;
    }

    // Sélection de la première caméra
    auto cameras = cm->cameras();
    if (cameras.empty()) {
        std::cout << "Aucune caméra détectée sur le système." << std::endl;
        cm->stop();
        return EXIT_FAILURE;
    }

    std::string cameraId = cameras[0]->id();
    camera = cm->get(cameraId);
    
    // 2. Acquisition (verrouillage) de la caméra
    if (!camera || camera->acquire()) {
        std::cerr << "Échec de l'acquisition de la caméra" << std::endl;
        cm->stop();
        return EXIT_FAILURE;
    }

    // 3. Configuration pour capture photo RAW (StillCapture)
    std::unique_ptr<CameraConfiguration> config = 
        camera->generateConfiguration({StreamRole::StillCapture});
    
    if (!config) {
        std::cerr << "Échec de génération de la configuration" << std::endl;
        camera->release();
        cm->stop();
        return EXIT_FAILURE;
    }

    StreamConfiguration &streamConfig = config->at(0);
    
    // Configuration de la résolution MAXIMALE
    streamConfig.size.width = 4608;
    streamConfig.size.height = 2592;
    // IMPORTANT: Le format sera RAW/DNG (SBGGR12, par exemple) si le mode StillCapture est utilisé.
    // Pour forcer le RAW sur tous les capteurs, on peut aussi utiliser: streamConfig.pixelFormat = libcamera::formats::SBGGR12; 

    // Validation et application de la configuration
    config->validate();
    std::cout << "Configuration validée: " << streamConfig.toString() << std::endl;

    if (camera->configure(config.get())) {
        std::cerr << "Échec de configuration de la caméra" << std::endl;
        camera->release();
        cm->stop();
        return EXIT_FAILURE;
    }

    // 4. Allocation des buffers
    FrameBufferAllocator *allocator = new FrameBufferAllocator(camera);
    Stream *stream = streamConfig.stream();

    if (allocator->allocate(stream) < 0) {
        std::cerr << "Impossible d'allouer les buffers" << std::endl;
        delete allocator;
        camera->release();
        cm->stop();
        return EXIT_FAILURE;
    }

    // 5. Connexion du callback et DÉMARRAGE de la caméra
    camera->requestCompleted.connect(requestComplete);
    if (camera->start()) {
        std::cerr << "Échec du démarrage de la caméra" << std::endl;
        delete allocator;
        camera->release();
        cm->stop();
        return EXIT_FAILURE;
    }

    std::cout << "\n=== Caméra prête (Mode RAW 4608x2592) ===" << std::endl;
    std::cout << "Destination des fichiers: /home/rpi0/images\n" << std::endl;

    // 6. Boucle principale pour capturer des photos à la demande
    const std::vector<std::unique_ptr<FrameBuffer>> &buffers = allocator->buffers(stream);
    
    /*std::string input;
    while (true) {
        std::cout << "Appuyez sur Entrée pour capturer une photo (ou 'q' + Entrée pour quitter): ";
        std::getline(std::cin, input);
        
        if (input == "q" || input == "Q") break;

        // Création et association d'une requête
        std::unique_ptr<Request> request = camera->createRequest();
        if (!request || request->addBuffer(stream, buffers[0].get()) < 0) {
            std::cerr << "Erreur: Problème lors de la création ou de l'association de la requête." << std::endl;
            continue;
        }

        // Réinitialisation et ENVOI de la requête (déclenchement)
        {
            std::lock_guard<std::mutex> lock(mtx);
            photoReady = false;
        }

        std::cout << "Capture en cours (Attente du Callback)..." << std::endl;
        camera->queueRequest(request.get());

        // Attente asynchrone que le callback signale la fin de la capture
        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [] { return photoReady; });
        }

        std::cout << "Opération terminée! (Photo sauvée sur disque)" << std::endl;
    }*/

    // Mesure de la durée totale du test
    auto start_test = high_resolution_clock::now();

    for (int i =0;i<30;i+=1){

        // Création et association d'une requête
        std::unique_ptr<Request> request = camera->createRequest();
        if (!request || request->addBuffer(stream, buffers[0].get()) < 0) {
            std::cerr << "Erreur: Problème lors de la création ou de l'association de la requête." << std::endl;
            continue;
        }

        // Réinitialisation et ENVOI de la requête (déclenchement)
        {
            std::lock_guard<std::mutex> lock(mtx);
            photoReady = false;
        }

        std::cout << "Capture en cours (Attente du Callback)..." << std::endl;
        camera->queueRequest(request.get());

        // Attente asynchrone que le callback signale la fin de la capture
        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [] { return photoReady; });
        }

        std::cout << "Opération terminée! (Photo sauvée sur disque)" << std::endl;
    }

    // Mesure de la durée totale du test (FIN)
    auto end_test = high_resolution_clock::now();
    auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_test - start_test);

    std::cout << "Temps total pour les 30 prises de vue :" << total_duration.count() << "ms" <<std::endl;
    std::cout << "Temps moyen:" << total_duration.count()/30 << "ms" <<std::endl;

    // 7. Nettoyage
    camera->stop();
    camera->requestCompleted.disconnect(requestComplete);
    delete allocator;
    camera->release();
    camera.reset();
    cm->stop();

    return EXIT_SUCCESS;
}