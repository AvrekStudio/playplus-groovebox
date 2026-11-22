/* Version affichée au démarrage et dans Settings > Firmware.
 *
 * Hook (target.json) : le pointeur de chaîne de version de l'officiel est redirigé ici. La chaîne vit
 * dans la cave : la voir sur la machine prouve que notre code a bien été flashé. */
#include "cfw.h"
#include "target.h"

const char cfw_version[] = CFW_VERSION_STRING;
