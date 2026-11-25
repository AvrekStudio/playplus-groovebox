# Analyse (en cours) : comment le firmware du Play+ accède aux pads, LED, potards et boutons

*25/11/2026. Premières observations sur le Play+ virtuel (trace des accès aux registres matériels du
i.MX RT1062 pendant la fonction de lecture des entrées). Base nécessaire au « mode invité » et à tout
portage. **Hypothèses marquées comme telles, à confirmer.***

## Ce que l'émulateur nous donnait déjà (repris de FreePlay, `cfw/vdev/hle.py`)

- **Lecture des entrées** : une seule fonction, appelée à chaque tour de la boucle principale,
  `0x600aab28`. Elle lit le matériel puis appelle un **« listener »** (objet en `0x20018c34`, vtable
  `0x6007b27c`) avec des événements de haut niveau : bouton, pad (index = rangée × 20 + colonne),
  potard (delta), toucher de potard (touché / relâché / maintenu), encodeur. Le Play+ virtuel injecte
  ses entrées à ce niveau-là, sans simuler le matériel.
- **LED des pads** : une trame de **0x1E30 octets (7728)** envoyée par **LPSPI4 + eDMA**
  (`0x600afc8c` démarre le transfert, `0x600afd74` est la machine d'états, fin signalée en
  `0x20005e4c` par l'ISR `0x600afe48`).
  **Vérifié le 25/11/2026** (voir `cfw/src/guest_led.c`) : 161 LED × 48 o = 7728 (la 161e est inconnue).
  Par LED : vert, rouge, bleu, 16 o chacun ; l'octet 1 + k porte le bit 7 - k (0x07 = 1, 0x3f = 0).
  Pilote en 0x20004d84 (+0 état 0/1/2, +4 trame en SDRAM, +8 « modifiée », +0x14 file d'attente) ;
  encodeur 0x600af998(pilote, i, r, g, b), **i = rangée × 20 + colonne**. Polyend écrit par le slot 1
  de la vtable du gestionnaire 0x60076eb8 (0x600ac580) et par les clignotements (BL en 0x600aca2a).

## Ce que montre la trace (50 ms de fonctionnement, accès matériels pendant `0x600aab28`)

| Registre | Périphérique i.MX RT1062 | Accès | Interprétation |
|---|---|---|---|
| `0x400C4000` (HC0), `0x400C4020` (HS), `0x400C4024` (R0) | **ADC1** | ~13 400 conversions/s ; canaux **13** puis **5**, 16 lectures d'affilée chacun | **Potards** lus en analogique. Hypothèse : multiplexeur analogique et moyenne sur 16 lectures |
| `0x401BC084` / `0x401BC088` (DR_SET / DR_CLEAR) | **GPIO2** | bits **16, 17, 24, 25** basculés entre deux séries de conversions | Sélection du multiplexeur des potards (hypothèse) |
| `0x401BC000` (DR) | **GPIO2** | ~18 000 lectures/s, en alternance avec GPIO3 | Balayage de la **grille de pads / boutons** (hypothèse : matrice ou registres à décalage) |
| `0x401C0000` (DR) | **GPIO3** | idem | Idem ; GPIO3.2 est aussi la détection de carte SD |
| `0x401B8000` (DR) | **GPIO1** | rare | À identifier (boutons ?) |
| `0x403C800C`, `0x403C8014` | **ENC1** (décodeur en quadrature) | 1 lecture par tour | **Encodeur de l'écran**, compté par le matériel |

Fonctions de l'officiel qui font ces accès : ADC `0x600b96de` (démarrage), `0x600af510` /
`0x600af550` (lecture) ; GPIO2 `0x600b933c` / `0x600b9342` (sélection), `0x600b11e8` (lecture) ;
GPIO3 `0x600c09f6` ; ENC1 `0x600b95f8`.

## Prochaines étapes

- [ ] Décoder le balayage des pads : ordre des lectures, quels bits de GPIO2 / GPIO3 portent quelles
      rangées, comment les colonnes sont sélectionnées (écritures non vues : registres à décalage en
      GPIO rapide `0x42xxxxxx` ? autre fonction ?).
- [ ] Potards : quelles adresses du multiplexeur donnent quel potard ; potards sans fin (deux curseurs)
      ou absolus ; comment le **toucher** est détecté (autre canal ADC ? capacitif ?).
- [x] LED : format exact, ordre des pads, trame à nous vérifiée dans l'émulateur (mode invité M3).
- [ ] Boutons de fonction et boutons sous l'écran : par où passent-ils ?
- [ ] Pour le mode invité : au niveau du **listener**, les événements sont déjà propres. On peut les
      intercepter là sans toucher au matériel. Le matériel brut ne servirait qu'à un firmware autonome
      (option A).
