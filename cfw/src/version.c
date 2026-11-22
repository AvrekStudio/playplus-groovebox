/* Version affichée au démarrage et dans Settings > Firmware.
 *
 * Crochet (target.json, « firmware version string pointer ») : l'officiel a une petite fonction qui
 * renvoie la chaîne de version (0x600c83ec, Thumb 0x600c83ed). Elle charge l'adresse de la chaîne
 * depuis son pool de littéraux, au mot 0x600c83f0, qui vaut 0x60078208 dans l'officiel. Le build
 * remplace ce mot par l'adresse de cfw_version. Rien d'autre ne change.
 *
 * La chaîne vit dans la cave : la voir sur la machine prouve que notre code a bien été flashé. Le test
 * vdev_base.py appelle cette fonction dans le Play+ virtuel et compare avec CFW_VERSION_STRING.
 * Garder le préfixe « 1.3.0- » : c'est la forme qu'utilise l'officiel. */
#include "cfw.h"
#include "target.h"

const char cfw_version[] = CFW_VERSION_STRING;
