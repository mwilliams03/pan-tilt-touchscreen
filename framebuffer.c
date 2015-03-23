/*
        A simple program that demonstrates how to use a touchscreen to
        control the pan and tilt of a camera. This is specifically on
        the Raspberry Pi, however this program can be used for most
        Linux based systems.
        For more details: ozzmaker.com

    Copyright (C) 2015  Mark Williams

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public
    License along with this library; if not, write to the Free
    Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
    MA 02111-1307, USA
*/
#include <stdlib.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include "font_8x8.c"
#include <stdint.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <bcm_host.h>
#include <syslog.h>


#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"

DISPMANX_DISPLAY_HANDLE_T display;
DISPMANX_MODEINFO_T display_info;
DISPMANX_RESOURCE_HANDLE_T screen_resource;
VC_IMAGE_TRANSFORM_T transform;
uint32_t image_prt;
VC_RECT_T rect1;
int ret;
int fbfd = 0;
char *fbcpfbp = 0;

struct fb_var_screeninfo vinfo;
struct fb_fix_screeninfo finfo;
static struct fb_fix_screeninfo fix;
static struct fb_var_screeninfo orig_var;
static struct fb_var_screeninfo var;
char *fbp ;
char *backBufferP = 0;
int fb=0;
long int screensize = 0;



// default framebuffer palette
typedef enum {
  BLACK        =  0, /*   0,   0,   0 */
  BLUE         =  1, /*   0,   0, 172 */
  GREEN        =  2, /*   0, 172,   0 */
  CYAN         =  3, /*   0, 172, 172 */
  RED          =  4, /* 172,   0,   0 */
  PURPLE       =  5, /* 172,   0, 172 */
  ORANGE       =  6, /* 172,  84,   0 */
  LTGREY       =  7, /* 172, 172, 172 */
  GREY         =  8, /*  84,  84,  84 */
  LIGHT_BLUE   =  9, /*  84,  84, 255 */
  LIGHT_GREEN  = 10, /*  84, 255,  84 */
  LIGHT_CYAN   = 11, /*  84, 255, 255 */
  LIGHT_RED    = 12, /* 255,  84,  84 */
  LIGHT_PURPLE = 13, /* 255,  84, 255 */
  YELLOW       = 14, /* 255, 255,  84 */
  WHITE        = 15  /* 255, 255, 255 */
} COLOR_INDEX_T;
static unsigned short def_r[] =
    { 0,   0,   0,   0, 172, 172, 172, 168,
     84,  84,  84,  84, 255, 255, 255, 255};
static unsigned short def_g[] =
    { 0,   0, 168, 168,   0,   0,  84, 168,
     84,  84, 255, 255,  84,  84, 255, 255};
static unsigned short def_b[] =
    { 0, 172,   0, 168,   0, 172,   0, 168,
     84, 255,  84, 255,  84, 255,  84, 255};



void put_pixel_16bpp(int x, int y, int r, int g, int b)
{
        unsigned int pix_offset;
        unsigned short c;
        // calculate the pixel's byte offset inside the buffer
        pix_offset = x*2 + y * fix.line_length;

        //some magic to work out the color
        c = ((r / 8) << 11) + ((g / 4) << 5) + (b / 8);

        // write 'two bytes at once'
        *((unsigned short*)(backBufferP + pix_offset)) = c;
}


void drawSquare(int x, int y,int height, int width,  int c)
{
	int h = 0;
	int w = 0;
	for ( h = 0; h< height;h++)
		for ( w = 0; w< width;w++)
			put_pixel_16bpp( h+(x-2), w+(y-2) , def_r[c],def_g[c],def_b[c]);

}


