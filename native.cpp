// à compiler avec:  g++ -o nat native.cpp $(pkg-config --cflags --libs libcamera) -lpigpio -std=c++17

#include <iomanip>
#include <iostream>
#include <memory>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <chrono>
#include <sstream>
#include <fstream>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <pigpio.h>

#include <libcamera/libcamera.h>
#include <libcamera/control_ids.h>
#include <libcamera/property_ids.h>

#ifdef HAVE_DNG_WRITER
#include "dng_writer.h"
#endif

using namespace libcamera;
using namespace std::chrono_literals;
using namespace std::chrono;
using namespace std;

int clk_externe; // la clock externe donné par le GPS 
int clk_interne; 
int temps_total_prise_de_vue = 900; //temps total de prise de vue en secondes, NE PAS DÉBRANCHER AVANT

static std::shared_ptr<Camera> camera;
static std::mutex mtx;
static std::condition_variable cv;
static bool photoReady = false; // True si il y a eu une impulsion False sinon 
static int photoCounter = 0; // compteur d'impulsion 
int gpio_imp = 17; // GPIO pour les impulsions
int gpio_clk = 27; // GPIO pour la clock externe


// Fonctions callback pour impulsions et horloge
void rising_callback_clk(int gpio, int level, uint32_t tick) {
    if (level == 1){
        clk_externe += 1;
    }
}

void rising_callback_impul(int gpio, int level, uint32_t tick) {
    if (level == 1){
        photoReady = true;
        photoCounter += 1;
    }
}

static std::string generateFilename(int index) {
    std::ostringstream oss;
    oss << "photo_" << std::setw(4) << std::setfill('0') << index << std::setw(4) << std::setfill('0') << to_string(clk_externe) << std::setw(12) << std::setfill('0')<< gpioTick() << ".dng";
    return oss.str();
}

static bool saveFrameBufferWithDNG(FrameBuffer *buffer, const std::string &filename, 
                                    const ControlList &metadata, 
                                    const StreamConfiguration &streamConfig) {
    std::string filepath = "/home/rpi0/images/" + filename;
    
    const FrameBuffer::Plane &plane = buffer->planes()[0];
    int fd_in = plane.fd.get();
    size_t size = plane.length;
    
    // Mapping mémoire pour accéder aux données RAW
    void *data = mmap(NULL, size, PROT_READ, MAP_SHARED, fd_in, 0);
    if (data == MAP_FAILED) {
        std::cerr << "Erreur: mmap a échoué." << std::endl;
        return false;
    }

    std::string rawpath = filepath;
    rawpath.replace(rawpath.length() - 4, 4, ".raw");
    
    int fd_out = open(rawpath.c_str(), O_WRONLY | O_CREAT, 0666);
    if (fd_out < 0) {
        std::cerr << "Erreur: Impossible d'ouvrir " << rawpath << std::endl;
        munmap(data, size);
        return false;
    }

    if (write(fd_out, data, size) != size) {
        std::cerr << "Erreur: Échec de l'écriture." << std::endl;
        munmap(data, size);
        close(fd_out);
        return false;
    }

    munmap(data, size);
    close(fd_out);

    // Créer un fichier .info avec les métadonnées pour reconstruction ultérieure
    std::string infopath = rawpath + ".info";
    std::ofstream info(infopath);
    info << "width=" << streamConfig.size.width << "\n";
    info << "height=" << streamConfig.size.height << "\n";
    info << "format=" << streamConfig.pixelFormat.toString() << "\n";
    info << "stride=" << streamConfig.stride << "\n";
    info.close();

    std::cout << "  [RAW] Fichier écrit: " << rawpath 
              << " (" << size / (1024 * 1024.0) << " MB)" << std::endl;
    std::cout << "        Métadonnées: " << infopath << std::endl;
    std::cout << "        Note: Convertir avec raw2dng ou le script Python fourni" << std::endl;
    
    return true;

}

// Callback modifié pour passer les métadonnées
static StreamConfiguration *globalStreamConfig = nullptr;

static void requestComplete(Request *request)
{
    std::lock_guard<std::mutex> lock(mtx);
    
    if (request->status() == Request::RequestCancelled) {
        std::cerr << "Requête annulée" << std::endl;
        return;
    }

    if (request->status() == Request::RequestComplete) {
        const std::map<const Stream *, FrameBuffer *> &buffers = request->buffers();
        
        // Récupérer les métadonnées de la requête
        const ControlList &metadata = request->metadata();
        
        for (auto const &bufferPair : buffers) {
            FrameBuffer *buffer = bufferPair.second;
            const FrameMetadata &frameMeta = buffer->metadata();
            
            std::cout << "\n[CALLBACK] Photo capturée (seq: " << std::setw(6) 
                      << std::setfill('0') << frameMeta.sequence << ")";
            
            if (buffer->planes().empty() || buffer->planes()[0].length == 0) {
                std::cerr << "Erreur: Buffer vide" << std::endl;
                continue;
            }

            photoCounter++;
            std::string filename = generateFilename(photoCounter);

            if (globalStreamConfig) {
                saveFrameBufferWithDNG(buffer, filename, metadata, *globalStreamConfig);
            } else {
                std::cerr << "Erreur: StreamConfig non disponible" << std::endl;
            }
        }
        
        photoReady = true;
        cv.notify_one();
    }
}

