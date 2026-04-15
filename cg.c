#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <zlib.h>
#include <math.h>
typedef unsigned char byte;
typedef unsigned int uint;

/**
 * @brief Prints an error message and stops execution
 * 
 * @param msg 
 */
void error_exit(char *msg)
{
    perror(msg);
    exit(1);
}

/*
 * CRC code
 * https://www.libpng.org/pub/png/spec/1.2/PNG-CRCAppendix.html
 */

/* Table of CRCs of all 8-bit messages. */
unsigned int crc_table[256];

/* Flag: has the table been computed? Initially false. */
int crc_table_computed = 0;

/* Make the table for a fast CRC. */
void make_crc_table(void)
{
    unsigned int c;
    int n, k;

    for (n = 0; n < 256; n++)
    {
        c = (unsigned int)n;
        for (k = 0; k < 8; k++)
        {
            if (c & 1)
                c = 0xedb88320L ^ (c >> 1);
            else
                c = c >> 1;
        }
        crc_table[n] = c;
    }
    crc_table_computed = 1;
}

/* Update a running CRC with the bytes buf[0..len-1]--the CRC
   should be initialized to all 1's, and the transmitted value
   is the 1's complement of the final running CRC (see the
   crc() routine below)). */
unsigned int update_crc(unsigned int crc, unsigned char *buf, int len)
{
    unsigned int c = crc;
    int n;

    if (!crc_table_computed)
        make_crc_table();
    for (n = 0; n < len; n++)
    {
        c = crc_table[(c ^ buf[n]) & 0xff] ^ (c >> 8);
    }
    return c;
}

/* Return the CRC of the bytes buf[0..len-1]. */
unsigned int crc(unsigned char *buf, int len)
{
    return update_crc(0xffffffff, buf, len) ^ 0xffffffff;
}

/**
 * End of CRC code
 */



/**
 * Utils
 */

/**
 * @brief Flips the bytes of a word
 * 
 * @param x 
 * @return x in big endian order 
 */
unsigned int big_endian(unsigned int x)
{
    return (x << 24 & 0xff000000) | (x << 8 & 0xff0000) | (x >> 8 & 0xff00) | (x >> 24 & 0xff);
}

/**
 * @brief
 * Converts RGB values to HSV
 * 
 * https://mattlockyer.github.io/iat455/documents/rgb-hsv.pdf
 * 
 * @param rgb Input RGB values
 * @param hsv Output HSV values
 */
void rgb2hsv(const byte rgb[3], byte hsv[3])
{
    double R = rgb[0] / 255.0; double G = rgb[1] / 255.0; double B = rgb[2] / 255.0;

    double max = R; double min = R;
    if (G > max) max = G; else if (G < min) min = G;
    if (B > max) max = B; else if (B < min) min = B;

    double d = max - min;

    double H;
    if (d == 0.0) H = 0.0;
    else if (max == R) H = (G - B) / d;
    else if (max == G) H = (B - R) / d + 2;
    else if (max == B) H = (R - G) / d + 4;
    while (H < 0) H += 6;
    while (H >= 6) H -= 6;
    
    hsv[0] = (byte)(H / 6.0 * 256.0); // [0,256)

    double V = max;
    hsv[2] = (byte)(V * 255.0);

    double S = 0.0;
    if (V != 0.0) S = d / V;
    hsv[1] = (byte)(S * 255.0);
}

/**
 * @brief
 * Converts HSV values to RGB
 * 
 * https://mattlockyer.github.io/iat455/documents/rgb-hsv.pdf
 * 
 * @param rgb Input HSV values
 * @param hsv Output RGB values
 */
