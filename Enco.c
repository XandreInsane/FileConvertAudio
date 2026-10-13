#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <string.h>

#define TAUX_ECHANTILLONNAGE 44100
#define PI 3.14159265358979323846

// Forcer l'alignement à 1 octet pour éviter le padding compilateur
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

const char* NOMS_ONDES[] = {"Inconnu", "Sinus", "Carre", "Dent de scie", "Triangle"};

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

void init_entete_wav(EnTeteWav *entete, uint32_t taille_donnees) {
    memcpy(entete->id_bloc, "RIFF", 4);
    entete->taille_bloc = 36 + taille_donnees;
    memcpy(entete->format, "WAVE", 4);
    
    memcpy(entete->id_sous_bloc1, "fmt ", 4);
    entete->taille_sous_bloc1 = 16;
    entete->format_audio = 1; 
    entete->nb_canaux = 1; 
    entete->taux_echantillonnage = TAUX_ECHANTILLONNAGE;
    entete->bits_par_echantillon = 16; 
    entete->debit_octets = TAUX_ECHANTILLONNAGE * 1 * (16 / 8);
    entete->alignement_bloc = 1 * (16 / 8);
    
    memcpy(entete->id_sous_bloc2, "data", 4);
    entete->taille_sous_bloc2 = taille_donnees;
}

int main() {
    char chemin_entree[256], nom_sortie[260], mdp[256] = "";
    int duree_bloc_ms = 0, num_ref = -1, type_onde = 0, mode_strict = -1;
    int utiliser_mdp = 0;
    double frequences[8];

    printf("=========================================================\n");
    printf("                       ENCODEUR AUDIO                     \n");
    printf("=========================================================\n\n");
    
    printf("Entrez le chemin du fichier a cacher : ");
    if (scanf("%255s", chemin_entree) != 1) return 1;
    
    printf("Entrez le nom du fichier audio de sortie (sans extension) : ");
    if (scanf("%255s", nom_sortie) != 1) return 1;
    strcat(nom_sortie, ".wav");

    printf("Voulez-vous proteger l'audio avec un mot de passe ? (1. Oui / 0. Non) : ");
    if (scanf("%d", &utiliser_mdp) != 1) return 1;

    if (utiliser_mdp == 1) {
        printf("Entrez le mot de passe : ");
        if (scanf("%255s", mdp) != 1) return 1;
        
        while (mode_strict != 0 && mode_strict != 1) {
            printf("Voulez-vous inclure la forme d'onde et la reference dans la cle (Mode Strict) ? (1. Oui / 0. Non) : ");
            if (scanf("%d", &mode_strict) != 1) while (getchar() != '\n');
        }
    }
    
    while (duree_bloc_ms < 1 || duree_bloc_ms > 5000) {
        printf("Entrez la duree d'un bloc en ms [Min: 1, Max: 5000] : ");
        if (scanf("%d", &duree_bloc_ms) != 1) while (getchar() != '\n');
    }
    
    while (num_ref < 0 || num_ref > 12000) {
        printf("Entrez le nombre de reference [Min: 0, Max: 12000] : ");
        if (scanf("%d", &num_ref) != 1) while (getchar() != '\n');
    }
    
    while (type_onde < 1 || type_onde > 4) {
        printf("\nForme de l'onde [Min: 1, Max: 4] :\n1. Sinus\n2. Carre\n3. Dent de scie\n4. Triangle\nOption : ");
        if (scanf("%d", &type_onde) != 1) while (getchar() != '\n');
    }

    for (int i = 0; i < 8; i++) {
        frequences[i] = 1000.0 * (i + 1) + num_ref;
    }

    if (utiliser_mdp == 1) {
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

    FILE *f_entree = fopen(chemin_entree, "rb");
    if (!f_entree) { printf("Erreur fichier d'entree.\n"); return 1; }

    fseek(f_entree, 0, SEEK_END);
    long taille_fichier = ftell(f_entree);
    fseek(f_entree, 0, SEEK_SET);

    uint32_t echantillons_par_bloc = (TAUX_ECHANTILLONNAGE * duree_bloc_ms) / 1000;
    uint32_t total_echantillons = taille_fichier * echantillons_par_bloc;
    uint32_t taille_donnees = total_echantillons * sizeof(int16_t);

    FILE *f_sortie = fopen(nom_sortie, "wb");
    if (!f_sortie) { fclose(f_entree); return 1; }

    EnTeteWav entete;
    init_entete_wav(&entete, taille_donnees);
    fwrite(&entete, sizeof(EnTeteWav), 1, f_sortie);

    unsigned char octet;
    int16_t *tampon = malloc(echantillons_par_bloc * sizeof(int16_t));
    uint32_t compteur_global = 0;
    
    printf("\nEncodage en cours...\n");

    while (fread(&octet, 1, 1, f_entree) == 1) {
        for (uint32_t t = 0; t < echantillons_par_bloc; t++) {
            double signal_accumule = 0.0;
            int nb_bits_actifs = 0;

            for (int bit = 0; bit < 8; bit++) {
                if ((octet >> bit) & 1) {
                    double temps = (double)(compteur_global + t) / TAUX_ECHANTILLONNAGE;
                    signal_accumule += generer_onde(frequences[bit], temps, type_onde, 0);
                    nb_bits_actifs++;
                }
            }
            
            if (nb_bits_actifs > 0) signal_accumule /= nb_bits_actifs;
            tampon[t] = (int16_t)(signal_accumule * 25000.0);
        }
        
        fwrite(tampon, sizeof(int16_t), echantillons_par_bloc, f_sortie);
        compteur_global += echantillons_par_bloc;
    }

    free(tampon); 
    fclose(f_entree); 
    fclose(f_sortie);

    FILE *journal = fopen("log.txt", "w");
    if (journal) {
        fprintf(journal, "=========================================\n");
        fprintf(journal, "       RECAPITULATIF D'ENCODAGE \n");
        fprintf(journal, "=========================================\n");
        fprintf(journal, "Fichier source          : %s\n", chemin_entree);
        fprintf(journal, "Taille du fichier       : %ld octets\n", taille_fichier);
        fprintf(journal, "Audio genere            : %s\n", nom_sortie);
        fprintf(journal, "Duree d'un bloc         : %d ms\n", duree_bloc_ms);
        fprintf(journal, "Nombre de reference     : %d\n", num_ref);
        fprintf(journal, "Forme de l'onde         : %s\n", NOMS_ONDES[type_onde]);
        fprintf(journal, "Mot de passe utilise    : %s\n", utiliser_mdp ? "OUI" : "NON");
        if(utiliser_mdp) {
            fprintf(journal, "Mode Strict (Cle)       : %s\n", mode_strict ? "OUI" : "NON");
        }
        fprintf(journal, "=========================================\n");
        fclose(journal);
    }

    printf("Succes ! Audio genere : %s\n", nom_sortie);
    return 0;
}