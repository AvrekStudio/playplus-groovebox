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

## Vérification de l'hypothèse « Voice Sleep » (25/11/2026) : CONFIRMÉE

Expérience sur le Play+ virtuel, **sans jouer une seule note** : on change seulement la polyphonie des
synthés dans *Menu → Synths* et on lit la charge moyenne avec notre mesure (`audio_load.c`, mode
`exact_count`). Projet par défaut : Synth 1 = VAP (3 voix), Synth 2 = ACD, Synth 3 = WTFM.

| Configuration (silence total) | Charge |
|---|---|
| Par défaut (VAP 3 voix + ACD + WTFM) | 23,7 % |
| Synth 1 (VAP) à Off | 12,0 % |
| Synth 1 (VAP) à 6 voix | 35,3 % |
| Synth 1 et 2 à Off | 9,0 % |
| **Les 3 synthés à Off** | **5,5 %** |

**Conclusion :** le Play+ 1.3.0 **calcule en permanence toutes les voix de synthé allouées**, même
muettes. Sur les 23,7 % de charge en silence, **18 points viennent des voix de synthé inactives**. Le
vrai coût fixe (effets, mixage, séquenceur…) n'est que d'environ **5,5 %**.

- VAP : environ **3,9 % par voix**, linéaire (3 voix = 11,7 points, 6 voix = 23,3 points).
- ACD : environ 3 points ; WTFM : environ 3,5 points.

(Pourcentages du simulateur, 1 instruction = 1 cycle : les proportions sont fiables, les valeurs
absolues seront à recaler sur la machine.)

**Ce que ça change :**
1. **Tout de suite, sans rien coder** : mettre à *Off* les emplacements de synthé qu'on n'utilise pas,
   et ne donner à chacun que les voix nécessaires. C'est une fonction d'usine.
2. **Dans notre firmware** : un **Voice Sleep** à la manière du Tracker CE (ne pas calculer une voix
   dont l'enveloppe est finie) récupérerait l'essentiel de ces 18 points, sans réduire la polyphonie.
   C'est le gain le plus important identifié à ce jour.
3. La marge réelle pour nos moteurs est bien plus grande qu'estimé : avec des voix qui dorment, la
   charge de fond tombe vers 5–6 %.

## Voice Sleep, première version : le moteur VAP (25/11/2026)

**Où :** profil de l'interruption audio (blocs exécutés par fonction) sur le projet par défaut : la
fonction ITCM **0x6c00** est appelée 3 fois par bloc (une par voix VAP), son oscillateur 0xfa94 fait à
lui seul 16 % du temps audio. Elle est atteinte par le relais 0x601315e0, **slot 4** des vtables des voix
VAP (0x6008baac et 0x6008bae4). Autres slots : 2 = aiguillage des événements (type 4 note, 5 fin de
note, 0 / 1 porte, 8 pitch bend…), 6 = « voix active ? » (octet +0x114).

**État d'une voix (+0x114), observé en envoyant des événements :** 0 au repos, 2 pendant la note, 4
pendant le relâchement, 0 à la fin du relâchement (~230 ms ici). 0x6c00 remet à zéro puis remplit son
tampon de 128 flottants (4e mot de ses arguments).

**Voice Sleep :** `cfw/src/voice_sleep.c` remplace le slot 4 : état 0 → tampon mis à zéro, voix non
calculée ; sinon, calcul normal. *Menu → Voice Sleep* l'active ou le coupe (inactif au démarrage tant
que ce n'est pas validé à l'oreille sur la machine).

| Projet par défaut, silence (virtuel, exact_count) | Charge |
|---|---|
| Voice Sleep inactif | 23,8 % |
| **Voice Sleep actif** | **13,3 %** |

Reste : ACD (~3 points) et WTFM (~3,5 points), même méthode (leurs vtables : appels de 0xe828 et
0xf29c dans le profil). Limite connue : le projet par défaut du virtuel ne sonne pas, donc la
non-régression **audible** reste à vérifier sur la machine (comparer avec et sans).
