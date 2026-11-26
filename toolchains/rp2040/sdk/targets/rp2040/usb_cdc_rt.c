/*
 * Loom-Pico Bare-Metal Runtime: RP2040 Clocks + USB-CDC ACM Stdio
 * Freestanding C99 for ARM Cortex-M0+ (thumbv6m-none-eabi).
 */
#include <stdint.h>
#include <stddef.h>

#define REG32(addr) (*(volatile uint32_t *)(uintptr_t)(addr))

/* Resets */
#define RESETS_BASE         0x4000C000u
#define RESETS_RESET        REG32(RESETS_BASE + 0x00u)
#define RESETS_RESET_DONE   REG32(RESETS_BASE + 0x08u)
#define RESET_IO_BANK0      (1u << 5)
#define RESET_PADS_BANK0    (1u << 8)
#define RESET_PLL_SYS       (1u << 12)
#define RESET_PLL_USB       (1u << 13)
#define RESET_TIMER         (1u << 21)
#define RESET_USBCTRL       (1u << 24)

/* XOSC */
#define XOSC_BASE           0x40024000u
#define XOSC_CTRL           REG32(XOSC_BASE + 0x00u)
#define XOSC_STATUS         REG32(XOSC_BASE + 0x04u)
#define XOSC_STARTUP        REG32(XOSC_BASE + 0x0Cu)

/* Clocks */
#define CLOCKS_BASE         0x40008000u
#define CLK_REF_CTRL        REG32(CLOCKS_BASE + 0x30u)
#define CLK_REF_SELECTED    REG32(CLOCKS_BASE + 0x38u)
#define CLK_SYS_CTRL        REG32(CLOCKS_BASE + 0x3Cu)
#define CLK_SYS_DIV         REG32(CLOCKS_BASE + 0x40u)
#define CLK_SYS_SELECTED    REG32(CLOCKS_BASE + 0x44u)
#define CLK_PERI_CTRL       REG32(CLOCKS_BASE + 0x48u)
#define CLK_USB_CTRL        REG32(CLOCKS_BASE + 0x54u)
#define CLK_USB_DIV         REG32(CLOCKS_BASE + 0x58u)
#define CLK_ADC_CTRL        REG32(CLOCKS_BASE + 0x60u)
#define CLK_ADC_DIV         REG32(CLOCKS_BASE + 0x64u)

/* PLLs */
#define PLL_SYS_BASE        0x40028000u
#define PLL_USB_BASE        0x4002C000u
#define PLL_CS(b)           REG32((b) + 0x00u)
#define PLL_PWR(b)          REG32((b) + 0x04u)
#define PLL_FBDIV_INT(b)    REG32((b) + 0x08u)
#define PLL_PRIM(b)         REG32((b) + 0x0Cu)

/* Watchdog tick (1 us for Timer) */
#define WATCHDOG_TICK       REG32(0x4005802Cu)

/* NVIC */
#define NVIC_ISER           REG32(0xE000E100u)
#define USBCTRL_IRQ_NUM     5u

/* USBCTRL */
#define USB_DPRAM_BASE      0x50100000u
#define USB_REGS_BASE       0x50110000u
#define USB_ADDR_ENDP       REG32(USB_REGS_BASE + 0x00u)
#define USB_MAIN_CTRL       REG32(USB_REGS_BASE + 0x40u)
#define USB_SOF_WR          REG32(USB_REGS_BASE + 0x44u)
#define USB_SIE_CTRL        REG32(USB_REGS_BASE + 0x4Cu)
#define USB_SIE_STATUS      REG32(USB_REGS_BASE + 0x50u)
#define USB_INT_EP_CTRL     REG32(USB_REGS_BASE + 0x54u)
#define USB_BUFF_STATUS     REG32(USB_REGS_BASE + 0x58u)
#define USB_MUXING          REG32(USB_REGS_BASE + 0x74u)
#define USB_PWR             REG32(USB_REGS_BASE + 0x78u)
#define USB_INTE            REG32(USB_REGS_BASE + 0x90u)
#define USB_INTS            REG32(USB_REGS_BASE + 0x98u)

