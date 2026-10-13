#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include <SDL2/SDL.h>

#define SAMPLING_RATE 44100
#define PI 3.14159265358979323846

const double PIANO_GRID[6][8] = {
    {65.41,  73.42,  82.41,  87.31,  98.00,  110.00, 123.47, 130.81},
    {130.81, 146.83, 164.81, 174.61, 196.00, 220.00, 246.94, 261.63},
    {261.63, 293.66, 329.63, 349.23, 392.00, 440.00, 493.88, 523.25},
    {523.25, 587.33, 659.25, 698.46, 783.99, 880.00, 987.77, 1046.50},
    {1046.50,1174.66,1318.51,1396.91,1567.98,1760.00,1975.53,2093.00},
    {2093.00,2349.32,2637.02,2793.83,3135.96,3520.00,3951.07,4186.01}
};

void ecrire_en_tete_bmp(FILE *f, int largeur, int hauteur, uint32_t taille_pixels) {
    uint32_t taille_fichier = 54 + taille_pixels;
    uint32_t offset_donnees = 54;
    uint32_t taille_entete_dib = 40;
    uint16_t plans = 1, bpp = 24;
    uint32_t zero = 0, compression = 0, resolution = 2835;

    fwrite("BM", 1, 2, f);
    fwrite(&taille_fichier, 4, 1, f);
    fwrite(&zero, 4, 1, f);
    fwrite(&offset_donnees, 4, 1, f);
    fwrite(&taille_entete_dib, 4, 1, f);
    fwrite(&largeur, 4, 1, f);
    fwrite(&hauteur, 4, 1, f);
    fwrite(&plans, 2, 1, f);
    fwrite(&bpp, 2, 1, f);
    fwrite(&compression, 4, 1, f);
    fwrite(&taille_pixels, 4, 1, f);
    fwrite(&resolution, 4, 1, f);
    fwrite(&resolution, 4, 1, f);
    fwrite(&zero, 4, 1, f);
    fwrite(&zero, 4, 1, f);
}

// Fonction ultra rapide qui génère les pixels directement depuis le cache mémoire
void generer_pixels_depuis_cache(double *magnitude_cache, uint32_t total_blocks, int num_octaves, int img_largeur, uint8_t *pixel_data, int img_hauteur, int bytes_per_row_avec_padding) {
    // Initialiser le buffer à 0 (fond noir)
    memset(pixel_data, 0, bytes_per_row_avec_padding * img_hauteur);

    int current_row = 0, current_col = 0;

    for (uint32_t b = 0; b < total_blocks; b++) {
        if (current_row >= img_hauteur) break;

        double r_acc = 0.0, g_acc = 0.0, b_acc = 0.0;
        int notes_count = 0;

        for (int oct = 0; oct < num_octaves; oct++) {
            double max_mag = 0.0, total_energy = 0.0;
            uint32_t block_oct_offset = (b * num_octaves * 8) + (oct * 8);

            for (int bit = 0; bit < 8; bit++) {
                double mag = magnitude_cache[block_oct_offset + bit];
                total_energy += mag;
                if (mag > max_mag) max_mag = mag;
            }
            double average_energy = total_energy / 8.0;
            double threshold = (max_mag * 0.20) + (average_energy * 0.3);

            for (int bit = 0; bit < 8; bit++) {
                double mag = magnitude_cache[block_oct_offset + bit];
                if (mag > threshold && mag > 0.4) {
                    double octave_factor = 0.4 + ((double)oct / (double)num_octaves) * 0.6;
                    if (bit <= 1)      r_acc += mag * octave_factor;
                    else if (bit <= 4) g_acc += mag * octave_factor;
                    else               b_acc += mag * octave_factor;
                    notes_count++;
                }
            }
        }

        uint8_t final_r = 0, final_g = 0, final_b = 0;
        if (notes_count > 0) {
            double max_acc = r_acc;
            if (g_acc > max_acc) max_acc = g_acc;
            if (b_acc > max_acc) max_acc = b_acc;
            if (max_acc > 0.0) {
                final_r = (uint8_t)((r_acc / max_acc) * 255.0 * (1.0 - 1.0 / (1.0 + r_acc)));
                final_g = (uint8_t)((g_acc / max_acc) * 255.0 * (1.0 - 1.0 / (1.0 + g_acc)));
                final_b = (uint8_t)((b_acc / max_acc) * 255.0 * (1.0 - 1.0 / (1.0 + b_acc)));
            }
        }

        int index = (current_row * bytes_per_row_avec_padding) + (current_col * 3);
        pixel_data[index] = final_b;
        pixel_data[index + 1] = final_g;
        pixel_data[index + 2] = final_r;

        current_col++;
        if (current_col >= img_largeur) { current_col = 0; current_row++; }
    }
}

