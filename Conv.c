#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#define BLOCK_SIZE 1024
#define PI 3.14159265358979323846

#pragma pack(push, 1)
typedef struct {
    char chunkID[4];         
    int chunkSize;           
    char format[4];          
    
    char subChunk1ID[4];     
    int subChunk1Size;       
    short audioFormat;       
    short numChannels;       
    int sampleRate;          
    int byteRate;            
    short blockAlign;        
    short bitsPerSample;     
    
    char subChunk2ID[4];     
    int subChunk2Size;       
} StandardWavHeader;
#pragma pack(pop)

void afficher_barre_progression(long long octets_traites, long long octets_totaux) {
    if (octets_totaux <= 0) return;
    int largeur_barre = 40; 
    double ratio = (double)octets_traites / octets_totaux;
    if (ratio > 1.0) ratio = 1.0; 
    int position_curseur = (int)(largeur_barre * ratio);

    printf("\rProgression : [");
    for (int i = 0; i < largeur_barre; i++) {
        if (i < position_curseur) printf("=");
        else if (i == position_curseur) printf(">");
        else printf(" ");
    }
    printf("] %.1f%%", ratio * 100.0);
    fflush(stdout); 
}

double detect_frequency(short* mono_samples, int n, int sampleRate) {
    int max_lags = n / 2; 
    double* r = (double*)calloc(max_lags, sizeof(double));
    if (!r) return 0.0;

    double energie = 0.0;
    for (int i = 0; i < n; i++) {
        energie += (double)mono_samples[i] * mono_samples[i];
    }
    
    if (energie < 500000.0) {
        free(r);
        return 0.0; 
    }

    for (int lag = 0; lag < max_lags; lag++) {
        for (int i = 0; i < n - lag; i++) {
            r[lag] += (double)mono_samples[i] * mono_samples[i + lag];
        }
    }

    int debut_recherche = 1;
    for (int i = 1; i < max_lags - 1; i++) {
        if (r[i] < r[i - 1] && r[i] < r[i + 1]) {
            debut_recherche = i;
            break;
        }
    }

    double valeur_max = -1.0;
    int meilleur_lag = -1;
    for (int lag = debut_recherche; lag < max_lags - 1; lag++) {
        if (r[lag] > valeur_max && r[lag] > r[lag - 1] && r[lag] > r[lag + 1]) {
            valeur_max = r[lag];
            meilleur_lag = lag;
        }
    }

    free(r);
    if (meilleur_lag > 0) {
        return (double)sampleRate / meilleur_lag;
    }
    return 0.0; 
}

