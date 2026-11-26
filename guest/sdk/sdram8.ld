/* Invité PGI dans la grande zone SDRAM (8 Mo, PGI_SDRAM8_BASE, Avrek OS 0.6.0) ; sinon comme loom.ld : tout en SDRAM, à PGI_SDRAM_BASE (haut de la mémoire des samples de Polyend,
 * voir cfw/src/pgi.h). Les grands tampons « .pool » d'fm1-base (zéro au départ) vont avec .bss. */
MEMORY { SDRAM (rwx) : ORIGIN = 0x81800000, LENGTH = 0x7FE000 }
SECTIONS
{
    .text : { KEEP(*(.pgi_header)) *(.text .text.*) *(.app_ram .app_ram.*) *(.rodata .rodata.*) } > SDRAM
    .data : ALIGN(4) { *(.data .data.*) } > SDRAM
    . = ALIGN(4);
    __pgi_image_end = .;
    .bss (NOLOAD) : ALIGN(8) { *(.bss .bss.* COMMON) *(.pool .pool.*) } > SDRAM
    . = ALIGN(4);
    __pgi_bss_end = .;
    /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.comment) *(.ARM.attributes) }
}