void hsv2rgb(const byte hsv[3], byte rgb[3])
{
    double H = hsv[0] / 255.0 * 6.0;
    double S = hsv[1] / 255.0;
    double V = hsv[2] / 255.0;

    double a = V * (1 - S);
    double b = V * (1 - (H - (byte)H) * S);
    double c = V * (1 - (1 - (H - (byte)H)) * S);

    double R, G, B;
    if (H < 1.0)      R = V, G = c, B = a;
    else if (H < 2.0) R = b, G = V, B = a;
    else if (H < 3.0) R = a, G = V, B = c;
    else if (H < 4.0) R = a, G = b, B = V;
    else if (H < 5.0) R = c, G = a, B = V;
    else              R = V, G = a, B = b;

    rgb[0] = (byte)(255.0 * R); 
    rgb[1] = (byte)(255.0 * G); 
    rgb[2] = (byte)(255.0 * B); 
}

int max(int a, int b)
{
    if (a > b) return a;
    return b;
}

uint umax(uint a, uint b)
{
    if (a > b) return a;
    return b;
}

int min(int a, int b)
{
    if (a < b) return a;
    return b;
}

uint umin(uint a, uint b)
{
    if (a < b) return a;
    return b;
}

typedef struct
{
    double position;
    byte color[3];
} ColorGradientNode;

void colorGradient(ColorGradientNode gradient[], int gradientNodeCount, double position, byte out[3])
{
    //printf("Checking position %5.1lf\n", position);

    if (position <= gradient[0].position)
    {
        out[0] = gradient[0].color[0];
        out[1] = gradient[0].color[1];
        out[2] = gradient[0].color[2];
        //printf("  Position too small! lol!\n");
        return;
    }

    for (int i = 0; i < gradientNodeCount - 1; i++)
    {
        if (gradient[i+1].position <= gradient[i].position) error_exit("Invalid gradient positions");

        if (gradient[i].position <= position && position <= gradient[i+1].position)
        {
            double progress = (position - gradient[i].position) / (gradient[i+1].position - gradient[i].position);
            out[0] = (byte)((1. - progress) * gradient[i].color[0] + progress * gradient[i+1].color[0]);
            out[1] = (byte)((1. - progress) * gradient[i].color[1] + progress * gradient[i+1].color[1]);
            out[2] = (byte)((1. - progress) * gradient[i].color[2] + progress * gradient[i+1].color[2]);
            //printf("  Found my place at i = %d, progress = %3.2f\n", i, progress);
            return;
        }
    }

    out[0] = gradient[gradientNodeCount-1].color[0];
    out[1] = gradient[gradientNodeCount-1].color[1];
    out[2] = gradient[gradientNodeCount-1].color[2]; 
    //printf("  Position too big! lol!\n");
}

/**
 * End of utils
 */



/**
 * Colorgrades
 * 
 * Each takes 3 bytes (RGB) in in as input, and puts 3 bytes (RGB) in out as output
 */

void identity(const byte in[3], byte out[3])
{
    out[0] = in[0]; out[1] = in[1]; out[2] = in[2];
}

void bluegreen(const byte in[3], byte out[3])
{
    out[0] = in[0];
    if (in[2] > in[1])
    {
        out[1] = in[2];
        out[2] = in[1];
    }
    else
    {
        out[1] = in[1];
        out[2] = in[2];
    }
}

void greenkillblue(const byte in[3], byte out[3])
{
    out[0] = in[0];
    out[1] = in[1];
    if (in[2] >= in[1]) out[2] = in[2] - in[1];
    else out[2] = 0;
}

void greenhurtbluesavedred(const byte in[3], byte out[3])
{
    out[0] = in[0];
    out[1] = in[1];
    int ouch =  in[0] / 2 - in[1];
    if (ouch >= 0) out[2] = in[2];
    else if (in[2] + ouch >= 0) out[2] = in[2] + ouch;
    else out[2] = 0;
}

void cyanintervalgreen(const byte in[3], byte out[3])
{
    byte hsv[3];
    rgb2hsv(in, hsv);
    if (128 <= hsv[0] && hsv[0] <= 153) hsv[0] -= 55;
    hsv2rgb(hsv, out);
}

