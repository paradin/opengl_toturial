#include "sgl.h"
#include <time.h>
#include <stdio.h>
#include <stdlib.h>


double cal_fps()
{
	static int frame = 0;
	static double fps = 0;
	static clock_t last = 0, current = clock();
	double timegap = 0;

	if(frame++ <= 50)
		return fps;

	frame = 0;

	last = current;
	current = clock();
	timegap = (current - last)/(double)CLK_TCK;
	fps = 50.0/timegap;

	return fps;
}

void grab(GLint x, GLint y, GLsizei width, GLsizei height) {
	FILE *pDummyFile;
	FILE *pWritingFile;

	GLubyte *pPixelData;
	GLubyte BMP_Header[BMP_HEADER_LENGTH];
	GLint i, j;
	GLint PixelDataLength;

	// 每行字节个数，默认会按4的倍数补齐
	i = width * 3;
	//while( i%4 != 0)
	//	++i;
	//i += (BMP_LINE_ALIGNMENT - i%BMP_LINE_ALIGNMENT);
	ALIGNLINESIZE(i);

	// 图像像素字节数
	PixelDataLength = i * height;

	// 分配内存和打开文件
	pPixelData = (GLubyte *)malloc(PixelDataLength);
	if(pPixelData == 0)
		return;

	errno_t err1 = fopen_s(&pDummyFile, "dummy.bmp", "rb");
	if(err1 != 0)
		return;

	errno_t err2 = fopen_s(&pWritingFile, "grab.bmp", "wb");
	if(err2 != 0)
		return;

	// 设置读取文件对齐字节
	glPixelStorei(GL_UNPACK_ALIGNMENT, BMP_LINE_ALIGNMENT);
	// 读取像素
	glReadPixels(x, y, width, height,
		GL_BGR_EXT, GL_UNSIGNED_BYTE, pPixelData);

	// 把dummy.bmp的文件头复制为新文件的文件头
	fread(BMP_Header, sizeof(BMP_Header), 1, pDummyFile);
	fwrite(BMP_Header, sizeof(BMP_Header), 1, pWritingFile);
	fseek(pWritingFile, 0x0012, SEEK_SET);
	i = width;
	j = height;
	fwrite(&i, sizeof(i), 1, pWritingFile);
	fwrite(&j, sizeof(j), 1, pWritingFile);

	fseek(pWritingFile, 0, SEEK_END);
	fwrite(pPixelData, PixelDataLength, 1, pWritingFile);

	fclose(pDummyFile);
	fclose(pWritingFile);
	free(pPixelData);
}

/* 函数power_of_two
* 检查一个整数是否为2的整数次方，如果是，返回1，否则返回0
* 实际上只要查看其二进制位中有多少个，如果正好有1个，返回1，否则返回0
* 在“查看其二进制位中有多少个”时使用了一个小技巧
* 使用n &= (n-1)可以使得n中的减少一个（具体原理大家可以自己思考）
*/
int power_of_two(int n)
{
	if( n <= 0 )
		return 0;
	return (n & (n-1)) == 0;
}

/* 函数load_texture
* 读取一个BMP文件作为纹理
* 如果失败，返回0，如果成功，返回纹理编号
*/
GLuint load_texture(const char* file_name)
{
	GLint width, height, total_bytes;
	GLubyte* pixels = 0;
	GLuint last_texture_ID, texture_ID = 0;

	// 打开文件，如果失败，返回
	FILE* pFile;
	errno_t err1 = fopen_s(&pFile, file_name, "rb");
	if( err1 != 0 )
		return 0;

	// 读取文件中图象的宽度和高度
	fseek(pFile, 0x0012, SEEK_SET);
	fread(&width, sizeof(width), 1, pFile);
	fread(&height, sizeof(height), 1, pFile);
	fseek(pFile, BMP_HEADER_LENGTH, SEEK_SET);

	// 计算每行像素所占字节数，并根据此数据计算总像素字节数
	{
		GLint line_bytes = width * 3;
		//while( line_bytes % 4 != 0 )
		//	++line_bytes;
		//line_bytes += (BMP_LINE_ALIGNMENT-line_bytes%BMP_LINE_ALIGNMENT);
		ALIGNLINESIZE(line_bytes);
		total_bytes = line_bytes * height;
	}

	// 根据总像素字节数分配内存
	pixels = (GLubyte*)malloc(total_bytes);
	if( pixels == 0 )
	{
		fclose(pFile);
		return 0;
	}

	// 读取像素数据
	if( fread(pixels, total_bytes, 1, pFile) <= 0 )
	{
		free(pixels);
		fclose(pFile);
		return 0;
	}

	// 在旧版本的OpenGL中
	// 如果图象的宽度和高度不是的整数次方，则需要进行缩放
	// 这里并没有检查OpenGL版本，出于对版本兼容性的考虑，按旧版本处理
	// 另外，无论是旧版本还是新版本，
	// 当图象的宽度和高度超过当前OpenGL实现所支持的最大值时，也要进行缩放
	{
		GLint max;
		glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max);
		if( !power_of_two(width)
			|| !power_of_two(height)
			|| width > max
			|| height > max )
		{
			const GLint new_width = 256;
			const GLint new_height = 256; // 规定缩放后新的大小为边长的正方形
			GLint new_line_bytes, new_total_bytes;
			GLubyte* new_pixels = 0;

			// 计算每行需要的字节数和总字节数
			new_line_bytes = new_width * 3;
			//while( new_line_bytes % 4 != 0 )
			//	++new_line_bytes;
			//new_line_bytes += (4-new_line_bytes%4);
			ALIGNLINESIZE(new_line_bytes);
			new_total_bytes = new_line_bytes * new_height;

			// 分配内存
			new_pixels = (GLubyte*)malloc(new_total_bytes);
			if( new_pixels == 0 )
			{
				free(pixels);
				fclose(pFile);
				return 0;
			}

			// 进行像素缩放
			gluScaleImage(GL_RGB,
				width, height, GL_UNSIGNED_BYTE, pixels,
				new_width, new_height, GL_UNSIGNED_BYTE, new_pixels);

			// 释放原来的像素数据，把pixels指向新的像素数据，并重新设置width和height
			free(pixels);
			pixels = new_pixels;
			width = new_width;
			height = new_height;
		}
	}

	// 分配一个新的纹理编号
	glGenTextures(1, &texture_ID);
	if( texture_ID == 0 )
	{
		free(pixels);
		fclose(pFile);
		return 0;
	}

	// 绑定新的纹理，载入纹理并设置纹理参数
	// 在绑定前，先获得原来绑定的纹理编号，以便在最后进行恢复
	glGetIntegerv(GL_TEXTURE_BINDING_2D, (GLint*)&last_texture_ID);
	glBindTexture(GL_TEXTURE_2D, texture_ID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0,
		GL_BGR_EXT, GL_UNSIGNED_BYTE, pixels);
	glBindTexture(GL_TEXTURE_2D, last_texture_ID);

	// 之前为pixels分配的内存可在使用glTexImage2D以后释放
	// 因为此时像素数据已经被OpenGL另行保存了一份（可能被保存到专门的图形硬件中）
	free(pixels);
	return texture_ID;
}