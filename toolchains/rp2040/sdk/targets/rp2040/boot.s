    .syntax unified
    .cpu cortex-m0plus
    .thumb

    /* W25Q080 256-byte stage2 bootloader with CRC32 checksum */
    .section .boot2, "ax"
    .align 2
    .global __boot2
__boot2:
    .byte 0x00, 0xb5, 0x32, 0x4b, 0x21, 0x20, 0x58, 0x60, 0x98, 0x68, 0x02, 0x21, 0x88, 0x43, 0x98, 0x60
    .byte 0xd8, 0x60, 0x18, 0x61, 0x58, 0x61, 0x2e, 0x4b, 0x00, 0x21, 0x99, 0x60, 0x02, 0x21, 0x59, 0x61
    .byte 0x01, 0x21, 0xf0, 0x22, 0x99, 0x50, 0x2b, 0x49, 0x19, 0x60, 0x01, 0x21, 0x99, 0x60, 0x35, 0x20
    .byte 0x00, 0xf0, 0x44, 0xf8, 0x02, 0x22, 0x90, 0x42, 0x14, 0xd0, 0x06, 0x21, 0x19, 0x66, 0x00, 0xf0
    .byte 0x34, 0xf8, 0x19, 0x6e, 0x01, 0x21, 0x19, 0x66, 0x00, 0x20, 0x18, 0x66, 0x1a, 0x66, 0x00, 0xf0
    .byte 0x2c, 0xf8, 0x19, 0x6e, 0x19, 0x6e, 0x19, 0x6e, 0x05, 0x20, 0x00, 0xf0, 0x2f, 0xf8, 0x01, 0x21
    .byte 0x08, 0x42, 0xf9, 0xd1, 0x00, 0x21, 0x99, 0x60, 0x1b, 0x49, 0x19, 0x60, 0x00, 0x21, 0x59, 0x60
    .byte 0x1a, 0x49, 0x1b, 0x48, 0x01, 0x60, 0x01, 0x21, 0x99, 0x60, 0xeb, 0x21, 0x19, 0x66, 0xa0, 0x21
    .byte 0x19, 0x66, 0x00, 0xf0, 0x12, 0xf8, 0x00, 0x21, 0x99, 0x60, 0x16, 0x49, 0x14, 0x48, 0x01, 0x60
    .byte 0x01, 0x21, 0x99, 0x60, 0x01, 0xbc, 0x00, 0x28, 0x00, 0xd0, 0x00, 0x47, 0x12, 0x48, 0x13, 0x49
    .byte 0x08, 0x60, 0x03, 0xc8, 0x80, 0xf3, 0x08, 0x88, 0x08, 0x47, 0x03, 0xb5, 0x99, 0x6a, 0x04, 0x20
    .byte 0x01, 0x42, 0xfb, 0xd0, 0x01, 0x20, 0x01, 0x42, 0xf8, 0xd1, 0x03, 0xbd, 0x02, 0xb5, 0x18, 0x66
    .byte 0x18, 0x66, 0xff, 0xf7, 0xf2, 0xff, 0x18, 0x6e, 0x18, 0x6e, 0x02, 0xbd, 0x00, 0x00, 0x02, 0x40
    .byte 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x07, 0x00, 0x00, 0x03, 0x5f, 0x00, 0x21, 0x22, 0x00, 0x00
    .byte 0xf4, 0x00, 0x00, 0x18, 0x22, 0x20, 0x00, 0xa0, 0x00, 0x01, 0x00, 0x10, 0x08, 0xed, 0x00, 0xe0
    .byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x74, 0xb2, 0x4e, 0x7a

    /* Cortex-M0+ Vector Table (16 core + 26 RP2040 IRQs) */
    .section .vectors, "ax"
    .align 2
    .global __vectors
