#include "config.h"
#include <string.h>
#include "cpu.h"
#include "lcd.h"
#include "kernel.h"
#include "system.h"
#include "mmu-meg-fx.h"
#include <stdlib.h>
#include "memory.h"
#include "lcd-target.h"
#include "font.h"
#include "rbunicode.h"
#include "bidi.h"

#define LCDADDR(x, y) (&lcd_framebuffer[(y)][(x)])
/*
** We prepare foreground and background fills ahead of time - DMA fills in 16 byte groups
*/
unsigned long fg_pattern_blit[4];
unsigned long bg_pattern_blit[4];

volatile bool use_dma_blit = false;
static volatile bool lcd_on = true;
volatile bool lcd_poweroff = false;
/*
** These are imported from lcd-16bit.c
*/
extern unsigned fg_pattern;
extern unsigned bg_pattern;

bool lcd_enabled()
{
    return lcd_on;
}

unsigned int LCDBANK(unsigned int address)
{
    return ((address >> 22) & 0xff);
}

unsigned int LCDBASEU(unsigned int address)
{
    return (address & ((1 << 22)-1)) >> 1;
}

unsigned int LCDBASEL(unsigned int address)
{
    address += 320*240*2;
    return (address & ((1 << 22)-1)) >> 1;
}


/* LCD init */
void lcd_init_device(void)
{
    LCDSADDR1 = (LCDBANK((unsigned)FRAME) << 21) | (LCDBASEU((unsigned)FRAME));
    LCDSADDR2 = LCDBASEL((unsigned)FRAME);
    LCDSADDR3 = 0x000000F0;

    LCDCON5 |= 1 << 11;     /* Switch from 555I mode to 565 mode */

#if !defined(BOOTLOADER)
    memset16(fg_pattern_blit, fg_pattern, sizeof(fg_pattern_blit)/2);
    memset16(bg_pattern_blit, bg_pattern, sizeof(bg_pattern_blit)/2);
    clean_dcache_range((void *)fg_pattern_blit, sizeof(fg_pattern_blit));
    clean_dcache_range((void *)bg_pattern_blit, sizeof(bg_pattern_blit));
    use_dma_blit = true;
    lcd_poweroff = false;
#endif
}

/* Update a fraction of the display. */
void lcd_update_rect(int x, int y, int width, int height)
{
    (void)x;
    (void)width;
    (void)y;
    (void)height;

    if(!lcd_on)
    {
        sleep(200);
        return;
    }
    if (use_dma_blit)
    {
        /* Wait for this controller to stop pending transfer */
        while((DSTAT1 & 0x000fffff))
            CLKCON |= (1 << 2); /* set IDLE bit */

        /* Flush DCache */
        invalidate_dcache_range((void *)(((int) &lcd_framebuffer[0][0])+(y * sizeof(fb_data) * LCD_WIDTH)), (height * sizeof(fb_data) * LCD_WIDTH));

        /* set DMA dest */
        DIDST1 = ((int) FRAME) + (y * sizeof(fb_data) * LCD_WIDTH);

        /* FRAME on AHB buf, increment */
        DIDSTC1 = 0;
        /* Handshake on AHB, Burst transfer, Whole service, Don't reload, transfer 32-bits */
        DCON1 = ((1<<30) | (1<<28) |  (1<<27) | (1<<22) | (2<<20)) | ((height * sizeof(fb_data) * LCD_WIDTH) >> 4);

        /* set DMA source  */
        DISRC1 = ((int) &lcd_framebuffer[0][0]) + (y * sizeof(fb_data) * LCD_WIDTH) + 0x30000000;
        /* memory is on AHB bus, increment addresses */
        DISRCC1 = 0x00;

        /* Activate the channel */
        DMASKTRIG1 = 0x2;

        /* Start DMA */
        DMASKTRIG1 |= 0x1;

        /* Wait for transfer to complete */
        while((DSTAT1 & 0x000fffff))
            CLKCON |= (1 << 2); /* set IDLE bit */
    }
    else
        memcpy(((char*)FRAME) + (y * sizeof(fb_data) * LCD_WIDTH), ((char *)&lcd_framebuffer) + (y * sizeof(fb_data) * LCD_WIDTH), ((height * sizeof(fb_data) * LCD_WIDTH)));
}


