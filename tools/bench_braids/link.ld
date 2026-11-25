MEMORY { FLASH (rx) : ORIGIN = 0x60200000, LENGTH = 4M  RAM (rwx) : ORIGIN = 0x20200000, LENGTH = 512K }
SECTIONS {
  .text : { *(.text*) *(.rodata*) } > FLASH
  .init_array : { __init_array_start = .; KEEP(*(.init_array*)) __init_array_end = .; } > FLASH
  .data : { *(.data*) } > RAM AT > FLASH
  .bss (NOLOAD) : { *(.bss*) *(COMMON) } > RAM
  /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.comment) }
}