#define USB_EP_CTRL_OFFSET(ep, in) (0x08u + ((ep) - 1u) * 8u + ((in) ? 0u : 4u))
#define USB_BUF_CTRL_OFFSET(ep, in) (0x80u + (ep) * 8u + ((in) ? 0u : 4u))
#define USB_EP_CTRL(ep, in)  REG32(USB_DPRAM_BASE + USB_EP_CTRL_OFFSET(ep, in))
#define USB_BUF_CTRL(ep, in) REG32(USB_DPRAM_BASE + USB_BUF_CTRL_OFFSET(ep, in))

#define USB_EP0_BUF          ((volatile uint8_t *)(uintptr_t)(USB_DPRAM_BASE + 0x100u))
#define USB_EP1_IN_BUF_OFF   0x180u
#define USB_EP2_IN_BUF_OFF   0x1C0u
#define USB_EP2_OUT_BUF_OFF  0x200u
#define USB_EP2_IN_BUF       ((volatile uint8_t *)(uintptr_t)(USB_DPRAM_BASE + USB_EP2_IN_BUF_OFF))
#define USB_EP2_OUT_BUF      ((volatile uint8_t *)(uintptr_t)(USB_DPRAM_BASE + USB_EP2_OUT_BUF_OFF))

/* Freestanding memory helpers */
void *memcpy(void *dst, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    while (n--) *d++ = *s++;
    return dst;
}

void *memset(void *dst, int c, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    while (n--) *d++ = (uint8_t)c;
    return dst;
}

void *memmove(void *dst, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    if (d < s) {
        while (n--) *d++ = *s++;
    } else if (d > s) {
        d += n; s += n;
        while (n--) *--d = *--s;
    }
    return dst;
}

