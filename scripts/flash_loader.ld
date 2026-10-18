ENTRY(flash_entry)
MEMORY { RAM (rwx) : ORIGIN = 0x20000000, LENGTH = 0x2000 }
SECTIONS {
 .text : { KEEP(*(.entry)) *(.text*) *(.rodata*) *(.ram*) } > RAM
 .data : { *(.data*) } > RAM
 .bss (NOLOAD) : { *(.bss*) *(COMMON) } > RAM
 /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.comment*) }
}
