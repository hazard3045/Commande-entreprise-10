#!/usr/bin/env python3
"""
Convertit les fichiers RAW Raspberry Pi en TIFF 16-bit debayérisé
"""

import numpy as np
import sys
import os
import glob
from PIL import Image

def lire_info(fichier_info):
    """Lit le fichier .info"""
    info = {}
    try:
        with open(fichier_info, 'r') as f:
            for line in f:
                if '=' in line:
                    key, value = line.strip().split('=', 1)
                    info[key] = value
        return info
    except FileNotFoundError:
        return None

def unpack_10bit_csi2p(data, width, height, stride):
    """Dépaquette 10-bit CSI2P vers uint16"""

    
    packed = np.frombuffer(data, dtype=np.uint8)
    unpacked = np.zeros((height, width), dtype=np.uint16)
    
    # ------------------ CALCUL CRITIQUE ------------------
    # 4 pixels sont codés dans 5 octets (un groupe).
    # Calculer le nombre de groupes de 5 octets nécessaires pour la largeur totale des pixels.
    num_groups = width // 4
    if width % 4 != 0:
        num_groups += 1 # Gérer les pixels restants s'il y en a

    # La longueur des données réelles empaquetées pour les pixels (sans le padding)
    packed_data_length = num_groups * 5
    # ----------------------------------------------------
    
    packed_line_length = stride # Ceci est la taille totale de la ligne, y compris le padding
    
    print(f"   • Groupes/Ligne: {num_groups}. Données/Ligne: {packed_data_length} octets.")
    print(f"   • Stride (total ligne): {packed_line_length} octets.")
    
    for row in range(height):
        row_offset = row * packed_line_length # Début de la ligne empaquetée
        
        # Parcourir uniquement les groupes de données utiles (jusqu'à num_groups)
        for group in range(num_groups):
            # L'offset est basé sur la position dans la ligne empaquetée
            offset = row_offset + group * 5
            
            # Vérification de sécurité (essentielle)
            if offset + 4 >= len(packed) or (group * 4) >= width:
                break
            
            b0, b1, b2, b3, b4 = packed[offset:offset+5]
            
            # Reconstruire 4 pixels 10-bit (Cette logique est correcte pour CSI2P)
            p0 = (b0 << 2) | ((b4 >> 0) & 0x3)
            p1 = (b1 << 2) | ((b4 >> 2) & 0x3)
            p2 = (b2 << 2) | ((b4 >> 4) & 0x3)
            p3 = (b3 << 2) | ((b4 >> 6) & 0x3)
            
            col = group * 4
            
            # Appliquer les pixels, en faisant attention aux bords
            pixels_to_write = [p0, p1, p2, p3]
            
            # Écrire uniquement le nombre de pixels qui existent réellement dans le groupe final
            unpacked[row, col : col + min(4, width - col)] = pixels_to_write[:min(4, width - col)]
            
    # ... (Le reste de la fonction est inchangé, incluant la normalisation 10-bit -> 16-bit)
    stats_min, stats_max, stats_mean = unpacked.min(), unpacked.max(), unpacked.mean()
    print(f"   Valeurs 10-bit: Min={stats_min}, Max={stats_max}, Moy={stats_mean:.1f}")
    
    # Normaliser vers 16-bit (multiplier par 64 pour 10bit→16bit)
    unpacked = (unpacked << 6).astype(np.uint16)
    
    print(f"  Normalisé 16-bit: Min={unpacked.min()}, Max={unpacked.max()}, Moy={unpacked.mean():.1f}")
    
    return unpacked

def test_offset(bayer_array, h_offset=0, v_offset=0):
    """Décalage (roll) cyclique pour tester l'alignement"""
    if h_offset != 0 or v_offset != 0:
        print(f"  Décalage cyclique appliqué: H={h_offset}, V={v_offset}")
    return np.roll(bayer_array, (v_offset, h_offset), axis=(0, 1))

