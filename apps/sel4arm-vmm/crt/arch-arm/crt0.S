.text

/*
 * Image Entry point.
 */
.global _start
_start:
    /* Setup a stack for ourselves. */
    ldr     sp, =_stack_top

    /* Setup bootinfo. The pointer to the bootinfo struct starts in 'r0'. */
    bl      seL4_InitBootInfo

    /* Call main. */
    bl      main
    b       exit

/* .text Literal Pool */
.pool

/* Stack for the image. */
.bss
.align  4
_stack_bottom:
.space  65536
_stack_top:

