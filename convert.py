#!/usr/bin/env python3
import numpy as np
import cv2
import os

def convert_raw_to_jpg(raw_file, output_file, width=4608, height=2592):
    # Récupère la taille du fichier
    file_size = os.path.getsize(raw_file)
    print(f"Taille du fichier: {file_size} octets")
    print(f"Résolution: {width}x{height}")
    
    # Calcul des tailles attendues pour différents formats
    formats = {
        'RGB24': width * height * 3,
        'RGBA': width * height * 4,
        'YUV420': width * height * 3 // 2,
        'YUYV': width * height * 2,
        'GRAY': width * height,
        'BGR24': width * height * 3,
    }
    
    print("\nTailles attendues par format:")
    for fmt, size in formats.items():
        match = "✓ CORRESPONDANCE" if size == file_size else ""
        print(f"  {fmt}: {size} octets {match}")
    
    # Lecture des données brutes
    raw_data = np.fromfile(raw_file, dtype=np.uint8)
    
    # Détection automatique du format
    try:
        if file_size == formats['YUV420']:
            print("\n→ Conversion depuis YUV420...")
            yuv_image = raw_data.reshape((height * 3 // 2, width))
            bgr_image = cv2.cvtColor(yuv_image, cv2.COLOR_YUV2BGR_I420)
            
        elif file_size == formats['YUYV']:
            print("\n→ Conversion depuis YUYV...")
            yuv_image = raw_data.reshape((height, width, 2))
            bgr_image = cv2.cvtColor(yuv_image, cv2.COLOR_YUV2BGR_YUYV)
            
        elif file_size == formats['RGB24']:
            print("\n→ Conversion depuis RGB24...")
            rgb_image = raw_data.reshape((height, width, 3))
            bgr_image = cv2.cvtColor(rgb_image, cv2.COLOR_RGB2BGR)
            
        elif file_size == formats['BGR24']:
            print("\n→ Format BGR24 (déjà correct)...")
            bgr_image = raw_data.reshape((height, width, 3))
            
        elif file_size == formats['RGBA']:
            print("\n→ Conversion depuis RGBA...")
            rgba_image = raw_data.reshape((height, width, 4))
            bgr_image = cv2.cvtColor(rgba_image, cv2.COLOR_RGBA2BGR)
            
        elif file_size == formats['GRAY']:
            print("\n→ Conversion depuis GRAY (niveaux de gris)...")
            bgr_image = raw_data.reshape((height, width))
            bgr_image = cv2.cvtColor(bgr_image, cv2.COLOR_GRAY2BGR)
            
        else:
            # Tentative avec la taille réelle
            pixels_total = file_size
            bytes_per_pixel = file_size / (width * height)
            print(f"\n⚠ Format non standard détecté")
            print(f"Octets par pixel: {bytes_per_pixel:.2f}")
            
            if bytes_per_pixel == 1:
                print("→ Tentative en niveaux de gris...")
                bgr_image = raw_data.reshape((height, width))
                bgr_image = cv2.cvtColor(bgr_image, cv2.COLOR_GRAY2BGR)
            elif bytes_per_pixel >= 2 and bytes_per_pixel < 3:
                print("→ Tentative en YUYV...")
                yuv_image = raw_data[:height * width * 2].reshape((height, width, 2))
                bgr_image = cv2.cvtColor(yuv_image, cv2.COLOR_YUV2BGR_YUYV)
            else:
                print("→ Tentative en RGB24...")
                bgr_image = raw_data[:height * width * 3].reshape((height, width, 3))
                bgr_image = cv2.cvtColor(bgr_image, cv2.COLOR_RGB2BGR)
        
        # Sauvegarde de l'image
        cv2.imwrite(output_file, bgr_image)
        print(f"\n✓ Image convertie avec succès: {output_file}")
        print(f"Dimensions finales: {bgr_image.shape[1]}x{bgr_image.shape[0]}")
        
    except Exception as e:
        print(f"\n✗ Erreur lors de la conversion: {e}")
        print("\nSuggestions:")
        print("1. Vérifiez que la résolution (4608x2592) est correcte")
        print("2. Modifiez le code C++ pour afficher le format pixel exact")
        print("3. Essayez avec ffmpeg: ffmpeg -f rawvideo -pixel_format <format> -video_size 4608x2592 -i capture.raw output.jpg")

if __name__ == "__main__":
    convert_raw_to_jpg('capture.raw', 'capture.jpg')