def debayer_simple_rapide(bayer, pattern='RGGB'):
    """
    Debayering simple et robuste (nearest-neighbor avec amélioration)
    Retourne une image RGB 16-bit
    """
    print(f"  Debayering {pattern}...")
    
    height, width = bayer.shape
    rgb = np.zeros((height, width, 3), dtype=np.uint16)
    
    if pattern == 'BGGR':
        # Pattern BGGR: B(0,0) G(0,1) / G(1,0) R(1,1)
        
        # Bleu: lignes paires, colonnes paires
        rgb[0::2, 0::2, 2] = bayer[0::2, 0::2]
        # Interpolation simple (répétition + moyenne)
        rgb[0::2, 1::2, 2] = bayer[0::2, 0::2]
        rgb[1::2, 0::2, 2] = bayer[0::2, 0::2]
        rgb[1::2, 1::2, 2] = bayer[0::2, 0::2]
        
        # Vert G1: lignes paires, colonnes impaires
        # Vert G2: lignes impaires, colonnes paires
        rgb[0::2, 1::2, 1] = bayer[0::2, 1::2]
        rgb[1::2, 0::2, 1] = bayer[1::2, 0::2]
        # Moyenne pour les autres positions
        rgb[0::2, 0::2, 1] = (bayer[0::2, 1::2].astype(np.uint32) + 
                               bayer[1::2, 0::2].astype(np.uint32)) // 2
        rgb[1::2, 1::2, 1] = (bayer[0::2, 1::2].astype(np.uint32) + 
                               bayer[1::2, 0::2].astype(np.uint32)) // 2
        
        # Rouge: lignes impaires, colonnes impaires
        rgb[1::2, 1::2, 0] = bayer[1::2, 1::2]
        rgb[1::2, 0::2, 0] = bayer[1::2, 1::2]
        rgb[0::2, 1::2, 0] = bayer[1::2, 1::2]
        rgb[0::2, 0::2, 0] = bayer[1::2, 1::2]
        
    elif pattern == 'RGGB':
        # Pattern RGGB: R(0,0) G(0,1) / G(1,0) B(1,1)
        
        # Rouge
        rgb[0::2, 0::2, 0] = bayer[0::2, 0::2]
        rgb[0::2, 1::2, 0] = bayer[0::2, 0::2]
        rgb[1::2, 0::2, 0] = bayer[0::2, 0::2]
        rgb[1::2, 1::2, 0] = bayer[0::2, 0::2]
        
        # Vert
        rgb[0::2, 1::2, 1] = bayer[0::2, 1::2]
        rgb[1::2, 0::2, 1] = bayer[1::2, 0::2]
        rgb[0::2, 0::2, 1] = (bayer[0::2, 1::2].astype(np.uint32) + 
                               bayer[1::2, 0::2].astype(np.uint32)) // 2
        rgb[1::2, 1::2, 1] = (bayer[0::2, 1::2].astype(np.uint32) + 
                               bayer[1::2, 0::2].astype(np.uint32)) // 2
        
        # Bleu
        rgb[1::2, 1::2, 2] = bayer[1::2, 1::2]
        rgb[1::2, 0::2, 2] = bayer[1::2, 1::2]
        rgb[0::2, 1::2, 2] = bayer[1::2, 1::2]
        rgb[0::2, 0::2, 2] = bayer[1::2, 1::2]
    
    elif pattern == 'GRBG':
        # Pattern GRBG: G(0,0) R(0,1) / B(1,0) G(1,1)
        
        rgb[0::2, 0::2, 1] = bayer[0::2, 0::2]
        rgb[1::2, 1::2, 1] = bayer[1::2, 1::2]
        rgb[0::2, 1::2, 1] = bayer[0::2, 0::2]
        rgb[1::2, 0::2, 1] = bayer[1::2, 1::2]
        
        rgb[0::2, 1::2, 0] = bayer[0::2, 1::2]
        rgb[0::2, 0::2, 0] = bayer[0::2, 1::2]
        rgb[1::2, :, 0] = bayer[0::2, 1::2]
        
        rgb[1::2, 0::2, 2] = bayer[1::2, 0::2]
        rgb[1::2, 1::2, 2] = bayer[1::2, 0::2]
        rgb[0::2, :, 2] = bayer[1::2, 0::2]
    
    elif pattern == 'GBRG':
        # Pattern GBRG: G(0,0) B(0,1) / R(1,0) G(1,1)
        
        rgb[0::2, 0::2, 1] = bayer[0::2, 0::2]
        rgb[1::2, 1::2, 1] = bayer[1::2, 1::2]
        rgb[0::2, 1::2, 1] = bayer[0::2, 0::2]
        rgb[1::2, 0::2, 1] = bayer[1::2, 1::2]
        
        rgb[0::2, 1::2, 2] = bayer[0::2, 1::2]
        rgb[0::2, 0::2, 2] = bayer[0::2, 1::2]
        rgb[1::2, :, 2] = bayer[0::2, 1::2]
        
        rgb[1::2, 0::2, 0] = bayer[1::2, 0::2]
        rgb[1::2, 1::2, 0] = bayer[1::2, 0::2]
        rgb[0::2, :, 0] = bayer[1::2, 0::2]
    
    print(f"   Debayering terminé")
    print(f"   RGB Min={rgb.min()}, Max={rgb.max()}, Moy={rgb.mean():.1f}")
    
    return rgb

