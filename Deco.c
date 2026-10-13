#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <string.h>

#define TAUX_ECHANTILLONNAGE 44100
#define PI 3.14159265358979323846

#pragma pack(push, 1)
typedef struct {
    char     id_bloc[4]; 
    uint32_t taille_bloc; 
    char     format[4]; 
    char     id_sous_bloc1[4];
    uint32_t taille_sous_bloc1; 
    uint16_t format_audio; 
    uint16_t nb_canaux;
    uint32_t taux_echantillonnage; 
    uint32_t debit_octets; 
    uint16_t alignement_bloc;
    uint16_t bits_par_echantillon; 
    char     id_sous_bloc2[4]; 
    uint32_t taille_sous_bloc2;
} EnTeteWav;
#pragma pack(pop)

uint32_t hacher_mdp(const char *chaine) {
    uint32_t hachage = 5381;
    int c;
    while ((c = (unsigned char)*chaine++)) {
        hachage = ((hachage << 5) + hachage) + c;
    }
    return hachage;
}

uint32_t xorshift32(uint32_t *etat) {
    uint32_t x = *etat;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *etat = x;
    return x;
}

double generer_onde(double freq, double temps, int type, int quadrature) {
    double angle = 2.0 * PI * freq * temps;
    double decalage_phase = quadrature ? (PI / 2.0) : 0.0;
    double phase_t = freq * temps + (quadrature ? 0.25 : 0.0);
    double fraction = phase_t - floor(phase_t);

    switch (type) {
        case 1: return cos(angle + decalage_phase);
        case 2: return (fraction < 0.5) ? 1.0 : -1.0;
        case 3: return 2.0 * fraction - 1.0;
        case 4: return 4.0 * fabs(fraction - 0.5) - 1.0;
        default: return 0.0;
    }
}

