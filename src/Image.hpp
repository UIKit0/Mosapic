#ifndef IMAGE_HDR
#define IMAGE_HDR

#include "Types.hpp"

DECLARE_CLASS(Image);
DECLARE_CLASS(SDL_Surface);
DECLARE_CLASS(SDL_PixelFormat);

class Image {
public:
	Image();
	Image(cInt inWidth, cInt inHeight);
	~Image();

	int Read(rcString inFileName);
	void Write(rcString inFileName);

	Uint32 GetPixel(cInt inX, cInt inY);
	void PutPixel(cInt inX, cInt inY, cUint32 inColor);
	bool Resize(int inX, int inY, cBool inDeform, cFloat inCropScalar);
	pSDL_PixelFormat GetFormat();

	int Width();
	int Height();

private:
	pSDL_Surface mSurface;

	void PutPixel(pSDL_Surface inSurface, cInt inX, cInt inY, cUint32 inColor);
	Uint32 Average(cInt inXStart, cInt inYStart, cInt inXEnd, cInt inYEnd);
};

#endif // IMAGE_HDR
