# Analyse : d'où vient la charge audio, et gagne-t-on à retirer les moteurs Polyend ?

*23/11/2026, Play+ virtuel, firmware 1.3.0 + notre socle. Mesures en mode `exact_count`
(1 instruction = 1 cycle) : ordres de grandeur seulement, à refaire sur la machine.*

## Mesures

| Situation | Charge moyenne | Pic |
|---|---|---|
| Lecture, 4 pistes audio × 16 pas | 24,0 % | 26,7 % |
| + 4 pistes synthé/MIDI × 16 pas | 24,1 % | 31,4 % |

Le projet par défaut du virtuel ne produit **aucun son** (sortie audio nulle : pas de sample chargé,
pistes synthé probablement en MIDI externe). Ces ~24 % sont donc un **coût fixe**, payé même en
silence : environ 414 000 instructions par bloc de 128 échantillons.

Profil de ce coût fixe (script d'exploration, non versionné) : plus de la moitié en ITCM (code DSP
rapide copié depuis la flash `0x6017625C`), réparti sur une vingtaine de fonctions. Certaines sont
appelées 3, 13–14, voire 128 fois par bloc : un mélange de traitements globaux et par piste. **Pas
encore nommées** : reverb, delay, limiteur, chaînes de piste (filtre, overdrive), mixage, audio USB
sont les candidats.

## Conclusion provisoire

- Retirer ou remplacer les **moteurs synthé** de Polyend ne libère *a priori* que ce qu'ils coûtent
  **quand ils jouent**. Sur une piste, on échange leur coût contre celui de notre moteur.
- Le gros levier est le **coût fixe** (~¼ du CPU sur le virtuel) : identifier ce qui tourne en
  silence, puis le court-circuiter quand c'est inutile (effet non utilisé, piste muette).
- La mémoire (32 Mo de SDRAM) et la flash ne sont pas le facteur limitant.

## À faire

- [ ] Nommer les fonctions du coût fixe (activer/désactiver reverb, delay, limiteur, et observer).
- [ ] Mesurer le coût par voix des moteurs d'usine avec un projet qui sonne vraiment.
- [ ] Refaire tout ça sur la vraie machine (*Menu → Diagnostic*).
