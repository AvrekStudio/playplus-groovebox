/* Répétition des crans (27/11, demande du propriétaire) : un pad ou un bouton du Play+ relié à un cran
 * d'encodeur (« Encodeur → pad (+/-) » de la page de câblage : MT_ENC avec MA_INC / MA_DEC) ou au MASTER (+/-),
 * tenu, répète son cran comme une touche de clavier : 400 ms après l'appui, puis toutes les 70 ms (14 crans
 * par seconde), jusqu'au relâchement. Un menu défile ainsi cran par cran. Les valeurs fixes (MA_VAL : « ALGO 3 »)
 * et les boutons du FM-1 ne répètent pas.
 * Le fichier qui l'inclut définit REP_APPLY(m) : appliquer UN cran de l'entrée m (son apply(m, 0, 0, 0)), et
 * appelle rep_event à chaque appui / relâchement (source : pad 0..159, bouton 0x100 + n) et rep_tick dans sa
 * boucle. Tout se passe dans la boucle (événements et tick), rien sous interruption. */
#ifndef AUTOREP_H
#define AUTOREP_H
#define REP_FIRST_MS 400u
#define REP_MS 70u
static struct { const struct pp_map *m; uint32_t src, t; } rep;

static int rep_ok(const struct pp_map *m)
{
    return (m->t == MT_ENC && (m->act == MA_INC || m->act == MA_DEC)) || m->t == MT_MASTER;
}

static void rep_event(const struct pp_map *m, uint32_t src, uint32_t state, uint32_t now)
{
    if (state == 0u && rep_ok(m)) {
        rep.m = m;
        rep.src = src;
        rep.t = now + REP_FIRST_MS;
    } else if (state == 1u && rep.m && rep.src == src)
        rep.m = 0;
}

static void rep_tick(uint32_t now)
{
    if (rep.m && (int32_t)(now - rep.t) >= 0) {
        rep.t = now + REP_MS;
        REP_APPLY(rep.m);
    }
}
#endif
