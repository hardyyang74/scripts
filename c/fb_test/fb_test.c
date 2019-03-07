
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/mman.h>

typedef struct _BMP_HEADER
{
	unsigned short reserved0;
	unsigned short identifier;
	unsigned int file_size;
	unsigned int reserved1;
	unsigned int data_offset;
	unsigned int header_size;
	unsigned int width;
	unsigned int height;
	unsigned short planes;
	unsigned short bpp;
	unsigned int compression;
	unsigned short reserved;
} BMP_HEADER;



void LoadImage(struct fb_var_screeninfo *var, char *name, void* InBuf)
{
	FILE *fin = (FILE*)fopen(name, "rb");
	BMP_HEADER bmp;
	unsigned char* tmpbuf = NULL;
	unsigned short* ptr16;
	unsigned int* ptr32;
	unsigned char* ptr8;
	int bpl;
	int x, y;
	int copy_w, copy_h;
	int ret;



	if(fin == NULL)
	{
		printf("can not find %s\n",name);
		return;
	}

	// read bmp header
	tmpbuf = (unsigned char*)&bmp.identifier;
	ret = fread(tmpbuf, 1, sizeof(BMP_HEADER), fin);
	printf("Image info: size %d*%d, bpp %d, compression %d, offset %d\n", bmp.width, bmp.height, bmp.bpp, bmp.compression, bmp.data_offset);
	fclose(fin);
	if((bmp.bpp!=24) || bmp.compression)
	{
		printf("not supported bmp file\n");
		return;
	}

	// allocate buffer
	tmpbuf = malloc(bmp.width*bmp.height*bmp.bpp/8+1024);

	if(tmpbuf == NULL)
	{
		printf("malloc faild\n");
		return;
	}

	// read bmp data, only support rgb888 mode
	fin = fopen(name, "rb");
	if(fin == NULL)
	{
		printf("open bmp file faild\n");
	}

	fseek(fin, bmp.data_offset, SEEK_SET);
	ret = fread(tmpbuf, 1, bmp.width*bmp.height*bmp.bpp/8+1024, fin);
	fclose(fin);


	if(var->xres <= bmp.width)
		copy_w = var->xres;
	else
		copy_w = bmp.width;

	if(var->yres <= bmp.height)
		copy_h = var->yres;
	else
		copy_h = bmp.height;

	// copy image into framebuffer
	switch(var->bits_per_pixel)
	{
	case 32:
		if (InBuf)
			ptr8 = (unsigned char*) InBuf;
		else
			printf("InBuf NULL\n");
		
		bpl = bmp.width*bmp.bpp/8;

		for(y=0;y<copy_h;y++)
		{
			for(x=0;x<copy_w;x++)
			{

				ptr8[((var->yres-y-1) * var->xres + x)*4] = tmpbuf[y*bpl+x*3];
				ptr8[((var->yres-y-1) * var->xres + x)*4+1] = tmpbuf[y*bpl+x*3+1];
				ptr8[((var->yres-y-1) * var->xres + x)*4+2] = tmpbuf[y*bpl+x*3+2];
			}
		}

		break;
		
	case 24:
		break;




	default:
		break;
		
	}




}




int main ()
{

	int fp=0;

	struct fb_var_screeninfo vinfo;

	struct fb_fix_screeninfo finfo;

	long screensize=0;

	char *fbp = 0;


	int x = 0, y = 0,k;
	unsigned char red,green,blue;
	
	long location = 0;

	fp = open ("/dev/fb0",O_RDWR);

	if (fp < 0)
	{


		printf("Error : Can not open framebuffer device/n");

		exit(1);

	}

	if(ioctl(fp,FBIOGET_FSCREENINFO,&finfo))
	{

		printf("Error reading fixed information/n");

		exit(2);

	}

	if(ioctl(fp,FBIOGET_VSCREENINFO,&vinfo))
	{

		printf("Error reading variable information/n");

		exit(3);

	}


	screensize = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel /8;

	printf("------------xres:%d,yres:%d,bits_per_pixel:%d\n",vinfo.xres,vinfo.yres,vinfo.bits_per_pixel);
	printf("------------R:offset:%d,length:%d,msb_right:%d\n",vinfo.red.offset,vinfo.red.length,vinfo.red.msb_right);
	printf("------------G:offset:%d,length:%d,msb_right:%d\n",vinfo.green.offset,vinfo.green.length,vinfo.green.msb_right);
	printf("------------B:offset:%d,length:%d,msb_right:%d\n",vinfo.blue.offset,vinfo.blue.length,vinfo.blue.msb_right);
	

	fbp =(char *) mmap (0, screensize, PROT_READ | PROT_WRITE,MAP_SHARED, fp,0);

	if ((int) fbp == -1)
	{

		printf ("Error: failed to map framebuffer device tomemory./n");

		exit (4);

	}

#if 1
	//LoadImage(&vinfo,"test.bmp",fbp);

	for(k=0;k<10;k++)
	{

		for(x=vinfo.xres*k/10;x<vinfo.xres*(k+1)/10;x++)
		{

			for(y=0;y<vinfo.yres;y++)
			{
				location = x * (vinfo.bits_per_pixel / 8) + y *finfo.line_length;

				red = k*255/10;
				green =k*255/10;
				blue = k*255/10;

				red = red>>3;
				green = green>>2;
				blue = blue>>3;
					
				*((unsigned short *)(fbp + location)) = (red << 11) + (green << 5) + (blue << 0);
				
				//*(fbp + location + 1) = 0xe0;

			}
			
		}
	}


#else


		//red
		for(y=0;y<vinfo.yres/3;y++)
		{

			for(x=0;x<vinfo.xres;x++)
			{
				location = x * (vinfo.bits_per_pixel / 8) + y *finfo.line_length;
				
				*(fbp + location) = 0xf8;
				
				*(fbp + location + 1) = 0;
				
				//*(fbp + location + 2) = 0;
				
				//*(fbp + location + 3) = 0;
			}

		}
		
		//b
		for(y=vinfo.yres/3;y<vinfo.yres/3*2;y++)
		{

			for(x=0;x<vinfo.xres;x++)
			{

				location = x * (vinfo.bits_per_pixel / 8) + y *finfo.line_length;
				
				*(fbp + location) = 0;
				
				*(fbp + location + 1) = 0x1f;
				
				//*(fbp + location + 2) = 0;
				
				//*(fbp + location + 3) = 0;
			}

		}


		//g
		for(y=vinfo.yres/3*2;y<vinfo.yres;y++)
		{
		
			for(x=0;x<vinfo.xres;x++)
			{

				location = x * (vinfo.bits_per_pixel / 8) + y *finfo.line_length;
				
				*(fbp + location) = 0x7;
				
				*(fbp + location + 1) = 0xe0;
				
				//*(fbp + location + 2) = 255;
				
				//*(fbp + location + 3) = 0;
			}

		}



		

						
	


#endif

	munmap(fbp, screensize);

	close (fp);

	return 0;

}