__vectors:
    .word __stack_top
    .word Reset_Handler
    .word NMI_Handler
    .word HardFault_Handler
    .word 0
    .word 0
    .word 0
    .word 0
    .word 0
    .word 0
    .word 0
    .word SVC_Handler
    .word 0
    .word 0
    .word PendSV_Handler
    .word SysTick_Handler
    /* External Interrupts (IRQ 0..25) */
    .word TIMER_IRQ_0_Handler
    .word TIMER_IRQ_1_Handler
    .word TIMER_IRQ_2_Handler
    .word TIMER_IRQ_3_Handler
    .word PWM_IRQ_WRAP_Handler
    .word USBCTRL_IRQ_Handler
    .word XIP_IRQ_Handler
    .word PIO0_IRQ_0_Handler
    .word PIO0_IRQ_1_Handler
    .word PIO1_IRQ_0_Handler
    .word PIO1_IRQ_1_Handler
    .word DMA_IRQ_0_Handler
    .word DMA_IRQ_1_Handler
    .word IO_IRQ_BANK0_Handler
    .word IO_IRQ_QSPI_Handler
    .word SIO_IRQ_PROC0_Handler
    .word SIO_IRQ_PROC1_Handler
    .word CLOCKS_IRQ_Handler
    .word SPI0_IRQ_Handler
    .word SPI1_IRQ_Handler
    .word UART0_IRQ_Handler
    .word UART1_IRQ_Handler
    .word ADC_IRQ_FIFO_Handler
    .word I2C0_IRQ_Handler
    .word I2C1_IRQ_Handler
    .word RTC_IRQ_Handler

    .section .text.Reset_Handler, "ax"
    .align 2
    .global Reset_Handler
    .type Reset_Handler, %function
Reset_Handler:
    ldr r0, =__stack_top
    msr msp, r0

    /* Set VTOR = __vectors_start__ (0xE000ED08) */
    ldr r0, =0xE000ED08
    ldr r1, =__vectors_start__
    str r1, [r0]

    /* Copy .data from Flash to RAM */
    ldr r1, =_sdata
    ldr r2, =_edata
    ldr r3, =_sidata
copy_data_loop:
    cmp r1, r2
    bge copy_data_done
    ldr r0, [r3]
    str r0, [r1]
    adds r1, #4
    adds r3, #4
    b copy_data_loop
copy_data_done:

    /* Zero .bss */
    ldr r1, =_sbss
    ldr r2, =_ebss
    movs r0, #0
zero_bss_loop:
    cmp r1, r2
    bge zero_bss_done
    str r0, [r1]
    adds r1, #4
    b zero_bss_loop
zero_bss_done:

    /* Initialize clocks (XOSC 12MHz, PLL_SYS 125MHz, PLL_USB 48MHz) & USB-CDC */
    bl __loom_rp2040_early_init

    /* Enter Loom Main */
    bl Main

hang_loop:
    wfi
    b hang_loop

    .weak NMI_Handler
    .thumb_set NMI_Handler, Default_Handler

    .section .text.HardFault_Handler, "ax"
    .align 2
    .global HardFault_Handler
    .type HardFault_Handler, %function
HardFault_Handler:
    movs r0, #4
    mov r1, lr
    tst r0, r1
    beq 1f
    mrs r0, psp
    b 2f
1:
    mrs r0, msp