def convertir_raw_vers_tiff(fichier_raw, fichier_sortie=None, boost_exposure=1.0):
    """Convertit .raw en TIFF 16-bit RGB"""
    
    fichier_info = fichier_raw + ".info"
    
    print(f"\n{'='*70}")
    print(f"CONVERSION: {os.path.basename(fichier_raw)}")
    print(f"{'='*70}")
    
    # Lire métadonnées
    info = lire_info(fichier_info)
    if not info:
        print(f"Fichier .info introuvable: {fichier_info}")
        return False
    
    try:
        largeur = int(info['width'])
        hauteur = int(info['height'])
        format_pixel = info.get('format', 'UNKNOWN')
        stride = int(info.get('stride', largeur))
    except (KeyError, ValueError) as e:
        print(f"Métadonnées invalides: {e}")
        return False
    
    print(f"Configuration:")
    print(f"   • Résolution: {largeur}x{hauteur}")
    print(f"   • Format: {format_pixel}")
    print(f"   • Stride: {stride} bytes")
    # Lire fichier RAW
    try:
        with open(fichier_raw, 'rb') as f:
            data = f.read()
    except FileNotFoundError:
        print(f"Fichier introuvable")
        return False
    
    # Déterminer pattern Bayer (Correction du point 1)
    if len(format_pixel) >= 5 and format_pixel[0] == 'S':
        # Extrait les 4 lettres du motif (ex: SBGGR10_CSI2P -> BGGR)
        pattern = format_pixel[1:5].upper()
    else:
        pattern = 'RGGB' # Fallback le plus commun pour les capteurs Pi
    
    # Forcer un pattern spécifique pour les tests
    #pattern = 'GRBG' 

    #print(f"   • Pattern Bayer forcé: {pattern}")
    
    taille = len(data)
    print(f"   • Taille: {taille / (1024*1024):.2f} MB")
    
    # Déterminer le pattern Bayer à partir du format_pixel (e.g., SBGGR10_CSI2P -> BGGR)
    # Le format commence par 'S' (Sensor), suivi du pattern.
    if len(format_pixel) >= 5 and format_pixel[0] == 'S':
        pattern = format_pixel[1:5]
    else:
        pattern = 'BGGR' # Fallback
    
    pattern = pattern.upper()
    print(f"   • Pattern Bayer détecté: {pattern}")
    
    print(f"   • Pattern Bayer: {pattern}")
    
    # Traiter selon format
    pixels_attendus = largeur * hauteur
    bayer_array = None
    
    if 'CSI2P' in format_pixel or '10_CSI2P' in format_pixel:
        bayer_array = unpack_10bit_csi2p(data, largeur, hauteur, stride)
    elif taille == pixels_attendus * 2:
        print(f"   📦 Format: 16-bit RAW")
        bayer_array = np.frombuffer(data, dtype=np.uint16).reshape((hauteur, largeur))
    elif taille == pixels_attendus:
        print(f"   📦 Format: 8-bit RAW")
        bayer_array = np.frombuffer(data, dtype=np.uint8).reshape((hauteur, largeur))
        bayer_array = (bayer_array.astype(np.uint16) << 8)
    else:
        print(f"❌ Format non reconnu (taille: {taille}, attendu: {pixels_attendus})")
        return False
    
    if bayer_array is None:
        return False
    
    # Tester décalages (optionnel)
    #h_offset_test = 2
    #v_offset_test = 0
    
    #if h_offset_test != 0 or v_offset_test != 0:
    #    bayer_array = test_offset(bayer_array, h_offset=h_offset_test, v_offset=v_offset_test)
    
    # ------------------------------------------------------------
    
    # Appliquer boost d'exposition si nécessaire
    if boost_exposure != 1.0:
        print(f"   💡 Boost exposition: x{boost_exposure}")
        bayer_array = np.clip(bayer_array.astype(np.uint32) * boost_exposure, 
                               0, 65535).astype(np.uint16)
        print(f"   📊 Après boost: Min={bayer_array.min()}, Max={bayer_array.max()}")
    
    
    # Debayering
    rgb = debayer_simple_rapide(bayer_array, pattern)

    # Debayering
    rgb = debayer_simple_rapide(bayer_array, pattern)
    
    # Générer nom fichier
    if fichier_sortie is None:
        base = os.path.splitext(fichier_raw)[0]
        fichier_sortie = base + ".tif"
    
    # Sauvegarder en TIFF 16-bit
    print(f"   💾 Sauvegarde TIFF 16-bit...")
    
    # PIL supporte TIFF 16-bit en mode 'I;16' (grayscale) ou RGB via numpy
    # Convertir en format PIL accepte
    img = Image.fromarray(rgb, mode='RGB')
    
    # Sauvegarder avec compression LZW (sans perte)
    img.save(fichier_sortie, format='TIFF', compression='lzw')
    
    taille_sortie = os.path.getsize(fichier_sortie)
    print(f"\n{'='*70}")
    print(f"SUCCÈS!")
    print(f"   Fichier: {os.path.basename(fichier_sortie)}")
    print(f"   Taille: {taille_sortie / (1024*1024):.2f} MB")
    print(f"   Format: TIFF RGB 16-bit (compatible universel)")
    print(f"{'='*70}\n")
    
    return True

