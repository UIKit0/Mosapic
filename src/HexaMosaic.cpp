#include "HexaMosaic.hpp"

#include "Debugger.hpp"
#include "Image.hpp"
#include <cmath>

HexaMosaic::HexaMosaic(
	const std::string& inSourceImage,
	const std::string& inDestImage,
	const std::string& inDatabase,
	const int inWidth,
	const int inHeight
):
	mSourceImage(inSourceImage),
	mDestImage(inDestImage),
	mDatabase(inDatabase),
	mWidth(inWidth),
	mHeight(inHeight) {
}

void HexaMosaic::Create() {
	Image src_img;
	src_img.Read(mSourceImage);

	// unit dimensions of hexagon facing upwards
	const float unit_dx    = 2.0f * sinf(M_PI/3.0f);
	const float unit_dy    = 1.5f;
	const float unit_ratio = unit_dx / unit_dy;
	const float radius     = 20.0f;

	// real ratios (from source image) of hexagon facing upwards
	const float dst_ratio  = (mWidth * unit_dx) / (mHeight * unit_dy);
	const float src_ratio  = (float(src_img.Width()) / src_img.Height());

	// source height is greater then destination height
	float dx, dy, start_x, start_y;
	if (src_ratio < dst_ratio)
	{
		dx = float(src_img.Width()) / mWidth;
		dy = dx / unit_ratio;
		start_x = dx / 2.0f;
		start_y = (src_img.Height() - mHeight * dy) / 2.0f;
	}
	// source width is greater then destination width
	else
	{
		dy = float(src_img.Height()) / mHeight;
		dx = dy * unit_ratio;
		start_y = dy / 2.0f;
		start_x = (src_img.Width() - mWidth * dx) / 2.0f;
	}

	Image dst_img(int(roundf(mWidth*radius*unit_dx)), int(roundf(mHeight*radius*unit_dy)));
	for (int j = 0; j < mHeight; j++)
	{
		int src_y = roundf(start_y + j * dy);
		float dst_y = unit_dy * radius * (j + 0.5f);
		for (int i = 0; i < mWidth; i++)
		{
			int src_x = roundf(start_x + i * dx + ((j % 2) * (dx / 2.0f)));
			float dst_x = unit_dx * radius * (i + 0.5f + ((j % 2) / 2.0f));
			Uint32 color = src_img.GetPixel(src_x, src_y);
			FillHexagon(dst_img, dst_x, dst_y, radius, color);
			//dst_img.PutPixel(dst_x, dst_y, color); 
		}
	}

	dst_img.Write(mDestImage);
}

void HexaMosaic::FillHexagon(Image& inImg, const float inX, const float inY, float inRadius, const Uint32 inColor) {

	// QUICKFIX
	inRadius += 1;

	for (int j = -inRadius; j < inRadius; j++)
	{
		for (int i = roundf(-inRadius * sinf(M_PI / 3.0f)); i < roundf(inRadius * sinf(M_PI / 3.0f)); i++)
		{
			if ( abs(i) <  float((j + inRadius) * 2.0f * sinf(M_PI / 3.0f)) && -abs(i) > float( (j - inRadius) * 2.0f* sinf(M_PI / 3.0f) ) )
			{
				inImg.PutPixel(i+inX, j+inY, inColor);
			}
		}
	}
}