int main(int argc, char* argv[]) {
    char input_wav[256];
    int block_ms = 40, num_octaves = 6, img_largeur = 400;

    printf("--- DÉCODEUR LIVE ULTRA-FLUIDE (AVEC CACHE SPECTRAL) ---\n");
    printf("Nom du fichier audio .wav (ex: golden.wav) : ");
    scanf("%255s", input_wav);

    FILE *infile = fopen(input_wav, "rb");
    if (!infile) { printf("[ERREUR] Impossible d'ouvrir l'audio.\n"); return 1; }

    char id[4]; uint32_t size; int data_found = 0;
    fseek(infile, 12, SEEK_SET);
    while (fread(id, 1, 4, infile) == 4) {
        if (fread(&size, 4, 1, infile) != 1) break;
        if (strncmp(id, "data", 4) == 0) { data_found = 1; break; }
        else fseek(infile, size, SEEK_CUR);
    }
    if (!data_found) { printf("[ERREUR] Fichier WAV non supporté.\n"); fclose(infile); return 1; }

    uint32_t samples_per_block = (SAMPLING_RATE * block_ms) / 1000;
    uint32_t total_blocks = (size / sizeof(int16_t)) / samples_per_block;

    printf("\n[1/2] Analyse de l'audio et mise en cache mémoire...\n");
    
    // Allocation du cache spectral (total_blocks * num_octaves * 8 notes)
    double *magnitude_cache = malloc(total_blocks * num_octaves * 8 * sizeof(double));
    int16_t *audio_buf = malloc(samples_per_block * sizeof(int16_t));
    uint32_t sample_absolute_counter = 0;

    for (uint32_t b = 0; b < total_blocks; b++) {
        if (fread(audio_buf, sizeof(int16_t), samples_per_block, infile) != samples_per_block) break;
        
        // Barre de progression console
        if (b % (total_blocks / 10 + 1) == 0) printf("Progression : %d%%\n", (b * 100) / total_blocks);

        for (int oct = 0; oct < num_octaves; oct++) {
            uint32_t block_oct_offset = (b * num_octaves * 8) + (oct * 8);
            for (int bit = 0; bit < 8; bit++) {
                double target_freq = PIANO_GRID[oct][bit];
                double real_sum = 0.0, imag_sum = 0.0;
                for (uint32_t t = 0; t < samples_per_block; t++) {
                    double time = (double)(sample_absolute_counter + t) / SAMPLING_RATE;
                    double sample = (double)audio_buf[t] / 32767.0;
                    real_sum += sample * cos(2.0 * PI * target_freq * time);
                    imag_sum += sample * sin(2.0 * PI * target_freq * time);
                }
                magnitude_cache[block_oct_offset + bit] = sqrt(real_sum * real_sum + imag_sum * imag_sum);
            }
        }
        sample_absolute_counter += samples_per_block;
    }
    free(audio_buf);
    fclose(infile);
    printf("Analyse terminée avec succès !\n");

    // Initialisation de SDL2
    if (SDL_Init(SDL_INIT_VIDEO) < 0) { printf("Erreur SDL Init\n"); free(magnitude_cache); return 1; }

    SDL_Window *window = SDL_CreateWindow("Spectrogramme Piano Interactif (Fluide)", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_SHOWN);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    uint8_t *pixel_buffer = NULL;
    uint32_t current_allocated_size = 0;
    int besoin_maj = 1;
    int running = 1;
    int img_hauteur = 0;
    int bytes_per_row_avec_padding = 0;

    printf("\n[Commandes Interface Graphique]\n-> FLECHE DROITE : Augmenter la largeur (+20 px)\n-> FLECHE GAUCHE : Diminuer la largeur (-20 px)\n-> TOUCHE S : Sauvegarder l'image en BMP\n-> TOUCHE ECHAP : Quitter\n");

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) { running = 0; }
            else if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                    case SDLK_ESCAPE: running = 0; break;
                    case SDLK_RIGHT: 
                        img_largeur += 20; 
                        besoin_maj = 1; 
                        break;
                    case SDLK_LEFT: 
                        if (img_largeur > 40) { img_largeur -= 20; besoin_maj = 1; }
                        break;
                    case SDLK_s: {
                        char out_name[256];
                        sprintf(out_name, "spectrogramme_%d_largeur.bmp", img_largeur);
                        FILE *outfile = fopen(out_name, "wb");
                        if (outfile) {
                            ecrire_en_tete_bmp(outfile, img_largeur, img_hauteur, current_allocated_size);
                            fwrite(pixel_buffer, 1, current_allocated_size, outfile);
                            fclose(outfile);
                            printf("[SAUVEGARDE] Fichier '%s' enregistré !\n", out_name);
                        }
                        break;
                    }
                }
            }
        }

        if (besoin_maj) {
            img_hauteur = total_blocks / img_largeur;
            if (img_hauteur <= 0) img_hauteur = 1;

            int bytes_per_row_sans_padding = img_largeur * 3;
            int padding_size = (4 - (bytes_per_row_sans_padding % 4)) % 4;
            bytes_per_row_avec_padding = bytes_per_row_sans_padding + padding_size;
            uint32_t total_size_requise = bytes_per_row_avec_padding * img_hauteur;

            if (total_size_requise > current_allocated_size) {
                pixel_buffer = realloc(pixel_buffer, total_size_requise);
                current_allocated_size = total_size_requise;
            }

            // Génération instantanée depuis la mémoire RAM !
            generer_pixels_depuis_cache(magnitude_cache, total_blocks, num_octaves, img_largeur, pixel_buffer, img_hauteur, bytes_per_row_avec_padding);
            besoin_maj = 0;

            SDL_Surface *surface = SDL_CreateRGBSurfaceFrom(
                pixel_buffer, img_largeur, img_hauteur, 24, 
                bytes_per_row_avec_padding, 
                0x0000FF, 0x00FF00, 0xFF0000, 0
            );
            
            SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_FreeSurface(surface);

            SDL_RenderClear(renderer);
            // SDL_FLIP_VERTICAL remet l'image dans le bon sens (car le format BMP écrit les lignes à l'envers)
            SDL_RenderCopyEx(renderer, texture, NULL, NULL, 0, NULL, SDL_FLIP_VERTICAL);
            SDL_RenderPresent(renderer);
            SDL_DestroyTexture(texture);
            
            printf("Largeur actuelle : %d px | Hauteur : %d px\n", img_largeur, img_hauteur);
        }
        SDL_Delay(16); // ~60 FPS pour laisser respirer le processeur
    }

    if (pixel_buffer) free(pixel_buffer);
    free(magnitude_cache);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

//gcc decP.c -o decolive.exe -lmingw32 -lSDL2main -lSDL2 -lm