int framebufferInitialize(int *xres, int *yres)
{
	char *fbdevice = "/dev/fb1" ;

	fb = open(fbdevice, O_RDWR );
	if (fb == -1) {
		perror("open fbdevice");
	return -1;
	}

	if (ioctl(fb, FBIOGET_FSCREENINFO, &fix) < 0) {
		perror("ioctl FBIOGET_FSCREENINFO");
		close(fb);
	return -1;
	}

	if (ioctl(fb, FBIOGET_VSCREENINFO, &var) < 0) {
		perror("ioctl FBIOGET_VSCREENINFO");
	close(fb);
	return -1;
	}

	printf("Original %dx%d, %dbpp\n", var.xres, var.yres, 
         var.bits_per_pixel );

	memcpy(&orig_var, &var, sizeof(struct fb_var_screeninfo));



	printf("Framebuffer %s%s%s resolution;\n",KYEL, fbdevice, KWHT);

  	printf("%dx%d, %d bpp\n\n\n", var.xres, var.yres, var.bits_per_pixel );

	// map framebuffer to user memory
	screensize = fix.smem_len;
	fbp = (char*)mmap(0,
                    screensize,
                    PROT_READ | PROT_WRITE,
                    MAP_SHARED,
                    fb, 0);

	if ((int)fbp == -1) {
	printf("Failed to mmap.\n");
	}
	//This is our double buffer space
	backBufferP=(char *)malloc(screensize*sizeof(uint16_t ));

	*xres = var.xres;
	*yres = var.yres;

	//clear framebuffer
        int x, y;
        for (x = 0; x<var.xres;x++)
                for (y = 0; y < var.yres;y++)
                        put_pixel_16bpp(x,y, 0, 0, 0);
	return 0;
}

void closeFramebuffer()
{
	int x, y;
	for (x = 0; x < var.xres; x++)
		for (y = 0; y < var.yres;y++)
			put_pixel_16bpp(x,y, 0, 0, 0);

	munmap(fbp, screensize);
        free(backBufferP);


	if (ioctl(fb, FBIOPUT_VSCREENINFO, &orig_var)) {
		printf("Error re-setting variable information.\n");
	}
	close(fb);

}
void put_char(int x, int y, int c, int colidx)
{
	int i,j,bits;
	for (i = 0; i < font_vga_8x8.height; i++) {
	bits = font_vga_8x8.data [font_vga_8x8.height * c + i];
		for (j = 0; j < font_vga_8x8.width; j++, bits <<= 1)
			if (bits & 0x80){
				put_pixel_16bpp(x+j,  y+i, 255,255,255);
			}
		}
}

void put_string(int x, int y, char *s, unsigned colidx)
{
	int i;
	for (i = 0; *s; i++, x += font_vga_8x8.width, s++)
	put_char (x, y, *s, colidx);
}








int initFbcp() {

    bcm_host_init();

    display = vc_dispmanx_display_open(0);
    if (!display) {
        syslog(LOG_ERR, "Unable to open primary display");
        return -1;
    }
    ret = vc_dispmanx_display_get_info(display, &display_info);
    if (ret) {
        syslog(LOG_ERR, "Unable to get primary display information");
        return -1;
    }
    syslog(LOG_INFO, "Primary display is %d x %d", display_info.width, display_info.height);


    if (ioctl(fb, FBIOGET_FSCREENINFO, &finfo)) {
        syslog(LOG_ERR, "Unable to get secondary display information");
        return -1;
    }
    if (ioctl(fb, FBIOGET_VSCREENINFO, &vinfo)) {
        syslog(LOG_ERR, "Unable to get secondary display information");
        return -1;
    }

    syslog(LOG_INFO, "Second display is %d x %d %dbps\n", vinfo.xres, vinfo.yres, vinfo.bits_per_pixel);


    screen_resource = vc_dispmanx_resource_create(VC_IMAGE_RGB565, vinfo.xres, vinfo.yres, &image_prt);
    printf(" vinfo.xres %i   vinfo.yres   %i\n", var.xres,var.yres);
    if (!screen_resource) {
        syslog(LOG_ERR, "Unable to create screen buffer");
        close(fbfd);
        vc_dispmanx_display_close(display);
        return -1;
    }



   fbcpfbp = (char*) mmap(0, finfo.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
    if (fbcpfbp <= 0) {
        syslog(LOG_ERR, "Unable to create mamory mapping");
        close(fbfd);
        ret = vc_dispmanx_resource_delete(screen_resource);
        vc_dispmanx_display_close(display);
        return -1;
    }

    vc_dispmanx_rect_set(&rect1, 0, 0, vinfo.xres, vinfo.yres);
    return 0;
}
void fbcp()
{

        ret = vc_dispmanx_snapshot(display, screen_resource, 0);

        vc_dispmanx_resource_read_data(screen_resource, &rect1, backBufferP, vinfo.xres * vinfo.bits_per_pixel / 8);

}
void closeFbcp()
{
    munmap(fbcpfbp, finfo.smem_len);
    close(fbfd);
    ret = vc_dispmanx_resource_delete(screen_resource);
    vc_dispmanx_display_close(display);

}