int main()
{
    std::unique_ptr<CameraManager> cm = std::make_unique<CameraManager>();
    if (cm->start()) {
        std::cerr << "Échec du démarrage du CameraManager" << std::endl;
        return EXIT_FAILURE;
    }

    auto cameras = cm->cameras();
    if (cameras.empty()) {
        std::cout << "Aucune caméra détectée." << std::endl;
        cm->stop();
        return EXIT_FAILURE;
    }

    std::string cameraId = cameras[0]->id();
    camera = cm->get(cameraId);
    
    if (!camera || camera->acquire()) {
        std::cerr << "Échec de l'acquisition de la caméra" << std::endl;
        cm->stop();
        return EXIT_FAILURE;
    }

    std::unique_ptr<CameraConfiguration> config = 
        camera->generateConfiguration({StreamRole::StillCapture});
    
    if (!config) {
        std::cerr << "Échec de génération de la configuration" << std::endl;
        camera->release();
        cm->stop();
        return EXIT_FAILURE;
    }

    StreamConfiguration &streamConfig = config->at(0);
    streamConfig.size.width = 4608;
    streamConfig.size.height = 2592;
    
    // FORCER LE FORMAT RAW BAYER (très important!)
    // Pour IMX708 (Camera v3), utiliser SBGGR10_CSI2P ou SBGGR12_CSI2P
    streamConfig.pixelFormat = formats::SBGGR10_CSI2P;  // ou SBGGR12_CSI2P
    // Alternatives selon la caméra:
    // formats::SRGGB10_CSI2P, formats::SGRBG10_CSI2P, formats::SGBRG10_CSI2P

    config->validate();
    std::cout << "Configuration validée: " << streamConfig.toString() << std::endl;

    if (camera->configure(config.get())) {
        std::cerr << "Échec de configuration de la caméra" << std::endl;
        camera->release();
        cm->stop();
        return EXIT_FAILURE;
    }

    // Sauvegarder le pointeur vers la config pour le callback
    globalStreamConfig = &streamConfig;

    FrameBufferAllocator *allocator = new FrameBufferAllocator(camera);
    Stream *stream = streamConfig.stream();

    if (allocator->allocate(stream) < 0) {
        std::cerr << "Impossible d'allouer les buffers" << std::endl;
        delete allocator;
        camera->release();
        cm->stop();
        return EXIT_FAILURE;
    }

    camera->requestCompleted.connect(requestComplete);
    if (camera->start()) {
        std::cerr << "Échec du démarrage de la caméra" << std::endl;
        delete allocator;
        camera->release();
        cm->stop();
        return EXIT_FAILURE;
    }
    
    // Attendre que l'AE/AWB se stabilise
    std::cout << "Attente stabilisation AE/AWB (3 secondes)..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(3));

    std::cout << "\n=== Caméra prête (Mode RAW 4608x2592) ===" << std::endl;
    std::cout << "Destination: /home/rpi0/images\n" << std::endl;

    const std::vector<std::unique_ptr<FrameBuffer>> &buffers = allocator->buffers(stream);

    // Initialisation gpio et interruptions
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

    while (clk_externe < temps_total_prise_de_vue){
        if (photoReady){
            photoReady = false;
            std::unique_ptr<Request> request = camera->createRequest();
            if (!request || request->addBuffer(stream, buffers[0].get()) < 0) {
                std::cerr << "Erreur: Problème lors de la création de la requête." << std::endl;
                continue;
            }
            
            // Configurer l'exposition manuelle si nécessaire
            // Décommenter ces lignes si l'image est trop sombre
            request->controls().set(controls::ExposureTime, 20000);  // 20ms
            request->controls().set(controls::AnalogueGain, 2.0);     // Gain x2

            {
                std::lock_guard<std::mutex> lock(mtx);
                photoReady = false;
            }

            camera->queueRequest(request.get());

            {
                std::unique_lock<std::mutex> lock(mtx);
                cv.wait(lock, [] { return photoReady; });
            }
        }

        usleep(1000); // Attendre 1 ms pour éviter une boucle trop rapide

    }


    camera->stop();
    camera->requestCompleted.disconnect(requestComplete);
    delete allocator;
    camera->release();
    camera.reset();
    cm->stop();

    return EXIT_SUCCESS;
}
