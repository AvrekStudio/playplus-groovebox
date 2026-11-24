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

## Indices dans le manuel (ajout du 24/11/2026)

- Polyend limite les synthés à **8 voix au total**, réparties entre les 3 emplacements (manuel p. 91).
- Le manuel déconseille de composer et de faire du sound design avec l'**audio USB** actif, à cause de
  problèmes de lecture possibles (p. 230).
- Les effets de sortie (limiteur, saturation, EQ / bass boost / space) et les envois reverb / delay
  sont toujours dans la chaîne : cohérent avec un coût fixe élevé en silence.

Le CPU est donc déjà bien occupé par l'usine. Nos moteurs devront être légers, ou prendre la place
de quelque chose (un emplacement de synthé, un effet non utilisé) plutôt que s'ajouter.

## À faire

- [ ] Nommer les fonctions du coût fixe (activer/désactiver reverb, delay, limiteur, et observer).
- [ ] Mesurer le coût par voix des moteurs d'usine avec un projet qui sonne vraiment.
- [ ] Refaire tout ça sur la vraie machine (*Menu → Diagnostic*).
