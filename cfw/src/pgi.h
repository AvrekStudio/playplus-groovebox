/* PGI : « Play+ Guest Image », format d'un programme invité chargé depuis la carte SD (mode invité, M4).
 * En-tête partagé par l'hôte (notre firmware, guest_load.c) et les invités (guest/sdk). Autonome :
 * pas de dépendance à cfw.h, pour que les invités puissent l'inclure tel quel.
 *
 * Le fichier est l'image brute de l'invité, LIÉE À L'ADRESSE PGI_BASE (OCRAM, 0x20200000) : pas de
 * relocation, l'hôte le copie tel quel à cette adresse. Pourquoi l'OCRAM : vide après le démarrage du
 * firmware (vérifié sur le Play+ virtuel, aucune constante de l'officiel n'y pointe hors de la table de
 * démarrage, section de taille 0), alors que le tas de l'officiel est en DTCM (non exécutable sur M7).
 * L'invité commence par struct pgi_header (section .pgi_header, placée en tête par guest/sdk/pgi.ld).
 * Ses variables globales sont normales (il tourne en RAM) ; .bss est mise à zéro par l'hôte.
 *
 * Appels : init une fois au chargement ; audio sous INTERRUPTION (128 échantillons int16 gauche/droite,
 * 44,1 kHz : rapide, sans fichier ni allocation) ; event et tick dans la boucle principale. */
#ifndef PGI_H
#define PGI_H

