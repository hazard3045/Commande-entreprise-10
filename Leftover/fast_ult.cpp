#include <libcamera/camera.h>
#include <libcamera/camera_manager.h>
#include <libcamera/framebuffer_allocator.h>
#include <libcamera/request.h>
#include <libcamera/stream.h>
#include <libcamera/control_ids.h>
#include <libcamera/property_ids.h>

#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <fstream>
#include <chrono>
#include <cstring>
#include <memory>
#include <sys/mman.h>
#include <signal.h>

using namespace libcamera;
using namespace std::chrono_literals;

// Structure pour le double buffering
struct FrameBuffer {
    std::vector<uint8_t> data;
    size_t size;
    bool ready;
    uint64_t timestamp;
    
    FrameBuffer() : size(0), ready(false), timestamp(0) {}
};

class OptimizedRAWCapture {
private:
    std::unique_ptr<CameraManager> cm;
    std::shared_ptr<Camera> camera;
    std::unique_ptr<CameraConfiguration> config;
    std::unique_ptr<FrameBufferAllocator> allocator;
    
    // Double buffer
    FrameBuffer buffers[2];
    std::atomic<int> writeBuffer{0};
    std::atomic<int> readBuffer{1};
    
    // Synchronisation
    std::mutex mtx;
    std::condition_variable cv;
    std::atomic<bool> frameReady{false};
    std::atomic<bool> running{true};
    std::atomic<int> frameCount{0};
    
    // Thread de sauvegarde
    std::thread saveThread;
    std::queue<std::pair<std::vector<uint8_t>, uint64_t>> saveQueue;
    std::mutex saveMtx;
    std::condition_variable saveCv;
    
    Stream *rawStream;
    std::vector<std::unique_ptr<Request>> requests;

public:
    OptimizedRAWCapture() : rawStream(nullptr) {}
    
    ~OptimizedRAWCapture() {
        stop();
        if (saveThread.joinable()) {
            running.store(false);
            saveCv.notify_all();
            saveThread.join();
        }
        cleanup();
    }
    
    bool initialize() {
        std::cout << "Initialisation du CameraManager..." << std::endl;
        
        // Initialisation du CameraManager
        cm = std::make_unique<CameraManager>();
        int ret = cm->start();
        if (ret) {
            std::cerr << "Erreur démarrage CameraManager: " << ret << std::endl;
            return false;
        }
        
        // Récupération de la caméra
        if (cm->cameras().empty()) {
            std::cerr << "Aucune caméra détectée" << std::endl;
            return false;
        }
        
        std::cout << "Nombre de caméras détectées: " << cm->cameras().size() << std::endl;
        
        camera = cm->cameras()[0];
        std::cout << "Caméra sélectionnée: " << camera->id() << std::endl;
        
        if (camera->acquire()) {
            std::cerr << "Erreur acquisition caméra" << std::endl;
            return false;
        }
        
        std::cout << "Caméra acquise" << std::endl;
        
        // Configuration pour RAW maximum qualité
        config = camera->generateConfiguration({StreamRole::Raw});
        if (!config || config->empty()) {
            std::cerr << "Erreur génération configuration" << std::endl;
            return false;
        }
        
        StreamConfiguration &streamConfig = config->at(0);
        rawStream = streamConfig.stream();
        
        std::cout << "Configuration par défaut: " << streamConfig.toString() << std::endl;
        
        // Configuration IMX708 en résolution maximale RAW
        // Formats possibles: SRGGB10_CSI2P, SRGGB12, SBGGR10_CSI2P
        streamConfig.pixelFormat = formats::SRGGB10_CSI2P;
        streamConfig.size = Size(4608, 2592); // Résolution max IMX708
        streamConfig.bufferCount = 6; // Buffers matériels pour pipeline
        
        // Validation
        CameraConfiguration::Status validation = config->validate();
        if (validation == CameraConfiguration::Invalid) {
            std::cerr << "Configuration invalide" << std::endl;
            return false;
        } else if (validation == CameraConfiguration::Adjusted) {
            std::cout << "Configuration ajustée automatiquement" << std::endl;
        }
        
        std::cout << "Configuration finale: " << streamConfig.toString() << std::endl;
        std::cout << "Format pixel: " << streamConfig.pixelFormat.toString() << std::endl;
        std::cout << "Taille: " << streamConfig.size.toString() << std::endl;
        
        // Application de la configuration
        if (camera->configure(config.get())) {
            std::cerr << "Erreur configuration caméra" << std::endl;
            return false;
        }
        
        std::cout << "Configuration appliquée" << std::endl;
        
        // Allocation des buffers
        allocator = std::make_unique<FrameBufferAllocator>(camera);
        ret = allocator->allocate(rawStream);
        if (ret < 0) {
            std::cerr << "Erreur allocation buffers: " << ret << std::endl;
            return false;
        }
        
        std::cout << "Buffers alloués: " << ret << std::endl;
        
        // Préparation des buffers logiciels
        size_t bufferSize = streamConfig.frameSize;
        buffers[0].data.resize(bufferSize);
        buffers[0].ready = false;
        buffers[1].data.resize(bufferSize);
        buffers[1].ready = false;
        
        std::cout << "Buffers logiciels créés: " << bufferSize << " bytes chacun" << std::endl;
        
        return true;
    }
    