void cyanintervalgreen2(const byte in[3], byte out[3])
{
    byte hsv[3];
    rgb2hsv(in, hsv);
    if (106 <= hsv[0] && hsv[0] <= 153) hsv[0] -= 55;
    hsv2rgb(hsv, out);
}

void cyanintervalgreen3(const byte in[3], byte out[3])
{
    byte hsv[3];
    rgb2hsv(in, hsv);
    if (106 <= hsv[0] && hsv[0] <= 180) hsv[0] -= 55;
    hsv2rgb(hsv, out);
}

void piss(const byte in[3], byte out[3])
{
    byte hsv[3];
    rgb2hsv(in, hsv);
    hsv[0] = 43;
    hsv2rgb(hsv, out);
}

void pisser(const byte in[3], byte out[3])
{
    byte hsv[3];
    rgb2hsv(in, hsv);
    hsv[0] = 43;
    hsv[1] = 255 - ((int)(255 - hsv[1]) * (int)(255 - hsv[1]) / 255);
    hsv2rgb(hsv, out);
}

void chess(const byte in[3], byte out[3])
{
    out[0] = out[1] = out[2] = 255 * ((in[0] / 17 + in[1] / 17 + in[2] / 17) % 2);
}

void randomshit(const byte in[3], byte out[3])
{
    srand((int)in[0] + (int)(in[1] << 8) + (int)(in[2] << 16));
    out[0] = rand() % 255;
    out[1] = rand() % 255;
    out[2] = rand() % 255;
}

void brownbox(const byte in[3], byte out[3])
{
    if (in[2] == 8*17)
    {
        out[0] = in[0];
        out[1] = in[1];
        out[2] = in[2];
    }
    else
    {
        out[0] = 89;
        out[1] = 56;
        out[2] = 13;
    }
}

void neon(const byte in[3], byte out[3])
{
    byte hsv[3];
    rgb2hsv(in, hsv);
    hsv[1] = (int)round(255 * sqrt(hsv[1] / 255.0));
    hsv[2] = (int)round(255 * sqrt(hsv[2] / 255.0));
    hsv2rgb(hsv, out);
}

void astralizeoverflow(const byte in[3], byte out[3])
{
    out[0] = in[1]+in[2];
    out[1] = in[2]+in[0];
    out[2] = in[0]+in[1];
}

void astralize(const byte in[3], byte out[3])
{
    out[0] = umin((uint)in[1]+(uint)in[2],255u);
    out[1] = umin((uint)in[2]+(uint)in[0],255u);
    out[2] = umin((uint)in[0]+(uint)in[1],255u);
}

void astralizereduced(const byte in[3], byte out[3])
{
    out[0] = ((uint)in[1]+(uint)in[2]) / 2;
    out[1] = ((uint)in[2]+(uint)in[0]) / 2;
    out[2] = ((uint)in[0]+(uint)in[1]) / 2;
}

void protanopia(const byte in[3], byte out[3])
{
    byte hsvin[3];
    rgb2hsv(in, hsvin);

    ColorGradientNode gradient[7] = {
        {.position=  0.0, .color={191, 178,   3}},
        {.position= 42.7, .color={247, 254, 116}},
        {.position= 85.3, .color={233, 239,  69}},
        {.position=128.0, .color={218, 232, 207}},
        {.position=170.7, .color={ 32,  71, 211}},
        {.position=213.3, .color={ 80,  96, 243}},
        {.position=256.0, .color={191, 178,   3}},
    };

    colorGradient(gradient, 7, (double)hsvin[0], out);

    byte hsvout[3];
    rgb2hsv(out, hsvout);
    //hsvout[1] = hsvin[1];
    hsvout[2] = (byte)(255*sqrt(hsvin[2] / 255.));
    hsv2rgb(hsvout, out);
}

void variedgreens(const byte in[3], byte out[3])
{
    byte hsv[3];
    rgb2hsv(in, hsv);
    double greenRange = 15.0;
    hsv[0] = (byte)(85. + greenRange*sin(2.*3.141592653589793/256.*(hsv[0] - 85.)));
    hsv2rgb(hsv, out);
}

