#include <stdio.h>
#include <stdint.h>
#include <thread>
#include <vector>

// include necessary header files
#include "etc2comp/EtcLib/Etc/Etc.h"
#include "etc2comp/EtcLib/Etc/EtcImage.h"
#include "etc2comp/EtcLib/Etc/EtcFilter.h"
#include "etc2comp/EtcTool/EtcFile.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

// this demo loads a .jpg image via the STB library, converts it into an
// ETC2 image, and saves it within the.ktx file format

int main()
{
	//  load an image as a 4-component RGBA bitmap
	int w, h, comp;
	const uint8_t* img = stbi_load("data/ch2_sample3_STB.jpg", &w, &h, &comp, 4);

	// Etc2Comp takes floating-point RGBA bitmaps as input, so we have to convert our data:
	std::vector<float> rgbaf;
	for (int i = 0; i != w * h * 4; i += 4)
	{
		rgbaf.push_back(img[i + 0] / 255.0f);
		rgbaf.push_back(img[i + 1] / 255.0f);
		rgbaf.push_back(img[i + 2] / 255.0f);
		rgbaf.push_back(img[i + 3] / 255.0f);
	}

	// Because we don't use alpha transparency, our target format should be RGB8
	const auto etcFormat = Etc::Image::Format::RGB8;
	// use the default BT.709 error metric minimization schema
	const auto errorMetric = Etc::ErrorMetric::BT709;

	// encode the floating-point image into ETC2 format using Etc2Comp
	Etc::Image image(rgbaf.data(), w, h, errorMetric);
	image.Encode(
		etcFormat,
		errorMetric,
		ETCCOMP_DEFAULT_EFFORT_LEVEL,
		// The encoder takes the number of threads as input:
		std::thread::hardware_concurrency(),
		1024);

	// Once the image is converted, we can save it into the .ktx file format, which can
	// store compressed texture data that is directly consumable by OpenGL
	Etc::File etcFile(
		// this .ktx file can be loaded into an OpenGL or Vulkan texture
		"image.ktx",
		Etc::File::Format::KTX,
		etcFormat,
		image.GetEncodingBits(),
		image.GetEncodingBitsBytes(),
		image.GetSourceWidth(),
		image.GetSourceHeight(),
		image.GetExtendedWidth(),
		image.GetExtendedHeight()
	);
	etcFile.Write();

	return 0;
}