size_t strlen(const char *s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

static void unreset_block_wait(uint32_t mask) {
    RESETS_RESET &= ~mask;
    while ((RESETS_RESET_DONE & mask) != mask) {}
}

static void init_pll(uint32_t base, uint32_t fbdiv, uint32_t postdiv1, uint32_t postdiv2) {
    PLL_PWR(base) = 0x2Du; /* Power down */
    PLL_FBDIV_INT(base) = fbdiv;
    PLL_PWR(base) = 0x0Cu; /* Power up VCO & main */
    while ((PLL_CS(base) & (1u << 31)) == 0) {}
    PLL_PRIM(base) = (postdiv1 << 16) | (postdiv2 << 12);
    PLL_PWR(base) = 0x00u; /* Power up postdivs */
}

static void init_clocks(void) {
    /* 12 MHz XOSC */
    XOSC_CTRL = 0xAA0u;
    XOSC_STARTUP = 47u;
    XOSC_CTRL = (0xFABu << 12) | 0xAA0u;
    while ((XOSC_STATUS & (1u << 31)) == 0) {}

    /* clk_ref = XOSC (12 MHz) */
    CLK_REF_CTRL = 0x2u;
    while (CLK_REF_SELECTED != (1u << 2)) {}

    /* Reset & configure PLL_SYS = 125 MHz (12*125 / (6*2) = 125 MHz) */
    RESETS_RESET |= RESET_PLL_SYS | RESET_PLL_USB;
    unreset_block_wait(RESET_PLL_SYS | RESET_PLL_USB);

    init_pll(PLL_SYS_BASE, 125u, 6u, 2u);
    init_pll(PLL_USB_BASE, 100u, 5u, 5u); /* 12*100 / (5*5) = 48 MHz */

    /* clk_sys = PLL_SYS (125 MHz) */
    CLK_SYS_DIV = 0x100u;
    CLK_SYS_CTRL = (0u << 5) | 0x1u;
    while (CLK_SYS_SELECTED != (1u << 1)) {}

    /* clk_peri = clk_sys (125 MHz) */
    CLK_PERI_CTRL = (1u << 11) | (0u << 5);

    /* clk_usb = PLL_USB (48 MHz) */
    CLK_USB_DIV = 0x100u;
    CLK_USB_CTRL = (1u << 11) | (0u << 5);

    /* clk_adc = PLL_USB (48 MHz) */
    CLK_ADC_DIV = 0x100u;
    CLK_ADC_CTRL = (1u << 11) | (0u << 5);

    /* Watchdog 1 us tick = 12 MHz / 12 */
    WATCHDOG_TICK = (1u << 9) | 12u;
}

/* USB-CDC Descriptors */
static const uint8_t dev_desc[18] = {
    18, 0x01, 0x00, 0x02, 0x02, 0x00, 0x00, 64,
    0x8A, 0x2E, 0x0A, 0x00, /* VID 0x2E8A (Raspberry Pi), PID 0x000A (Pico CDC) */
    0x00, 0x01, 1, 2, 3, 1
};

static const uint8_t cfg_desc[67] = {
    /* Configuration Descriptor */
    9, 0x02, 67, 0, 2, 1, 0, 0x80, 50,
    /* Interface 0: CDC Control */
    9, 0x04, 0, 0, 1, 0x02, 0x02, 0x01, 0,
    /* CDC Header */
    5, 0x24, 0x00, 0x10, 0x01,
    /* CDC Call Management */
    5, 0x24, 0x01, 0x00, 1,
    /* CDC ACM */
    4, 0x24, 0x02, 0x02,
    /* CDC Union */
    5, 0x24, 0x06, 0, 1,
    /* EP1 IN (Interrupt) */
    7, 0x05, 0x81, 0x03, 8, 0, 16,
    /* Interface 1: CDC Data */
    9, 0x04, 1, 0, 2, 0x0A, 0x00, 0x00, 0,
    /* EP2 OUT (Bulk) */
    7, 0x05, 0x02, 0x02, 64, 0, 0,
    /* EP2 IN (Bulk) */
    7, 0x05, 0x82, 0x02, 64, 0, 0
};

static const uint8_t str0[4]  = { 4, 0x03, 0x09, 0x04 };
static const uint8_t str1[10] = { 10, 0x03, 'L',0,'o',0,'o',0,'m',0 };
static const uint8_t str2[20] = { 20, 0x03, 'L',0,'o',0,'o',0,'m',0,'-',0,'P',0,'i',0,'c',0,'o',0 };
static const uint8_t str3[14] = { 14, 0x03, '0',0,'0',0,'0',0,'0',0,'0',0,'1',0 };

static volatile uint8_t usb_configured = 0;
static volatile uint8_t usb_pending_addr = 0;
static volatile uint8_t usb_set_addr_pending = 0;
static volatile uint8_t ep0_in_pid = 1;
static volatile uint8_t ep2_in_pid = 0;
static volatile uint8_t ep2_out_pid = 0;
static volatile uint8_t ep2_tx_busy = 0;

static uint8_t line_coding[7] = { 0x00, 0xC2, 0x01, 0x00, 0x00, 0x00, 0x08 }; /* 115200 8N1 */

#define CDC_BUF_SIZE 512u
static volatile uint8_t tx_buf[CDC_BUF_SIZE];
static volatile uint32_t tx_head = 0, tx_tail = 0;
static volatile uint8_t rx_buf[CDC_BUF_SIZE];
static volatile uint32_t rx_head = 0, rx_tail = 0;

static void ep0_send(const uint8_t *data, uint16_t len) {
    if (len > 64u) len = 64u;
    for (uint16_t i = 0; i < len; ++i) USB_EP0_BUF[i] = data[i];
    uint32_t pid = (ep0_in_pid ? (1u << 13) : 0u);
    ep0_in_pid ^= 1u;
    USB_BUF_CTRL(0, 1) = (uint32_t)len | (1u << 15) | pid | (1u << 10);
}

static void ep2_arm_out(void) {
    uint32_t pid = (ep2_out_pid ? (1u << 13) : 0u);
    ep2_out_pid ^= 1u;
    USB_BUF_CTRL(2, 0) = 64u | pid | (1u << 10);
}

static void ep2_kick_tx(void) {
    if (!usb_configured || ep2_tx_busy || tx_head == tx_tail) return;
    uint32_t count = 0;
    while (count < 64u && tx_tail != tx_head) {
        USB_EP2_IN_BUF[count++] = tx_buf[tx_tail];
        tx_tail = (tx_tail + 1u) % CDC_BUF_SIZE;
    }
    ep2_tx_busy = 1u;
    uint32_t pid = (ep2_in_pid ? (1u << 13) : 0u);
    ep2_in_pid ^= 1u;
    USB_BUF_CTRL(2, 1) = count | (1u << 15) | pid | (1u << 10);
}

static void handle_setup(void) {
    volatile uint8_t *setup = (volatile uint8_t *)(uintptr_t)USB_DPRAM_BASE;
    uint8_t bmRequestType = setup[0];
    uint8_t bRequest = setup[1];
    uint16_t wValue = (uint16_t)setup[2] | ((uint16_t)setup[3] << 8);
    uint16_t wLength = (uint16_t)setup[6] | ((uint16_t)setup[7] << 8);

    ep0_in_pid = 1u;

    if ((bmRequestType & 0x60u) == 0x00u) { /* Standard request */
        if (bRequest == 0x06u) { /* GET_DESCRIPTOR */
            uint8_t dtype = (uint8_t)(wValue >> 8);
            uint8_t didx  = (uint8_t)(wValue & 0xFFu);
            if (dtype == 0x01u) {
                uint16_t n = wLength < sizeof(dev_desc) ? wLength : sizeof(dev_desc);
                ep0_send(dev_desc, n);
                return;
            } else if (dtype == 0x02u) {
                uint16_t n = wLength < sizeof(cfg_desc) ? wLength : sizeof(cfg_desc);
                if (n > 64u) n = 64u;
                ep0_send(cfg_desc, n);
                return;
            } else if (dtype == 0x03u) {
                const uint8_t *s = str0;
                uint16_t slen = sizeof(str0);
                if (didx == 1u) { s = str1; slen = sizeof(str1); }
                else if (didx == 2u) { s = str2; slen = sizeof(str2); }
                else if (didx == 3u) { s = str3; slen = sizeof(str3); }
                ep0_send(s, wLength < slen ? wLength : slen);
                return;
            }
        } else if (bRequest == 0x05u) { /* SET_ADDRESS */
            usb_pending_addr = (uint8_t)(wValue & 0x7Fu);
            usb_set_addr_pending = 1u;
            ep0_send(NULL, 0);
            return;
        } else if (bRequest == 0x09u) { /* SET_CONFIGURATION */
            usb_configured = 1u;
            ep2_in_pid = 0u;
            ep2_out_pid = 0u;
            ep2_tx_busy = 0u;
            /* Enable EP1 IN (Interrupt), EP2 IN (Bulk), EP2 OUT (Bulk) */
            USB_EP_CTRL(1, 1) = (1u << 31) | (3u << 26) | USB_EP1_IN_BUF_OFF;
            USB_EP_CTRL(2, 1) = (1u << 31) | (2u << 26) | USB_EP2_IN_BUF_OFF;
            USB_EP_CTRL(2, 0) = (1u << 31) | (2u << 26) | USB_EP2_OUT_BUF_OFF;
            ep2_arm_out();
            ep0_send(NULL, 0);
            return;
        }
    } else if ((bmRequestType & 0x60u) == 0x20u) { /* CDC Class request */
        if (bRequest == 0x21u) { /* GET_LINE_CODING */
            ep0_send(line_coding, wLength < 7u ? wLength : 7u);
            return;
        }
        /* SET_LINE_CODING (0x20) / SET_CONTROL_LINE_STATE (0x22) */
        ep0_send(NULL, 0);
        return;
    }
    ep0_send(NULL, 0);
}

void USBCTRL_IRQ_Handler(void) {
    uint32_t status = USB_INTS;
    if (status & (1u << 12)) { /* BUS_RESET */
        USB_SIE_STATUS = (1u << 19);
        USB_ADDR_ENDP = 0u;
        usb_configured = 0u;
        usb_set_addr_pending = 0u;
        ep2_tx_busy = 0u;
    }
    if (status & (1u << 16)) { /* SETUP_REQ */
        USB_SIE_STATUS = (1u << 17);
        handle_setup();
    }
    if (status & (1u << 4)) { /* BUFF_STATUS */
        uint32_t bstat = USB_BUFF_STATUS;
        USB_BUFF_STATUS = bstat;
        if (bstat & (1u << 0)) { /* EP0 IN */
            if (usb_set_addr_pending) {
                USB_ADDR_ENDP = usb_pending_addr;
                usb_set_addr_pending = 0u;
            } else {
                USB_BUF_CTRL(0, 0) = (1u << 13) | (1u << 10); /* Arm EP0 OUT ZLP */
            }
        }
        if (bstat & (1u << 4)) { /* EP2 IN complete */
            ep2_tx_busy = 0u;
            ep2_kick_tx();
        }
        if (bstat & (1u << 5)) { /* EP2 OUT received */
            uint32_t len = USB_BUF_CTRL(2, 0) & 0x3FFu;
            for (uint32_t i = 0; i < len; ++i) {
                uint32_t next = (rx_head + 1u) % CDC_BUF_SIZE;
                if (next != rx_tail) {
                    rx_buf[rx_head] = USB_EP2_OUT_BUF[i];
                    rx_head = next;
                }
            }
            ep2_arm_out();
        }
    }
}

static void init_usb_cdc(void) {
    unreset_block_wait(RESET_USBCTRL);
    memset((void *)(uintptr_t)USB_DPRAM_BASE, 0, 4096u);
    USB_MUXING = (1u << 0) | (1u << 3); /* TO_PHY | SOFTCON */
    USB_PWR = (1u << 2) | (1u << 3);    /* VBUS_DETECT | VBUS_DETECT_OVERRIDE_EN */
    USB_MAIN_CTRL = (1u << 0);          /* CONTROLLER_EN */
    USB_INTE = (1u << 4) | (1u << 12) | (1u << 16); /* BUFF_STATUS | BUS_RESET | SETUP_REQ */
    USB_SIE_CTRL = (1u << 29) | (1u << 16); /* EP0_INT_1BUF | PULLUP_EN */
    NVIC_ISER = (1u << USBCTRL_IRQ_NUM);
}

/* Cortex-M0+ MPU */
#define MPU_TYPE            REG32(0xE000ED90u)
#define MPU_CTRL            REG32(0xE000ED94u)
#define MPU_RNR             REG32(0xE000ED98u)
#define MPU_RBAR            REG32(0xE000ED9Cu)
#define MPU_RASR            REG32(0xE000EDA0u)

extern uint32_t _ebss;
extern uint32_t __stack_top;

static void init_mpu(void) {
    if ((MPU_TYPE & 0xFF00u) == 0u) return;
    MPU_CTRL = 0u;

    /* Region 0: Flash XIP (0x10000000, 2MB, RO+Exec) */
    MPU_RNR = 0u;
    MPU_RBAR = 0x10000000u;
    MPU_RASR = (6u << 24) | (1u << 17) | (20u << 1) | 1u;

    /* Region 1: 256-byte no-access stack overflow guard above .bss */
    uintptr_t guard = ((uintptr_t)&_ebss + 255u) & ~(uintptr_t)255u;
    uintptr_t stack_top = (uintptr_t)&__stack_top;
    if (guard + 1024u < stack_top) {
        MPU_RNR = 1u;
        MPU_RBAR = (uint32_t)guard;
        /* XN=1, AP=000 (No access), SIZE=7 (256B), ENABLE=1 */
        MPU_RASR = (1u << 28) | (7u << 1) | 1u;
    }

    /* Enable MPU + PRIVDEFENA (default background map for peripherals/RAM) */
    MPU_CTRL = (1u << 0) | (1u << 2);
    __asm__ volatile("dsb\n\tisb" ::: "memory");
}

#define SRAM_VTOR_BASE 0x20040000u
extern const uint32_t __vectors_start__;

static void relocate_vtor_to_sram(void) {
    volatile uint32_t* src = (volatile uint32_t*)&__vectors_start__;
    volatile uint32_t* dst = (volatile uint32_t*)SRAM_VTOR_BASE;
    for (int i = 0; i < 48; ++i) {
        dst[i] = src[i];
    }
    *(volatile uint32_t*)0xE000ED08 = SRAM_VTOR_BASE;
    __asm__ volatile("dsb\n\tisb" ::: "memory");
}

void __loom_rp2040_early_init(void) {
    init_mpu();
    relocate_vtor_to_sram();
    init_clocks();
    unreset_block_wait(RESET_IO_BANK0 | RESET_PADS_BANK0 | RESET_TIMER);
    init_usb_cdc();
}

uint32_t __loom_irq_save(void) {
    uint32_t primask;
    __asm__ volatile("mrs %0, primask\n\tcpsid i" : "=r"(primask) :: "memory");
    return primask;
}

void __loom_irq_restore(uint32_t primask) {
    __asm__ volatile("msr primask, %0" :: "r"(primask) : "memory");
}

/* Output Sink Routing */
typedef enum {
    LOOM_SINK_USB = 0,
    LOOM_SINK_UART0 = 1,
    LOOM_SINK_UART1 = 2,
    LOOM_SINK_NULL = 3
} LoomOutputSink;

static volatile uint32_t g_output_sink = LOOM_SINK_USB;

void __loom_set_output_sink(uint32_t sink) {
    g_output_sink = sink;
}

uint32_t __loom_get_output_sink(void) {
    return g_output_sink;
}

/* Forward declaration for UART register base */
#define UART0_BASE_ADDR 0x40034000u
#define UART1_BASE_ADDR 0x40038000u
#define UART_DR_OFFSET  0x000u
#define UART_FR_OFFSET  0x018u
#define UART_FR_TXFF    (1u << 5)

static void uart_putc_direct(uint32_t base, uint8_t ch) {
    volatile uint32_t* fr = (volatile uint32_t*)(base + UART_FR_OFFSET);
    volatile uint32_t* dr = (volatile uint32_t*)(base + UART_DR_OFFSET);
    while (*fr & UART_FR_TXFF);
    *dr = (uint32_t)ch;
}

void __loom_usb_putchar(int32_t ch) {
    uint8_t c = (uint8_t)ch;
    uint32_t sink = g_output_sink;
    if (sink == LOOM_SINK_UART0) {
        uart_putc_direct(UART0_BASE_ADDR, c);
        return;
    } else if (sink == LOOM_SINK_UART1) {
        uart_putc_direct(UART1_BASE_ADDR, c);
        return;
    } else if (sink == LOOM_SINK_NULL) {
        return;
    }

    uint32_t next = (tx_head + 1u) % CDC_BUF_SIZE;
    if (next != tx_tail) {
        tx_buf[tx_head] = c;
        tx_head = next;
    }
    ep2_kick_tx();
}

void __loom_usb_write(const char *buf, uint32_t len) {
    for (uint32_t i = 0; i < len; ++i) {
        __loom_usb_putchar((int32_t)(uint8_t)buf[i]);
    }
}

void __loom_usb_write_str(const char *s) {
    if (!s) return;
    while (*s) {
        __loom_usb_putchar((int32_t)(uint8_t)*s++);
    }
}

void __loom_usb_write_u64(uint64_t v) {
    char tmp[21];
    int idx = 0;
    if (v == 0) {
        __loom_usb_putchar('0');
        return;
    }
    while (v > 0) {
        tmp[idx++] = (char)('0' + (v % 10u));
        v /= 10u;
    }
    while (idx > 0) __loom_usb_putchar(tmp[--idx]);
}

void __loom_usb_write_i64(int64_t v) {
    if (v < 0) {
        __loom_usb_putchar('-');
        __loom_usb_write_u64((uint64_t)(-(v + 1)) + 1u);
    } else {
        __loom_usb_write_u64((uint64_t)v);
    }
}

void __loom_usb_write_hex(uint64_t v) {
    const char *hex = "0123456789abcdef";
    char tmp[16];
    int idx = 0;
    if (v == 0) {
        __loom_usb_putchar('0');
        return;
    }
    while (v > 0) {
        tmp[idx++] = hex[v & 0xFu];
        v >>= 4;
    }
    while (idx > 0) __loom_usb_putchar(tmp[--idx]);
}

void __loom_usb_write_f64(double v) {
    if (v < 0.0) {
        __loom_usb_putchar('-');
        v = -v;
    }
    uint64_t whole = (uint64_t)v;
    double frac = v - (double)whole;
    __loom_usb_write_u64(whole);
    __loom_usb_putchar('.');
    uint64_t frac_scaled = (uint64_t)(frac * 1000.0 + 0.5);
    if (frac_scaled >= 1000u) frac_scaled = 999u;
    __loom_usb_putchar((int32_t)('0' + (frac_scaled / 100u) % 10u));
    __loom_usb_putchar((int32_t)('0' + (frac_scaled / 10u) % 10u));
    __loom_usb_putchar((int32_t)('0' + frac_scaled % 10u));
}

int32_t __loom_usb_available(void) {
    if (rx_head >= rx_tail) return (int32_t)(rx_head - rx_tail);
    return (int32_t)(CDC_BUF_SIZE - rx_tail + rx_head);
}

int32_t __loom_usb_read_byte(void) {
    if (rx_head == rx_tail) return -1;
    uint8_t b = rx_buf[rx_tail];
    rx_tail = (rx_tail + 1u) % CDC_BUF_SIZE;
    return (int32_t)b;
}

int32_t __loom_usb_is_configured(void) {
    return (int32_t)usb_configured;
}

void loom_panic(const char *file, int line, const char *reason) {
    __loom_usb_write_str("\n[LOOM PANIC] ");
    if (file) {
        __loom_usb_write_str(file);
        __loom_usb_putchar(':');
        __loom_usb_write_i64((int64_t)line);
        __loom_usb_write_str(": ");
    }
    if (reason) __loom_usb_write_str(reason);
    __loom_usb_putchar('\n');
    for (;;) { __asm__ volatile("wfi"); }
}

void loom_panic_overflow(const char *file, int line, const char *op, const char *type_name) {
    __loom_usb_write_str("\n[LOOM PANIC] integer overflow");
    if (op) { __loom_usb_write_str(" in "); __loom_usb_write_str(op); }
    if (type_name) { __loom_usb_write_str(" ("); __loom_usb_write_str(type_name); __loom_usb_putchar(')'); }
    if (file) { __loom_usb_write_str(" at "); __loom_usb_write_str(file); __loom_usb_putchar(':'); __loom_usb_write_i64((int64_t)line); }
    __loom_usb_putchar('\n');
    for (;;) { __asm__ volatile("wfi"); }
}

void loom_panic_bounds(const char *file, int line, int64_t index, int64_t length) {
    __loom_usb_write_str("\n[LOOM PANIC] index out of bounds: ");
    __loom_usb_write_i64(index);
    __loom_usb_write_str(" (len=");
    __loom_usb_write_i64(length);
    __loom_usb_putchar(')');
    if (file) { __loom_usb_write_str(" at "); __loom_usb_write_str(file); __loom_usb_putchar(':'); __loom_usb_write_i64((int64_t)line); }
    __loom_usb_putchar('\n');
    for (;;) { __asm__ volatile("wfi"); }
}

void loom_panic_divzero(const char *file, int line) {
    loom_panic(file, line, "division by zero");
}

void __loom_hardfault_c(const uint32_t *sp) {
    __loom_usb_write_str("\n[LOOM HARDFAULT] PC=0x");
    __loom_usb_write_hex((uint64_t)sp[6]);
    __loom_usb_write_str(" LR=0x");
    __loom_usb_write_hex((uint64_t)sp[5]);
    __loom_usb_write_str(" PSR=0x");
    __loom_usb_write_hex((uint64_t)sp[7]);
    __loom_usb_putchar('\n');
    for (;;) { __asm__ volatile("wfi"); }
}
