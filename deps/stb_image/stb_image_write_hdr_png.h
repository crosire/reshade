/* 16-bit PNG file writing support
   License: Public Domain (for more information, please refer to https://unlicense.org/)
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

STBIWDEF int stbi_write_hdr_png_to_func(stbi_write_func *func, void *context, int w, int h, int comp, stbi_us *data, int stride_in_bytes, unsigned char color_primaries, unsigned char transfer_function);

#ifdef __cplusplus
}
#endif

#ifdef STB_IMAGE_WRITE_IMPLEMENTATION

STBIWDEF int stbi_write_hdr_png_to_func(stbi_write_func *func, void *context, int w, int h, int comp, stbi_us *data, int stride_bytes, unsigned char color_primaries, unsigned char transfer_function)
{
	const int ctype[5] = { -1, 0, 4, 2, 6 };
	const unsigned char sig[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
	int zlen, flen, filt_stride_bytes = w * sizeof(stbi_us) * comp;
	unsigned char *filt, *zlib, *file, *o;

	if (0 == stride_bytes)
		stride_bytes = filt_stride_bytes;

	filt = (unsigned char *)STBIW_MALLOC(h * (filt_stride_bytes + 1));
	if (!filt) return 0;

	for (int y = 0; y < h; ++y)
	{
		// Convert little endian to big endian data
		unsigned char *z = (unsigned char *)data + stride_bytes * (stbi__flip_vertically_on_write ? h - 1 - y : y);
		for (int i = 0; i < w * comp; ++i, z += 2)
		{
			unsigned char temp = z[0];
			z[0] = z[1];
			z[1] = temp;
		}

		filt[y * (filt_stride_bytes + 1)] = 2;
		stbiw__encode_png_line((unsigned char *)data, stride_bytes, w * 2, h, y, comp, 2, filt + y * (filt_stride_bytes + 1) + 1);
	}

	zlib = stbi_zlib_compress(filt, h * (filt_stride_bytes + 1), &zlen, stbi_write_png_compression_level);
	STBIW_FREE(filt);
	if (!zlib) return 0;

	flen = 8 + (12 + 13) + (12 + 4) + (12 + comp) + (12 + 32) + (12 + zlen) + 12;
	file = (unsigned char *)STBIW_MALLOC(flen);
	if (!file) return 0;

	o = file;
	STBIW_MEMMOVE(o, sig, 8); o += 8;

	// Image header
	stbiw__wp32(o, 13);
	stbiw__wptag(o, "IHDR");
	{
		stbiw__wp32(o, w);
		stbiw__wp32(o, h);
		*o++ = 16; // Bit depth
		*o++ = STBIW_UCHAR(ctype[comp]);
		*o++ = 0;
		*o++ = 0;
		*o++ = 0;
	}
	stbiw__wpcrc(&o, 13);

	// Coding-independent code points for video signal type identification
	stbiw__wp32(o, 4);
	stbiw__wptag(o, "cICP");
	{
		*o++ = color_primaries; // Color primaries
		*o++ = transfer_function; // Transfer function
		*o++ = 0; // Matrix coefficients
		*o++ = 1; // Video full range flag
	}
	stbiw__wpcrc(&o, 4);

	// Significant bits
	stbiw__wp32(o, comp);
	stbiw__wptag(o, "sBIT");
	{
		for (int c = 0; c < comp; ++c)
			*o++ = 16;
	}
	stbiw__wpcrc(&o, comp);

	// Primary chromaticities and white point
	stbiw__wp32(o, 32);
	stbiw__wptag(o, "cHRM");
	{
		stbiw__wp32(o, 31270); // White point x
		stbiw__wp32(o, 32900); // White point y
		stbiw__wp32(o, 70800); // Red x
		stbiw__wp32(o, 29200); // Red y
		stbiw__wp32(o, 17000); // Green x
		stbiw__wp32(o, 79700); // Green y
		stbiw__wp32(o, 13100); // Blue x
		stbiw__wp32(o,  4600); // Blue y
	}
	stbiw__wpcrc(&o, 32);

	// Image data
	stbiw__wp32(o, zlen);
	stbiw__wptag(o, "IDAT");
	{
		STBIW_MEMMOVE(o, zlib, zlen); o += zlen;
	}
	stbiw__wpcrc(&o, zlen);

	// Image trailer
	stbiw__wp32(o, 0);
	stbiw__wptag(o, "IEND");
	stbiw__wpcrc(&o, 0);

	STBIW_ASSERT(o == file + flen);

	STBIW_FREE(zlib);
	func(context, file, flen);
	STBIW_FREE(file);

	return 1;
}

#endif