    bool start() {
        std::cout << "Préparation des requêtes..." << std::endl;
        
        const std::vector<std::unique_ptr<libcamera::FrameBuffer>> &fbuffers = 
            allocator->buffers(rawStream);
        
        // Création des requêtes
        for (unsigned int i = 0; i < fbuffers.size(); ++i) {
            std::unique_ptr<Request> request = camera->createRequest();
            if (!request) {
                std::cerr << "Erreur création requête " << i << std::endl;
                return false;
            }
            
            if (request->addBuffer(rawStream, fbuffers[i].get())) {
                std::cerr << "Erreur ajout buffer à la requête " << i << std::endl;
                return false;
            }
            
            // Optimisations pour vitesse maximale
            ControlList &controls = request->controls();
            controls.set(controls::AeEnable, false); // Désactive auto-exposition
            controls.set(controls::AwbEnable, false); // Désactive auto white balance
            controls.set(controls::ExposureTime, 10000); // 10ms exposition
            controls.set(controls::AnalogueGain, 1.0f); // Gain minimal
            
            requests.push_back(std::move(request));
        }
        
        std::cout << requests.size() << " requêtes créées" << std::endl;
        
        // Callback de traitement
        camera->requestCompleted.connect(this, &OptimizedRAWCapture::requestComplete);
        
        // Démarrage du thread de sauvegarde
        saveThread = std::thread(&OptimizedRAWCapture::saveThreadFunc, this);
        
        // Démarrage de la caméra
        if (camera->start()) {
            std::cerr << "Erreur démarrage caméra" << std::endl;
            return false;
        }
        
        std::cout << "Caméra démarrée" << std::endl;
        
        // Soumission des requêtes
        for (auto &request : requests) {
            camera->queueRequest(request.get());
        }
        
        std::cout << "Requêtes soumises, capture en cours..." << std::endl;
        return true;
    }
    
    void requestComplete(Request *request) {
        if (request->status() == Request::RequestCancelled) {
            return;
        }
        
        if (request->status() == Request::RequestComplete) {
            // Récupération du buffer RAW
            libcamera::FrameBuffer *buffer = request->buffers().begin()->second;
            const libcamera::FrameBuffer::Plane &plane = buffer->planes()[0];
            
            // Copie rapide vers le buffer d'écriture
            int wBuf = writeBuffer.load();
            
            void *mem = mmap(nullptr, plane.length, PROT_READ, MAP_SHARED, 
                             plane.fd.get(), 0);
            
            if (mem != MAP_FAILED) {
                std::memcpy(buffers[wBuf].data.data(), mem, plane.length);
                buffers[wBuf].size = plane.length;
                
                // Récupération du timestamp
                const ControlList &metadata = request->metadata();
                if (metadata.contains(controls::SensorTimestamp)) {
                    buffers[wBuf].timestamp = metadata.get(controls::SensorTimestamp).value();
                } else {
                    buffers[wBuf].timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
                }
                
                buffers[wBuf].ready = true;
                munmap(mem, plane.length);
                
                // Swap des buffers (atomic)
                int rBuf = readBuffer.load();
                writeBuffer.store(rBuf);
                readBuffer.store(wBuf);
                
                frameReady.store(true);
                cv.notify_one();
                
                frameCount++;
            } else {
                std::cerr << "Erreur mmap" << std::endl;
            }
        }
        
        // Re-soumission de la requête
        request->reuse(Request::ReuseBuffers);
        camera->queueRequest(request);
    }
    