void lcd_enable(bool state)
{
    if(!lcd_poweroff)
        return;
    if(state) {
        if(!lcd_on) {
            lcd_on = true;
            memcpy(FRAME, lcd_framebuffer, LCD_WIDTH*LCD_HEIGHT*2);
            LCDCON1 |= 1;
        }
    }
    else {
        if(lcd_on) {
            lcd_on = false;
            LCDCON1 &= ~1;
        }
    }
}

void lcd_set_foreground(unsigned color)
{
    fg_pattern = color;

    memset16(fg_pattern_blit, fg_pattern, sizeof(fg_pattern_blit)/2);
    invalidate_dcache_range((void *)fg_pattern_blit, sizeof(fg_pattern_blit));
}

void lcd_set_background(unsigned color)
{
    bg_pattern = color;
    memset16(bg_pattern_blit, bg_pattern, sizeof(bg_pattern_blit)/2);
    invalidate_dcache_range((void *)bg_pattern_blit, sizeof(bg_pattern_blit));
}

void lcd_device_prepare_backdrop(fb_data* backdrop)
{
    if(backdrop)
        invalidate_dcache_range((void *)backdrop, (LCD_HEIGHT * sizeof(fb_data) * LCD_WIDTH));
}

void lcd_clear_display_dma(void)
{
    void *src;
    bool inc = false;

    if(!lcd_on) {
        sleep(200);
    }
    if (lcd_get_drawmode() & DRMODE_INVERSEVID)
        src = fg_pattern_blit;
    else
    {
        fb_data* lcd_backdrop = lcd_get_backdrop();

        if (!lcd_backdrop)
            src = bg_pattern_blit;
        else
        {
            src = lcd_backdrop;
            inc = true;
        }
    }
    /* Wait for any pending transfer to complete */
    while((DSTAT3 & 0x000fffff))
        CLKCON |= (1 << 2); /* set IDLE bit */
    DMASKTRIG3 |= 0x4; /* Stop controller */
    DIDST3 = ((int) &lcd_framebuffer[0][0]) + 0x30000000; /* set DMA dest, physical address */
    DIDSTC3 = 0; /* Dest on AHB, increment */

    DISRC3 = ((int) src) + 0x30000000; /* Set source, in physical space */
    DISRCC3 = inc ? 0x00 : 0x01;  /* memory is on AHB bus, increment addresses based on backdrop */

    /* Handshake on AHB, Burst mode, whole service mode, no reload, move 32-bits */
    DCON3 = ((1<<30) | (1<<28) | (1<<27) | (1<<22) | (2<<20)) | ((LCD_WIDTH*LCD_HEIGHT*sizeof(fb_data)) >> 4);

    /* Dump DCache for dest, we are about to overwrite it with DMA */
    invalidate_dcache_range((void *)lcd_framebuffer, sizeof(lcd_framebuffer));
    /* Activate the channel */
    DMASKTRIG3 = 2;
    /* Start DMA */
    DMASKTRIG3 |= 1;

    /* Wait for transfer to complete */
    while((DSTAT3 & 0x000fffff))
        CLKCON |= (1 << 2); /* set IDLE bit */
}

void lcd_clear_display(void)
{
    lcd_stop_scroll();

    if(use_dma_blit)
    {
        lcd_clear_display_dma();
        return;
    }

    fb_data *dst = &lcd_framebuffer[0][0];

    if (lcd_get_drawmode() & DRMODE_INVERSEVID)
    {
        memset16(dst, fg_pattern, LCD_WIDTH*LCD_HEIGHT);
    }
    else
    {
        fb_data* lcd_backdrop = lcd_get_backdrop();
        if (!lcd_backdrop)
            memset16(dst, bg_pattern, LCD_WIDTH*LCD_HEIGHT);
        else
            memcpy(dst, lcd_backdrop, sizeof(lcd_framebuffer));
    }
}


/* Update the display.
   This must be called after all other LCD functions that change the display. */
void lcd_update(void)
{
    lcd_update_rect(0, 0, LCD_WIDTH, LCD_HEIGHT);
}

