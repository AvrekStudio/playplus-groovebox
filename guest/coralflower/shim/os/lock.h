/* CoralFlower sur le Play+ (27/11) : son « verrou CPU » (tools/emu/emu_hal_fw.h : fm1_irq_off / fm1_irq_on, le
 * verrou que l'émulateur prend à la place du masquage des interruptions du FM-1) devient ce qu'il remplace : le
 * masquage des interruptions (PRIMASK). Le son de CoralFlower tourne alors sous l'interruption audio du Play+ (comme
 * l'ISR ALNK0 du FM-1) et son interface dans la boucle, sans se marcher dessus : quand la boucle tient le verrou,
 * l'interruption attend. Remplace tools/emu/web/compat/os/lock.h (verrous vides : un seul fil dans le navigateur). */
#ifndef CF_OS_LOCK_H
#define CF_OS_LOCK_H
typedef int os_unfair_lock;
#define OS_UNFAIR_LOCK_INIT 0
static inline void os_unfair_lock_lock(os_unfair_lock *l)
{
    (void)l;
    __asm__ volatile("cpsid i" ::: "memory");
}
static inline void os_unfair_lock_unlock(os_unfair_lock *l)
{
    (void)l;
    __asm__ volatile("cpsie i" ::: "memory");
}
static inline _Bool os_unfair_lock_trylock(os_unfair_lock *l)
{
    (void)l;
    __asm__ volatile("cpsid i" ::: "memory");
    return 1;
}
#endif