int main() {
    char fichier_wav[256], fichier_sortie[256], mdp[256] = "";
    int mode_decodage = 1;
    int duree_bloc_ms = 0, mode_affichage = 2, num_ref = 0, type_onde = 1, mode_strict = 0;
    int utiliser_mdp = 0; 
    double frequences[8];
    uint32_t echantillons_par_bloc = 0;

    printf("=========================================================\n");
    printf("              DECODEUR AUDIO MULTI-MODE\n");
    printf("=========================================================\n\n");
    
    printf("Choisissez le mode de decodage :\n");
    printf("  [1] Mode classique (Fichier encodé avec Enco.c)\n");
    printf("  [2] Mode experimental / Glitch (Fichier converti avec Conv.c)\n");
    printf("Votre choix (1 ou 2) : ");
    if (scanf("%d", &mode_decodage) != 1 || (mode_decodage != 1 && mode_decodage != 2)) {
        printf("Erreur : Choix invalide.\n");
        return 1;
    }

    printf("\nFichier audio a decoder (avec extension .wav) : ");
    scanf("%255s", fichier_wav);

    if (mode_decodage == 1) {
        printf("L'audio est-il protege par un mot de passe ? (1. Oui / 0. Non) : ");
        scanf("%d", &utiliser_mdp);

        if (utiliser_mdp == 1) {
            printf("Entrez le mot de passe secret de dechiffrement : ");
            scanf("%255s", mdp);
            printf("Le mode strict a-t-il ete utilise a l'encodage ? (1. Oui / 0. Non) : ");
            scanf("%d", &mode_strict);
        }
        
        printf("Duree d'un bloc en ms : ");
        scanf("%d", &duree_bloc_ms);
        echantillons_par_bloc = (TAUX_ECHANTILLONNAGE * duree_bloc_ms) / 1000;
        
        printf("Nombre de reference utilise : ");
        scanf("%d", &num_ref);
        
        printf("Forme de l'onde originale (1.Sinus, 2.Carre, 3.Dent scie, 4.Triangle) : ");
        scanf("%d", &type_onde);
    } else {
        printf("\n[!] Mode experimental actif : bloc force a 1024 echantillons (~23.2ms).\n");
        echantillons_par_bloc = 1024; 
        printf("Entrez le nombre de reference utilise a l'encodage (ex: 0 ou 50) : ");
        scanf("%d", &num_ref);
    }
    
    printf("Affichage console (1.Binaire, 2.Hexa) : ");
    scanf("%d", &mode_affichage);
    
    printf("Nom du fichier a reconstruire (ex: sortie.txt ou image.bmp) : ");
    scanf("%255s", fichier_sortie);

    for (int i = 0; i < 8; i++) {
        frequences[i] = 1000.0 * (i + 1) + num_ref;
    }

    if (mode_decodage == 1 && utiliser_mdp == 1) {
        char empreinte_securite[512];
        if (mode_strict == 1) {
            snprintf(empreinte_securite, sizeof(empreinte_securite), "%s_%d_%d", mdp, num_ref, type_onde);
        } else {
            snprintf(empreinte_securite, sizeof(empreinte_securite), "%s", mdp);
        }
        
        uint32_t etat_crypto = hacher_mdp(empreinte_securite);
        for (int i = 7; i > 0; i--) {
            uint32_t val_aleatoire = xorshift32(&etat_crypto);
            int j = val_aleatoire % (i + 1);
            double temp = frequences[i];
            frequences[i] = frequences[j];
            frequences[j] = temp;
        }
    }

    FILE *f_entree = fopen(fichier_wav, "rb");
    if (!f_entree) { 
        printf("Erreur : Impossible d'ouvrir le fichier audio.\n"); 
        return 1; 
    }

    EnTeteWav entete;
    if (fread(&entete, sizeof(EnTeteWav), 1, f_entree) != 1) {
        printf("Erreur lors de la lecture de l'en-tete WAV.\n");
        fclose(f_entree);
        return 1;
    }

    uint16_t nb_canaux = entete.nb_canaux;
    if (nb_canaux < 1) nb_canaux = 1;

    uint32_t total_echantillons = entete.taille_sous_bloc2 / sizeof(int16_t);
    uint32_t total_blocs = total_echantillons / (echantillons_par_bloc * nb_canaux);

    FILE *f_sortie = fopen(fichier_sortie, "wb");
    if (!f_sortie) { 
        printf("Erreur lors de la creation du fichier de sortie.\n");
        fclose(f_entree); 
        return 1; 
    }

    int16_t *tampon = malloc(echantillons_par_bloc * nb_canaux * sizeof(int16_t));
    if (!tampon) {
        fclose(f_entree); fclose(f_sortie);
        return 1;
    }

    printf("\nDecodage en cours (%u blocs)...\n\n", total_blocs);

    for (uint32_t b = 0; b < total_blocs; b++) {
        if (fread(tampon, sizeof(int16_t), echantillons_par_bloc * nb_canaux, f_entree) != echantillons_par_bloc * nb_canaux) {
            break;
        }

        unsigned char octet_decode = 0;
        double amplitudes[8];
        double amplitude_max = 0.0;

        for (int bit = 0; bit < 8; bit++) {
            double freq_cible = frequences[bit];
            double somme_reelle = 0.0;
            double somme_imag = 0.0;

            for (uint32_t t = 0; t < echantillons_par_bloc; t++) {
                // CORRECTION CRITIQUE : Temps LOCAL au bloc pour annuler l'accumulation des dérives de phase.
                double temps = (double)t / TAUX_ECHANTILLONNAGE;
                double echantillon = (double)tampon[t * nb_canaux] / 32767.0;
                
                somme_reelle += echantillon * generer_onde(freq_cible, temps, 1, 0); 
                somme_imag   += echantillon * generer_onde(freq_cible, temps, 1, 1); 
            }

            amplitudes[bit] = sqrt(somme_reelle * somme_reelle + somme_imag * somme_imag);
            if (amplitudes[bit] > amplitude_max) {
                amplitude_max = amplitudes[bit];
            }
        }

        // CORRECTION : Tolérance accrue (0.35 au lieu de 0.6) en mode expérimental pour compenser la perte de Conv.c
        double facteur_seuil = (mode_decodage == 2) ? 0.35 : 0.6;
        double seuil_adaptatif = amplitude_max * facteur_seuil;
        if (seuil_adaptatif < 2.0) {
            seuil_adaptatif = 2.0;
        }

        for (int bit = 0; bit < 8; bit++) {
            if (amplitudes[bit] > seuil_adaptatif) {
                octet_decode |= (1 << bit);
            }
        }

        if (mode_affichage == 1) {
            for (int i = 7; i >= 0; i--) printf("%d", (octet_decode >> i) & 1);
            printf(" ");
        } else {
            printf("%02X ", octet_decode);
        }
        
        if ((b + 1) % 16 == 0) printf("\n");

        fwrite(&octet_decode, 1, 1, f_sortie);
    }

    printf("\n\nExtraction terminee. Fichier genere : '%s'\n", fichier_sortie);
    
    free(tampon);
    fclose(f_entree);
    fclose(f_sortie);
    
    return 0;
}