    bool captureFrame() {
        std::unique_lock<std::mutex> lock(mtx);
        
        // Attente d'une frame avec timeout
        if (!cv.wait_for(lock, 2s, [this] { return frameReady.load(); })) {
            std::cerr << "Timeout attente frame" << std::endl;
            return false;
        }
        
        int rBuf = readBuffer.load();
        if (buffers[rBuf].ready) {
            // Ajout à la queue de sauvegarde
            {
                std::lock_guard<std::mutex> saveLock(saveMtx);
                saveQueue.push({buffers[rBuf].data, buffers[rBuf].timestamp});
                buffers[rBuf].ready = false;
            }
            saveCv.notify_one();
        }
        
        frameReady.store(false);
        return true;
    }
    
    void saveThreadFunc() {
        int savedCount = 0;
        while (running.load() || !saveQueue.empty()) {
            std::unique_lock<std::mutex> lock(saveMtx);
            
            if (saveQueue.empty()) {
                saveCv.wait_for(lock, 100ms);
                continue;
            }
            
            auto frame = saveQueue.front();
            saveQueue.pop();
            lock.unlock();
            
            // Sauvegarde asynchrone
            std::string filename = "raw_" + std::to_string(frame.second) + ".raw";
            std::ofstream file(filename, std::ios::binary);
            if (file.is_open()) {
                file.write(reinterpret_cast<const char*>(frame.first.data()), 
                          frame.first.size());
                file.close();
                savedCount++;
                std::cout << "Sauvegardé (" << savedCount << "): " << filename 
                         << " (" << (frame.first.size() / 1024 / 1024) << " MB)" << std::endl;
            } else {
                std::cerr << "Erreur ouverture fichier: " << filename << std::endl;
            }
        }
        std::cout << "Thread de sauvegarde terminé. Total: " << savedCount << " images" << std::endl;
    }
    
    void stop() {
        std::cout << "Arrêt de la capture..." << std::endl;
        
        if (camera && camera->isRunning()) {
            camera->stop();
            std::cout << "Caméra arrêtée" << std::endl;
        }
        
        running.store(false);
        saveCv.notify_all();
    }
    
    void cleanup() {
        requests.clear();
        allocator.reset();
        
        if (camera) {
            camera->release();
            camera.reset();
        }
        
        if (cm) {
            cm->stop();
        }
        
        std::cout << "Nettoyage terminé" << std::endl;
    }
    
    int getFrameCount() const {
        return frameCount.load();
    }
};

// Gestionnaire de signal pour arrêt propre
static std::atomic<bool> shouldExit{false};
void signalHandler(int signal) {
    std::cout << "\nSignal reçu, arrêt..." << std::endl;
    shouldExit.store(true);
}

int main(int argc, char *argv[]) {
    // Installation du gestionnaire de signal
    signal(SIGINT, signalHandler);
    
    int numFrames = 10;
    if (argc > 1) {
        numFrames = std::atoi(argv[1]);
    }
    
    std::cout << "=== Capture RAW optimisée pour IMX708 ===" << std::endl;
    std::cout << "Nombre d'images à capturer: " << numFrames << std::endl;
    
    OptimizedRAWCapture capture;
    
    if (!capture.initialize()) {
        std::cerr << "Erreur initialisation" << std::endl;
        return 1;
    }
    
    if (!capture.start()) {
        std::cerr << "Erreur démarrage" << std::endl;
        return 1;
    }
    
    // Capture des images
    for (int i = 0; i < numFrames && !shouldExit.load(); i++) {
        std::cout << "\n--- Capture frame " << (i+1) << "/" << numFrames << " ---" << std::endl;
        
        if (!capture.captureFrame()) {
            std::cerr << "Erreur capture frame " << (i+1) << std::endl;
            break;
        }
        
        // Petite pause entre les captures
        std::this_thread::sleep_for(100ms);
    }
    
    std::cout << "\nArrêt et sauvegarde des dernières images..." << std::endl;
    capture.stop();
    
    // Attente de la fin des sauvegardes
    std::this_thread::sleep_for(2s);
    
    std::cout << "\n=== Capture terminée ===" << std::endl;
    std::cout << "Total frames capturées: " << capture.getFrameCount() << std::endl;
    
    return 0;
}