/**
 * End of Colorgrades
 */



int main(int argc, char *argv[])
{
    int fd = open("colorgrade.png", O_CREAT | O_TRUNC | O_WRONLY, 0644);

    // PNG bulllllllshitttttttt!
    // https://www.libpng.org/pub/png/spec/1.2/PNG-Contents.html

    write(fd, "\211PNG\r\n\032\n", 8);  // PNG identifier

    write(fd, "\0\0\0\015IHDR", 8);     // 13 bytes, IHDR
    write(fd, "\0\0\1\0\0\0\0\020", 8);  // 256 width x 16 height
    write(fd, "\b\2\0\0\0", 5);         // 8 bits, RGB, no compression, no filtering, no interlace
    
    unsigned int ihdr_crc = big_endian(crc("IHDR\0\0\1\0\0\0\0\020\b\2\0\0\0", 17));    
    write(fd, &ihdr_crc, 4);            // CRC of IHDR

    byte buffer[32768];
    byte * buf_p = buffer;
    int buf_size = 0;

    for (int g = 0; g < 256; g += 17) 
    {
        *buf_p = '\0'; // Filter byte
        buf_p++;
        buf_size++;
        for (int b = 0; b < 256; b += 17) for (int r = 0; r < 256; r += 17)
        {
            byte rgb_in[3] = {r, g, b};
            byte rgb_out[3];

            /// Call here your colorgrade functions!
            /// Only one at a time though
            /// Then compile the code by typing "make" into the terminal (without the "")
            /// Although if you're reading this then I kinda assume you know how this stuff works
            
            //identity(rgb_in, rgb_out);
            //bluegreen(rgb_in, rgb_out);
            //greenkillblue(rgb_in, rgb_out);
            //greenhurtbluesavedred(rgb_in, rgb_out);
            //cyanintervalgreen(rgb_in, rgb_out);
            //cyanintervalgreen2(rgb_in, rgb_out);
            //cyanintervalgreen3(rgb_in, rgb_out);
            //piss(rgb_in, rgb_out);
            //pisser(rgb_in, rgb_out);
            //chess(rgb_in, rgb_out);
            //randomshit(rgb_in, rgb_out);
            //brownbox(rgb_in, rgb_out);
            //neon(rgb_in, rgb_out);
            //astralizeoverflow(rgb_in, rgb_out);
            //astralize(rgb_in, rgb_out);
            //astralizereduced(rgb_in, rgb_out);
            //protanopia(rgb_in, rgb_out);
            variedgreens(rgb_in, rgb_out);
            
            buf_p[0] = rgb_out[0]; buf_p[1] = rgb_out[1]; buf_p[2] = rgb_out[2];
            buf_p += 3;
            buf_size += 3;    
        }
    }

    byte buffer2[32768];
    unsigned long buf2_size = sizeof(buffer2); 

    int compress2return = compress2(buffer2+4, &buf2_size, buffer, buf_size, 0);
    if (compress2return != Z_OK)
    {
        switch (compress2return)
        {
        case Z_BUF_ERROR:
            error_exit("compress buffer error");
        case Z_MEM_ERROR:
            error_exit("compress memory error");
        default:
            error_exit("compress stream error");
        }
    }
        
    buffer2[0] = 'I'; buffer2[1] = 'D'; buffer2[2] = 'A'; buffer2[3] = 'T';
    
    int data_size = big_endian(buf2_size);
    write(fd, &data_size, 4);

    write(fd, buffer2, buf2_size+4);
    unsigned int data_crc = big_endian(crc(buffer2, buf2_size+4));
    write(fd, &data_crc, 4);

    write(fd, "\0\0\0\0IEND", 8);
    unsigned int iend_crc = big_endian(crc("IEND", 4));
    write(fd, &iend_crc, 4);
}