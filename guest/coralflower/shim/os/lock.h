/* CoralFlower sur le Play+ (27/11) : son « verrou CPU » (tools/emu/emu_hal_fw.h : fm1_irq_off / fm1_irq_on, le
 * verrou que l'émulateur prend à la place du masquage des interruptions du FM-1) devient ce qu'il remplace : le
 * masquage des interruptions (PRIMASK). Le son de CoralFlower tourne alors sous l'interruption audio du Play+ (comme
 * l'ISR ALNK0 du FM-1) et son interface dans la boucle, sans se marcher dessus : quand la boucle tient le verrou,
 * l'interruption attend. Remplace tools/emu/web/compat/os/lock.h (verrous vides : un seul fil dans le navigateur). */
#ifndef CF_OS_LOCK_H
#define CF_OS_LOCK_H
typedef int os_unfair_lock;
#define OS_UNFAIR_LOCK_INIT 0
/* diagnostic (27/11) : le plus long masquage pris par la BOUCLE (pas par l'interruption audio), en cycles ; lu et remis
 * à zéro une fois par seconde par plat_coralflower.c (bande « M » : en ms) */
static volatile unsigned long cf_mask_t0, cf_mask_max, cf_mask_pc, cf_mask_who, cf_mask_who_max;
static inline unsigned long cf_ipsr(void) { unsigned long v; __asm__ volatile("mrs %0, ipsr" : "=r"(v)); return v; }
static inline void os_unfair_lock_lock(os_unfair_lock *l)
{
    (void)l;
    __asm__ volatile("cpsid i" ::: "memory");
    if (!cf_ipsr()) {
        cf_mask_t0 = *(volatile unsigned long *)0xE0001004u;
        cf_mask_pc = (unsigned long)__builtin_return_address(0);   /* (qui a pris le verrou : diagnostic) */
    }
}
static inline void os_unfair_lock_unlock(os_unfair_lock *l)
{
    (void)l;
    if (!cf_ipsr()) {
        unsigned long d = *(volatile unsigned long *)0xE0001004u - cf_mask_t0;
        if (d > cf_mask_max)
            cf_mask_max = d;
        if (d > cf_mask_who_max)
            cf_mask_who_max = d, cf_mask_who = cf_mask_pc;
    }
    __asm__ volatile("cpsie i" ::: "memory");
}
static inline _Bool os_unfair_lock_trylock(os_unfair_lock *l)
{
    (void)l;
    __asm__ volatile("cpsid i" ::: "memory");
    return 1;
}
#endif
