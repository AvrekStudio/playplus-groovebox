MEMORY { FLASH (rx) : ORIGIN = 0x60200000, LENGTH = 8M  RAM (rwx) : ORIGIN = 0x80000000, LENGTH = 4M }
SECTIONS {
  .text : { *(.text*) *(.rodata*) } > FLASH
  .init_array : { __init_array_start = .; KEEP(*(.init_array*)) __init_array_end = .; } > FLASH
  .data : { *(.data*) } > RAM AT > FLASH
  .bss (NOLOAD) : { *(.bss*) *(COMMON) } > RAM
  /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.comment) }
}
