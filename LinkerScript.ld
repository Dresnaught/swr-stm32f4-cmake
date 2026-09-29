ENTRY(Reset_Handler)

/* Specify the memory areas for STM32F401RCT6 */
MEMORY
{
  FLASH (rx)      : ORIGIN = 0x08000000, LENGTH = 256K
  RAM (xrw)       : ORIGIN = 0x20000000, LENGTH = 64K
}

SECTIONS
{
  /* The startup code goes first into FLASH */
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } >FLASH

  /* The program code and other data goes into FLASH */
  .text :
  {
    . = ALIGN(4);
    *(.text)           
    *(.text*)          
    *(.rodata)         
    *(.rodata*)
    
    . = ALIGN(4);
    KEEP(*(.init))
    KEEP(*(.fini))
    . = ALIGN(4);
    
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP(*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
    
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP(*(SORT(.init_array.*)))
    KEEP(*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
    
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP(*(SORT(.fini_array.*)))
    KEEP(*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
    /* -------------------------------------------------- */      
    . = ALIGN(4);
  } >FLASH

  /* Used by the startup to initialize data */
  _sidata = LOADADDR(.data);

  /* Initialized data sections go into RAM */
  .data :
  {
    . = ALIGN(4);
    _sdata = .;        
    *(.data)           
    *(.data*)          
    . = ALIGN(4);
    _edata = .;        
  } >RAM AT> FLASH

  /* Uninitialized data section */
  .bss :
  {
    . = ALIGN(4);
    _sbss = .;         
    *(.bss)
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;         
  } >RAM

  /* Define user stack pointer address top boundary */
  _estack = ORIGIN(RAM) + LENGTH(RAM);
}