#define PGI_MAGIC 0x31494750u      /* "PGI1" */
#define PGI_ABI 6u                 /* 1 : OCRAM seule ; 2 : + champ base (OCRAM ou SDRAM) + service écran ;
                                    * 3 : + fichiers (file_read, file_write) ;
                                    * 4 : + champ bands (textes des bandes latérales) ;
                                    * 5 (Avrek OS 0.6.2) : + lecture par morceaux (fopen / fread / fclose) et, pour
                                    *   les invités ABI 5 seulement, événements PGI_EV_TOUCH et PGI_EV_PADX ;
                                    * 6 (Avrek OS 0.6.4) : + backlight (luminosité de l'écran) */
#define PGI_BASE 0x20200000u       /* zone OCRAM (ABI 1 : toujours ici) */
#define PGI_LIMIT 0x80000u         /* 512 Ko (OCRAM2 du i.MX RT1062) : image + .bss */
/* Zone SDRAM (ABI 2, invités trop gros pour l'OCRAM : LOOM-8). C'est le haut de la mémoire des samples
 * de Polyend (bloc 0x804d900c..0x81ffee80, alloué au démarrage, rempli par le bas) : l'hôte refuse de
 * charger si les samples du projet y arrivent (curseur de fw_sample_pool). 2 Mo moins 8 Ko. */
#define PGI_SDRAM_BASE 0x81E00000u
#define PGI_SDRAM_LIMIT 0x1FE000u
/* Grande zone SDRAM (Avrek OS 0.6.0, invités de plus de 2 Mo : Skiff et sa flash simulée de 1 Mo…) : les 8 Mo du
 * haut de la même mémoire des samples, même règle (refus si les samples du projet y arrivent), même fin. Une région
 * MPU de 8 Mo (base alignée sur 8 Mo). Les invités de 2 Mo restent à PGI_SDRAM_BASE. */
#define PGI_SDRAM8_BASE 0x81800000u
#define PGI_SDRAM8_LIMIT 0x7FE000u
#define PGI_DIR "/Guests"          /* dossier des invités : Menu > Mode invite les liste */
#define PGI_PATH "/guest.pgi"      /* sinon, ce fichier seul est chargé directement */

/* types d'événements (mêmes valeurs que guest.h) ; états des boutons/pads : 0 appui, 1 relâché, 2 tenu */
#define PGI_EV_PAD 0          /* a = rangée x 20 + colonne (0..159), b = état */
#define PGI_EV_BUTTON 1       /* a = identifiant, b = état */
#define PGI_EV_KNOB 2         /* a = potard (0..14), b = pas signé (int32) */
#define PGI_EV_TOUCH 3        /* a = potard touché, b = état */
#define PGI_EV_ENCODER 4      /* a = 0, b = pas signé (int32) */
#define PGI_EV_PADX 5         /* (ABI 5) a = pad, b = 3e argument brut du pilote de Polyend (vélocité ? à l'étude),
                               * envoyé juste avant chaque PGI_EV_PAD */

struct pgi_host {
    unsigned int abi;
    unsigned int (*millis)(void);
    void (*led)(unsigned int i, unsigned int r, unsigned int g, unsigned int b);  /* LED du pad i */
    /* ABI 2 : l'image de l'invité, 240 x 240 RGB565 (rangée après rangée), rangées [y0, y1) envoyées à
     * l'écran ; swap = 1 si les pixels sont octets permutés (format du FM-1). L'hôte l'affiche au centre
     * de l'écran (bandes de 40 px de chaque côté) au lieu de sa mire. Boucle principale seulement. */
    void (*screen)(const unsigned short *px, unsigned int y0, unsigned int y1, unsigned int swap);
    /* ABI 3 : fichiers de l'invité, dans son dossier de la carte : /Guests/<nom de l'invité>/<name>
     * (name : lettres, chiffres, « . », « - », « _ » ; 24 caractères au plus). Boucle principale seulement
     * (jamais depuis audio). file_read : octets lus (au plus max), -1 si absent. file_write : remplace le
     * fichier, 0 si écrit en entier, -1 sinon. */
    int (*file_read)(const char *name, void *dst, unsigned int max);
    int (*file_write)(const char *name, const void *src, unsigned int len);
    /* ABI 5 : un fichier de l'invité gardé ouvert et lu par morceaux, dans l'ordre (4 à la fois au plus). Boucle
     * principale seulement. fopen : poignée 0..3, -1 si absent ou plus de place ; fread : octets lus (0 à la fin),
     * -1 si erreur ; fclose : libère la poignée (l'hôte ferme aussi tout au déchargement de l'invité). */
    int (*fopen)(const char *name);
    int (*fread)(int h, void *dst, unsigned int n);
    void (*fclose)(int h);
    /* ABI 6 : luminosité de l'écran, 0..128 (REG_PWM_DUTY de l'EVE ; 128 = pleine). Appliquée à l'image suivante
     * de l'hôte (og_draw). */
    void (*backlight)(unsigned int duty);
};

struct pgi_header {
    unsigned int magic;
    unsigned int abi;
    unsigned int image_end;   /* adresse de fin des octets du fichier (= PGI_BASE + taille du fichier) */
    unsigned int bss_end;     /* adresse de fin de .bss (<= PGI_BASE + PGI_LIMIT) */
    char name[16];            /* affiché à l'écran */
    void (*init)(const struct pgi_host *h);
    void (*audio)(short *l, short *r, unsigned int n);
    void (*event)(unsigned int kind, unsigned int a, unsigned int b);
    void (*tick)(void);
    unsigned int base;        /* ABI 2 : adresse de chargement (PGI_BASE ou PGI_SDRAM_BASE) */
    /* ABI 4 : textes des bandes latérales de 40 px (à côté de l'image 240 x 240), ou 0. 8 lignes à gauche
     * (bands[0..7]) puis 8 à droite (bands[8..15]), 5 caractères au plus, 0 ou "" = ligne vide. Lignes
     * paires en gris (étiquette), impaires en clair (valeur). L'invité peut les changer à tout moment :
     * l'hôte les relit à chaque image. Sous les 8 lignes, l'hôte affiche la charge CPU.
     * (Avrek OS 0.6.5) si bands[8] commence par l'octet 1, la bande de droite est en 4 cases au fond clair
     * (étiquette bands[8 + 2c], valeur bands[9 + 2c]) sans la charge ni le dernier bouton. */
    const char *const *bands;
};

#endif