void lcd_bitmap_transparent_part(const fb_data *src, int src_x, int src_y,
                                 int stride, int x, int y, int width,
                                 int height)
{
    fb_data *dst, *dst_end;
    unsigned int transcolor;

    /* nothing to draw? */
    if ((width <= 0) || (height <= 0) || (x >= LCD_WIDTH) || (y >= LCD_HEIGHT)
        || (x + width <= 0) || (y + height <= 0))
        return;

    /* clipping */
    if (x < 0)
    {
        width += x;
        src_x -= x;
        x = 0;
    }
    if (y < 0)
    {
        height += y;
        src_y -= y;
        y = 0;
    }
    if (x + width > LCD_WIDTH)
        width = LCD_WIDTH - x;
    if (y + height > LCD_HEIGHT)
        height = LCD_HEIGHT - y;

    src += stride * src_y + src_x; /* move starting point */
    dst = &lcd_framebuffer[(y)][(x)];
    dst_end = dst + height * LCD_WIDTH;
    width *= 2;
    stride *= 2;
    transcolor = TRANSPARENT_COLOR;
    asm volatile(
    "rowstart:  \n"
        "mov    r0, #0  \n"
    "nextpixel:  \n"
        "ldrh   r1, [%0, r0]   \n"  /* Load word src+r0 */
        "cmp    r1, %5 \n"             /* Compare to transparent color */
        "strneh r1, [%1, r0]   \n"  /* Store dst+r0 if not transparent */
        "add    r0, r0, #2  \n"
        "cmp    r0, %2 \n"             /* r0 == width? */         
        "bne    nextpixel \n"        /* More in this row? */
        "add    %0, %0, %4  \n"     /* src += stride */
        "add    %1, %1, #480 \n"    /* dst += LCD_WIDTH (x2) */
        "cmp    %1, %3 \n"             
        "bne    rowstart \n"        /* if(dst != dst_end), keep going */
        : : "r" (src), "r" (dst), "r" (width), "r" (dst_end), "r" (stride), "r" (transcolor) : "r0", "r1" );
}

/* Line write helper function for lcd_yuv_blit. Write two lines of yuv420. */
extern void lcd_write_yuv420_lines(fb_data *dst,
                                   unsigned char chroma_buf[LCD_HEIGHT/2*3],
                                   unsigned char const * const src[3],
                                   int width,
                                   int stride);
/* Performance function to blit a YUV bitmap directly to the LCD */
/* For the Gigabeat - show it rotated */
/* So the LCD_WIDTH is now the height */
void lcd_yuv_blit(unsigned char * const src[3],
                  int src_x, int src_y, int stride,
                  int x, int y, int width, int height)
{
    /* Caches for chroma data so it only need be recaculated every other
       line */
    unsigned char chroma_buf[LCD_HEIGHT/2*3]; /* 480 bytes */
    unsigned char const * yuv_src[3];
    off_t z;

    if (!lcd_on)
        return;

    /* Sorry, but width and height must be >= 2 or else */
    width &= ~1;
    height >>= 1;

    fb_data *dst = (fb_data*)FRAME + x * LCD_WIDTH + (LCD_WIDTH - y) - 1;

    z = stride*src_y;
    yuv_src[0] = src[0] + z + src_x;
    yuv_src[1] = src[1] + (z >> 2) + (src_x >> 1);
    yuv_src[2] = src[2] + (yuv_src[1] - src[1]);

    do
    {
        lcd_write_yuv420_lines(dst, chroma_buf, yuv_src, width,
                               stride);
        yuv_src[0] += stride << 1; /* Skip down two luma lines */
        yuv_src[1] += stride >> 1; /* Skip down one chroma line */
        yuv_src[2] += stride >> 1;
        dst -= 2;
    }
    while (--height > 0);
}

void lcd_set_contrast(int val) {
  (void) val;
  // TODO:
}

void lcd_set_invert_display(bool yesno) {
  (void) yesno;
  // TODO:
}

void lcd_blit(const fb_data* data, int bx, int y, int bwidth,
              int height, int stride)
{
  (void) data;
  (void) bx;
  (void) y;
  (void) bwidth;
  (void) height;
  (void) stride;
  //TODO:
}

void lcd_set_flip(bool yesno) {
  (void) yesno;
  // TODO:
}

