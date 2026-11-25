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
#define PGI_ABI 1u
#define PGI_BASE 0x20200000u
#define PGI_LIMIT 0x80000u         /* 512 Ko (OCRAM2 du i.MX RT1062) : image + .bss */
#define PGI_DIR "/Guests"          /* dossier des invités : Menu > Mode invite les liste */
#define PGI_PATH "/guest.pgi"      /* sinon, ce fichier seul est chargé directement */

/* types d'événements (mêmes valeurs que guest.h) ; états des boutons/pads : 0 appui, 1 relâché, 2 tenu */
#define PGI_EV_PAD 0          /* a = rangée x 20 + colonne (0..159), b = état */
#define PGI_EV_BUTTON 1       /* a = identifiant, b = état */
#define PGI_EV_KNOB 2         /* a = potard (0..14), b = pas signé (int32) */
#define PGI_EV_TOUCH 3        /* a = potard touché, b = état */
#define PGI_EV_ENCODER 4      /* a = 0, b = pas signé (int32) */

struct pgi_host {
    unsigned int abi;
    unsigned int (*millis)(void);
    void (*led)(unsigned int i, unsigned int r, unsigned int g, unsigned int b);  /* LED du pad i */
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
};

#endif