def convertir_batch(dossier=".", boost_exposure=1.0):
    """Convertit tous les .raw d'un dossier"""
    
    fichiers = sorted(glob.glob(os.path.join(dossier, "*.raw")))
    
    if not fichiers:
        print(f"Aucun fichier .raw trouvé dans {dossier}")
        return
    
    print(f"\n{'='*70}")
    print(f"CONVERSION EN BATCH")
    print(f"{'='*70}")
    print(f"Dossier: {os.path.abspath(dossier)}")
    print(f"Fichiers trouvés: {len(fichiers)}")
    print(f"Boost exposition: x{boost_exposure}")
    print(f"{'='*70}\n")
    
    succes = 0
    echecs = 0
    
    for i, fichier in enumerate(fichiers, 1):
        print(f"[{i}/{len(fichiers)}] ", end='')
        if convertir_raw_vers_tiff(fichier, boost_exposure=boost_exposure):
            succes += 1
        else:
            echecs += 1
    
    print(f"\n{'='*70}")
    print(f"RÉSUMÉ")
    print(f"{'='*70}")
    print(f"Succès: {succes}")
    print(f"Échecs: {echecs}")
    print(f"{'='*70}\n")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("="*70)
        print("CONVERTISSEUR RAW → TIFF 16-bit")
        print("   Pour Metashape, Photoshop, et tous logiciels photo")
        print("="*70)
        print("\n Usage:")
        print("  python3 convert_to_tiff.py fichier.raw [boost]")
        print("  python3 convert_to_tiff.py --batch [dossier] [boost]")
        print("\n Exemples:")
        print("  python3 convert_to_tiff.py capture_0001.raw")
        print("  python3 convert_to_tiff.py capture_0001.raw 10")
        print("  python3 convert_to_tiff.py --batch images/")
        print("  python3 convert_to_tiff.py --batch images/ 15")
        print("\n Notes:")
        print("  • Le fichier .raw.info doit exister")
        print("  • boost: multiplie la luminosité (défaut=1.0)")
        print("  • TIFF 16-bit = qualité maximale, compatible partout")
        print("="*70)
        sys.exit(1)
    
    if sys.argv[1] == "--batch":
        dossier = sys.argv[2] if len(sys.argv) > 2 and sys.argv[2] != sys.argv[-1] else "."
        boost = float(sys.argv[-1]) if len(sys.argv) > 2 and sys.argv[-1].replace('.','').isdigit() else 1.0
        convertir_batch(dossier, boost)
    else:
        fichier = sys.argv[1]
        boost = float(sys.argv[2]) if len(sys.argv) > 2 else 1.0
        convertir_raw_vers_tiff(fichier, boost_exposure=boost)