2:
    bl __loom_hardfault_c
    b .
    .weak SVC_Handler
    .thumb_set SVC_Handler, Default_Handler
    .weak PendSV_Handler
    .thumb_set PendSV_Handler, Default_Handler
    .weak SysTick_Handler
    .thumb_set SysTick_Handler, Default_Handler
    .weak TIMER_IRQ_0_Handler
    .thumb_set TIMER_IRQ_0_Handler, Default_Handler
    .weak TIMER_IRQ_1_Handler
    .thumb_set TIMER_IRQ_1_Handler, Default_Handler
    .weak TIMER_IRQ_2_Handler
    .thumb_set TIMER_IRQ_2_Handler, Default_Handler
    .weak TIMER_IRQ_3_Handler
    .thumb_set TIMER_IRQ_3_Handler, Default_Handler
    .weak PWM_IRQ_WRAP_Handler
    .thumb_set PWM_IRQ_WRAP_Handler, Default_Handler
    .weak USBCTRL_IRQ_Handler
    .thumb_set USBCTRL_IRQ_Handler, Default_Handler
    .weak XIP_IRQ_Handler
    .thumb_set XIP_IRQ_Handler, Default_Handler
    .weak PIO0_IRQ_0_Handler
    .thumb_set PIO0_IRQ_0_Handler, Default_Handler
    .weak PIO0_IRQ_1_Handler
    .thumb_set PIO0_IRQ_1_Handler, Default_Handler
    .weak PIO1_IRQ_0_Handler
    .thumb_set PIO1_IRQ_0_Handler, Default_Handler
    .weak PIO1_IRQ_1_Handler
    .thumb_set PIO1_IRQ_1_Handler, Default_Handler
    .weak DMA_IRQ_0_Handler
    .thumb_set DMA_IRQ_0_Handler, Default_Handler
    .weak DMA_IRQ_1_Handler
    .thumb_set DMA_IRQ_1_Handler, Default_Handler
    .weak IO_IRQ_BANK0_Handler
    .thumb_set IO_IRQ_BANK0_Handler, Default_Handler
    .weak IO_IRQ_QSPI_Handler
    .thumb_set IO_IRQ_QSPI_Handler, Default_Handler
    .weak SIO_IRQ_PROC0_Handler
    .thumb_set SIO_IRQ_PROC0_Handler, Default_Handler
    .weak SIO_IRQ_PROC1_Handler
    .thumb_set SIO_IRQ_PROC1_Handler, Default_Handler
    .weak CLOCKS_IRQ_Handler
    .thumb_set CLOCKS_IRQ_Handler, Default_Handler
    .weak SPI0_IRQ_Handler
    .thumb_set SPI0_IRQ_Handler, Default_Handler
    .weak SPI1_IRQ_Handler
    .thumb_set SPI1_IRQ_Handler, Default_Handler
    .weak UART0_IRQ_Handler
    .thumb_set UART0_IRQ_Handler, Default_Handler
    .weak UART1_IRQ_Handler
    .thumb_set UART1_IRQ_Handler, Default_Handler
    .weak ADC_IRQ_FIFO_Handler
    .thumb_set ADC_IRQ_FIFO_Handler, Default_Handler
    .weak I2C0_IRQ_Handler
    .thumb_set I2C0_IRQ_Handler, Default_Handler
    .weak I2C1_IRQ_Handler
    .thumb_set I2C1_IRQ_Handler, Default_Handler
    .weak RTC_IRQ_Handler
    .thumb_set RTC_IRQ_Handler, Default_Handler

    .section .text.Default_Handler, "ax"
    .type Default_Handler, %function
Default_Handler:
    b .

    .section .text.reset_usb_boot, "ax"
    .global reset_usb_boot
    .type reset_usb_boot, %function
    .thumb_func
reset_usb_boot:
    push {r4, lr}
    movs r1, #0x42       /* 'B' */
    lsls r1, r1, #8
    movs r0, #0x55       /* 'U' */
    orrs r1, r1, r0      /* 'UB' in r1 */
    ldrh r2, [r1]        /* dummy read to avoid unused */
    /* Lookup ROM table address at 0x00000014 */
    movs r3, #0x14
    ldrh r3, [r3]
    /* ROM lookup function at 0x00000018 */
    movs r2, #0x18
    ldrh r2, [r2]
    movs r0, r3
    blx r2               /* lookup_fn(table, 'UB') -> returns fn ptr in r0 */
    cmp r0, #0
    beq 1f
    movs r1, #0
    blx r0               /* reset_usb_boot(0, 0) */
1:
    pop {r4, pc}