int main() {
    char chemin_entree[256], nom_sortie_utilisateur[256], chemin_sortie_complet[280];
    int choix_onde;

    printf("=========================================================\n");
    printf("       CONVERTISSEUR ET ANALYSEUR AUDIO ACADÉMIQUE\n");
    printf("=========================================================\n\n");

    printf("1. Entrez le chemin complet du fichier .wav d'origine : ");
    if (scanf("%255s", chemin_entree) != 1) return 1;

    printf("2. Entrez le nom voulu pour le fichier de sortie (sans .wav) : ");
    if (scanf("%255s", nom_sortie_utilisateur) != 1) return 1;
    sprintf(chemin_sortie_complet, "%s.wav", nom_sortie_utilisateur);

    printf("\n3. Choisissez la forme d'onde de synthèse :\n");
    printf("   [1] Onde Sinusoïdale Pure (Son doux)\n");
    printf("   [2] Onde Carrée (Style console rétro)\n");
    printf("Votre choix (1 ou 2) : ");
    if (scanf("%d", &choix_onde) != 1 || (choix_onde != 1 && choix_onde != 2)) {
        printf("Erreur : Choix invalide.\n");
        return 1;
    }

    FILE* fichier_in = fopen(chemin_entree, "rb");
    if (!fichier_in) {
        printf("\nErreur : Impossible de trouver '%s'.\n", chemin_entree);
        return 1;
    }

    char id_riff[4], id_wave[4];
    int taille_riff;
    if (fread(id_riff, 1, 4, fichier_in) != 4 || strncmp(id_riff, "RIFF", 4) != 0 ||
        fread(&taille_riff, 4, 1, fichier_in) != 1 ||
        fread(id_wave, 1, 4, fichier_in) != 4 || strncmp(id_wave, "WAVE", 4) != 0) {
        printf("\nErreur : Le fichier n'est pas un WAV valide.\n");
        fclose(fichier_in);
        return 1;
    }

    short format_audio = 0, nombre_canaux = 0, bits_par_echantillon = 0;
    int frequence_echantillonnage = 0;
    long long taille_donnees_audio = 0;
    int trouve_fmt = 0, trouve_data = 0;
    char id_chunk_actuel[4];
    int taille_chunk_actuel;

    while (fread(id_chunk_actuel, 1, 4, fichier_in) == 4) {
        if (fread(&taille_chunk_actuel, 4, 1, fichier_in) != 1) break;

        if (strncmp(id_chunk_actuel, "fmt ", 4) == 0) {
            fread(&format_audio, 2, 1, fichier_in);
            fread(&nombre_canaux, 2, 1, fichier_in);
            fread(&frequence_echantillonnage, 4, 1, fichier_in);
            fseek(fichier_in, 6, SEEK_CUR); 
            fread(&bits_par_echantillon, 2, 1, fichier_in);
            if (taille_chunk_actuel > 16) {
                fseek(fichier_in, taille_chunk_actuel - 16, SEEK_CUR);
            }
            trouve_fmt = 1;
        } 
        else if (strncmp(id_chunk_actuel, "data", 4) == 0) {
            taille_donnees_audio = taille_chunk_actuel;
            trouve_data = 1;
            break; 
        } 
        else {
            if (taille_chunk_actuel < 0) break; 
            fseek(fichier_in, taille_chunk_actuel, SEEK_CUR);
        }
    }

    if (!trouve_fmt || !trouve_data || bits_par_echantillon != 16 || format_audio != 1) {
        printf("\nErreur : Format non supporté (Requis: 16-bit PCM standard).\n");
        fclose(fichier_in);
        return 1;
    }

    StandardWavHeader entete_sortie;
    memcpy(entete_sortie.chunkID, "RIFF", 4);
    entete_sortie.chunkSize = 36; 
    memcpy(entete_sortie.format, "WAVE", 4);
    memcpy(entete_sortie.subChunk1ID, "fmt ", 4);
    entete_sortie.subChunk1Size = 16;
    entete_sortie.audioFormat = 1;
    entete_sortie.numChannels = nombre_canaux;
    entete_sortie.sampleRate = frequence_echantillonnage;
    entete_sortie.bitsPerSample = 16;
    entete_sortie.blockAlign = nombre_canaux * 2;
    entete_sortie.byteRate = frequence_echantillonnage * entete_sortie.blockAlign;
    memcpy(entete_sortie.subChunk2ID, "data", 4);
    entete_sortie.subChunk2Size = 0; 

    FILE* fichier_out = fopen(chemin_sortie_complet, "wb");
    if (!fichier_out) {
        printf("\nErreur : Création du fichier de sortie impossible.\n");
        fclose(fichier_in);
        return 1;
    }

    fwrite(&entete_sortie, sizeof(StandardWavHeader), 1, fichier_out);

    int echantillons_par_bloc = BLOCK_SIZE * nombre_canaux;
    short* tampon_entree   = (short*)malloc(echantillons_par_bloc * sizeof(short));
    short* tampon_sortie   = (short*)malloc(echantillons_par_bloc * sizeof(short));
    short* tampon_analyse  = (short*)malloc(BLOCK_SIZE * sizeof(short));

    double somme_frequences = 0.0;
    long long nombre_blocs_analyses = 0;
    double phase_oscillateur = 0.0; 
    long long total_octets_ecrits = 0, total_octets_lus = 0;

    printf("\nTraitement de l'audio en cours...\n");

    while (total_octets_lus < taille_donnees_audio) {
        long long octets_a_lire = echantillons_par_bloc * sizeof(short);
        if (total_octets_lus + octets_a_lire > taille_donnees_audio) {
            octets_a_lire = taille_donnees_audio - total_octets_lus;
        }
        if (octets_a_lire <= 0) break;

        size_t echantillons_a_les = octets_a_lire / sizeof(short);
        size_t echantillons_lus = fread(tampon_entree, sizeof(short), echantillons_a_les, fichier_in);
        if (echantillons_lus <= 0) break;

        total_octets_lus += echantillons_lus * sizeof(short);

        if (echantillons_lus < echantillons_par_bloc) {
            memset(tampon_entree + echantillons_lus, 0, (echantillons_par_bloc - echantillons_lus) * sizeof(short));
        }

        for (int i = 0; i < BLOCK_SIZE; i++) {
            tampon_analyse[i] = tampon_entree[i * nombre_canaux];
        }

        double frequence_detectee = detect_frequency(tampon_analyse, BLOCK_SIZE, frequence_echantillonnage);
        if (frequence_detectee > 0.0) {
            somme_frequences += frequence_detectee;
            nombre_blocs_analyses++;
        }

        for (int i = 0; i < BLOCK_SIZE; i++) {
            short valeur_synthetisee = 0;
            if (frequence_detectee > 0.0) {
                if (choix_onde == 1) {
                    valeur_synthetisee = (short)(16000.0 * sin(phase_oscillateur)); 
                } else {
                    valeur_synthetisee = (sin(phase_oscillateur) >= 0.0) ? 14000 : -14000;
                }
                phase_oscillateur += 2.0 * PI * frequence_detectee / frequence_echantillonnage;
                if (phase_oscillateur >= 2.0 * PI) phase_oscillateur -= 2.0 * PI;
            } else {
                valeur_synthetisee = 0; 
            }

            for (int c = 0; c < nombre_canaux; c++) {
                tampon_sortie[i * nombre_canaux + c] = valeur_synthetisee;
            }
        }

        fwrite(tampon_sortie, sizeof(short), echantillons_par_bloc, fichier_out);
        total_octets_ecrits += echantillons_par_bloc * sizeof(short);
        afficher_barre_progression(total_octets_lus, taille_donnees_audio);
    }

    printf("\n");

    fseek(fichier_out, 4, SEEK_SET);
    int taille_finale_riff = 36 + total_octets_ecrits;
    fwrite(&taille_finale_riff, 4, 1, fichier_out);

    fseek(fichier_out, 40, SEEK_SET);
    int taille_finale_data = total_octets_ecrits;
    fwrite(&taille_finale_data, 4, 1, fichier_out);

    free(tampon_entree); free(tampon_sortie); free(tampon_analyse);
    fclose(fichier_in); fclose(fichier_out);

    FILE* fichier_info = fopen("info.txt", "w");
    if (fichier_info) {
        double moyenne_frequence = (nombre_blocs_analyses > 0) ? (somme_frequences / nombre_blocs_analyses) : 0.0;
        double duree_bloc_ms = ((double)BLOCK_SIZE * 1000.0) / frequence_echantillonnage;
        
        fprintf(fichier_info, "==================================================\n");
        fprintf(fichier_info, "       RAPPORT DE SYNTHÈSE ET ANALYSE AUDIO\n");
        fprintf(fichier_info, "==================================================\n\n");
        fprintf(fichier_info, "Forme d'onde generee        : %s\n", (choix_onde == 1) ? "Sinusoidale" : "Carree");
        fprintf(fichier_info, "Fichier audio cree          : %s\n", chemin_sortie_complet);
        fprintf(fichier_info, "Fréquence d'echantillonnage : %d Hz\n", frequence_echantillonnage);
        fprintf(fichier_info, "Nombre de canaux            : %d\n", nombre_canaux);
        fprintf(fichier_info, "Duree temporelle d'un bloc  : %.2f ms\n", duree_bloc_ms);
        fprintf(fichier_info, "Taille d'un bloc d'analyse  : %d echantillons\n", BLOCK_SIZE);
        fprintf(fichier_info, "Fréquence moyenne du signal : %.2f Hz\n\n", moyenne_frequence);
        fprintf(fichier_info, "==================================================\n");
        fclose(fichier_info);
    }

    printf("\nTraitement termine ! Fichier : %s\n", chemin_sortie_complet);
    return 0;
}