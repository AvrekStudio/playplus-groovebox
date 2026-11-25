/* Édition de liens d'un invité PGI : tout est placé à PGI_BASE (OCRAM), en-tête en premier. */
MEMORY { OCRAM (rwx) : ORIGIN = 0x20200000, LENGTH = 512K }
SECTIONS
{
    .text : { KEEP(*(.pgi_header)) *(.text .text.*) *(.rodata .rodata.*) } > OCRAM
    .data : ALIGN(4) { *(.data .data.*) } > OCRAM
    . = ALIGN(4);
    __pgi_image_end = .;
    .bss (NOLOAD) : ALIGN(4) { *(.bss .bss.* COMMON) } > OCRAM
    . = ALIGN(4);
    __pgi_bss_end = .;
    /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.comment) *(.ARM.attributes) }
}
