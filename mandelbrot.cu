/* CUDA code goes  here */
#include <cuda.h>

__global__ void mandel_float(uchar4 *ptr, int w, int h, float cx, float cy, float zoom, int iter_mult)
{
    int x = threadIdx.x + blockIdx.x * blockDim.x;
    int y = threadIdx.y + blockIdx.y * blockDim.y;
    int offset = x + y * blockDim.x * gridDim.x;

    if(x < w && y < h) {
#if 0
        uchar4 bgra = {0x0,0x0,0x0,0x0};
        bgra.y = (unsigned char)(t + 255.99f*y/(float)h);
        bgra.z = (unsigned char)(t + 255.99f*x/(float)w);
        *(ptr + offset) = bgra;
#else
        uchar4 bgra = {0x0,0x0,0x0,0x0};  // inside = black
        unsigned int MaxIterations = iter_mult*256;
        float ImageWidth = w;
        float ImageHeight = h;
        float MinRe = cx - 1.0f/zoom;
        float MaxRe = cx + 1.0f/zoom;
        float MinIm = cy - 1.0f/zoom;
        float MaxIm = MinIm+(MaxRe-MinRe)*ImageHeight/ImageWidth;
        float Re_factor = (MaxRe-MinRe)/(ImageWidth-1);
        float Im_factor = (MaxIm-MinIm)/(ImageHeight-1);

        float c_im = MinIm + y*Im_factor;
        float c_re = MinRe + x*Re_factor;

        float Z_re = c_re, Z_im = c_im;
        //bool isInside = true;
        for(unsigned int n=0; n<MaxIterations; ++n) {
            float Z_re2 = Z_re*Z_re, Z_im2 = Z_im*Z_im;
            if(Z_re2 + Z_im2 > 4.0f) {
                int nn = n & 0xFF;
                // outside the set, set color
                bgra.x = unsigned(char(nn));
                bgra.y = unsigned(char(nn));
                bgra.z = unsigned(char(nn));
                break;
            }
            Z_im = 2*Z_re*Z_im + c_im;
            Z_re = Z_re2 - Z_im2 + c_re;
        }
        *(ptr + offset) = bgra;
#endif
    }
}

__global__ void mandel_double(uchar4 *ptr, int w, int h, double cx, double cy, double zoom, int iter_mult)
{
    int x = threadIdx.x + blockIdx.x * blockDim.x;
    int y = threadIdx.y + blockIdx.y * blockDim.y;
    int offset = x + y * blockDim.x * gridDim.x;

    if(x < w && y < h) {
#if 0
        uchar4 bgra = {0x0,0x0,0x0,0x0};
        bgra.y = (unsigned char)(t + 255.99*y/(double)h);
        bgra.z = (unsigned char)(t + 255.99*x/(double)w);
        *(ptr + offset) = bgra;
#else
        uchar4 bgra = {0x0,0x0,0x0,0x0};  // inside = black
        unsigned int MaxIterations = iter_mult*256;
        double ImageWidth = w;
        double ImageHeight = h;
        double MinRe = cx - 1.0/zoom;
        double MaxRe = cx + 1.0/zoom;
        double MinIm = cy - 1.0/zoom;
        double MaxIm = MinIm+(MaxRe-MinRe)*ImageHeight/ImageWidth;
        double Re_factor = (MaxRe-MinRe)/(ImageWidth-1);
        double Im_factor = (MaxIm-MinIm)/(ImageHeight-1);

        double c_im = MinIm + y*Im_factor;
        double c_re = MinRe + x*Re_factor;

        double Z_re = c_re, Z_im = c_im;
        //bool isInside = true;
        for(unsigned int n=0; n<MaxIterations; ++n) {
            double Z_re2 = Z_re*Z_re, Z_im2 = Z_im*Z_im;
            if(Z_re2 + Z_im2 > 4.0){
                int nn = n & 0xFF;
                // outside the set, set color
                bgra.x = unsigned(char(nn));
                bgra.y = unsigned(char(nn));
                bgra.z = unsigned(char(nn));
                break;
            }
            Z_im = 2*Z_re*Z_im + c_im;
            Z_re = Z_re2 - Z_im2 + c_re;
        }
        *(ptr + offset) = bgra;
#endif
    }
}

void mandelbrot(void *devPtr, int w, int h, double cx, double cy, double zoom)
{
    const int blockSize = 16; // 256 threads per block
    if(false) {
        mandel_float<<<dim3(w / blockSize, h / blockSize), dim3(blockSize, blockSize)>>>((uchar4 *) devPtr, w, h, float(cx), float(cy), float(zoom), 1);
    }
    else {
        mandel_double<<<dim3(w / blockSize, h / blockSize), dim3(blockSize, blockSize)>>>((uchar4 *) devPtr, w, h, cx, cy, zoom, 1);
    }
}