// DDS loader
// http://lonesock.net/soil.html

#ifndef HEADER_STB_IMAGE_DDS_AUGMENTATION
#define HEADER_STB_IMAGE_DDS_AUGMENTATION

#ifdef __cplusplus
extern "C" {
#endif

extern int stbi_dds_test_memory(stbi_uc const *buffer, int len);
extern stbi_uc *stbi_dds_load(const char *filename, int *x, int *y, int *comp, int req_comp);
extern stbi_uc *stbi_dds_load_from_memory(stbi_uc const *buffer, int len, int *x, int *y, int *comp, int req_comp);
#ifndef STBI_NO_STDIO
extern int stbi_dds_test_file(FILE *f);
extern stbi_uc *stbi_dds_load_from_file(FILE *f, int *x, int *y, int *comp, int req_comp);
#endif

#ifdef STB_IMAGE_DDS_IMPLEMENTATION

typedef struct DDS_HEADER
{
	unsigned int	 dwSize;
	unsigned int	 dwFlags;
	unsigned int	 dwHeight;
	unsigned int	 dwWidth;
	unsigned int	 dwPitchOrLinearSize;
	unsigned int	 dwDepth;
	unsigned int	 dwMipMapCount;
	unsigned int	 dwReserved1[11];

	struct DDS_PIXELFORMAT
	{
		unsigned int dwSize;
		unsigned int dwFlags;
		unsigned int dwFourCC;
		unsigned int dwRGBBitCount;
		unsigned int dwRBitMask;
		unsigned int dwGBitMask;
		unsigned int dwBBitMask;
		unsigned int dwAlphaBitMask;
	} sPixelFormat;

	struct // DDCAPS2
	{
		unsigned int dwCaps1;
		unsigned int dwCaps2;
		unsigned int dwDDSX;
		unsigned int dwReserved;
	} sCaps;

	unsigned int     dwReserved2;
} DDS_HEADER;
typedef struct DDS_HEADER_DXT10
{
	unsigned int	dxgiFormat;
	unsigned int	resourceDimension;
	unsigned int	miscFlag;
	unsigned int	arraySize;
	unsigned int	miscFlags2;
} DDS_HEADER_DXT10;

#define DDSD_CAPS					0x00000001
#define DDSD_HEIGHT					0x00000002
#define DDSD_WIDTH					0x00000004
#define DDSD_PITCH					0x00000008
#define DDSD_PIXELFORMAT			0x00001000
#define DDSD_MIPMAPCOUNT			0x00020000
#define DDSD_LINEARSIZE				0x00080000
#define DDSD_DEPTH					0x00800000

#define DDPF_ALPHAPIXELS			0x00000001
#define	DDPF_ALPHA					0x00000002
#define DDPF_FOURCC					0x00000004
#define DDPF_INDEXED                0x00000020 
#define DDPF_RGB					0x00000040
#define DDPF_YUV					0x00000200
#define DDPF_LUMINANCE				0x00020000

#define DDSCAPS_COMPLEX				0x00000008
#define DDSCAPS_TEXTURE				0x00001000
#define DDSCAPS_MIPMAP				0x00400000

#define DDSCAPS2_CUBEMAP			0x00000200
#define DDSCAPS2_CUBEMAP_POSITIVEX	0x00000400
#define DDSCAPS2_CUBEMAP_NEGATIVEX	0x00000800
#define DDSCAPS2_CUBEMAP_POSITIVEY	0x00001000
#define DDSCAPS2_CUBEMAP_NEGATIVEY	0x00002000
#define DDSCAPS2_CUBEMAP_POSITIVEZ	0x00004000
#define DDSCAPS2_CUBEMAP_NEGATIVEZ	0x00008000
#define DDSCAPS2_VOLUME				0x00200000

#define MAKEFOURCC(ch0, ch1, ch2, ch3) ((stbi__uint32)(stbi_uc)(ch0) | ((stbi__uint32)(stbi_uc)(ch1) << 8) | ((stbi__uint32)(stbi_uc)(ch2) << 16) | ((stbi__uint32)(stbi_uc)(ch3) << 24 ))

stbi_inline static int stbi__convert_bit_range(int c, int from_bits, int to_bits)
{
	int b = (1 << (from_bits - 1)) + c * ((1 << to_bits) - 1);

	return (b + (b >> from_bits)) >> from_bits;
}
stbi_inline static void stbi__rgb_888_from_565(unsigned int c, int *r, int *g, int *b)
{
	*r = stbi__convert_bit_range((c >> 11) & 31, 5, 8);
	*g = stbi__convert_bit_range((c >> 05) & 63, 6, 8);
	*b = stbi__convert_bit_range((c >> 00) & 31, 5, 8);
}
void stbi__dxt_decode_DXT1_block(unsigned char uncompressed[16 * 4], unsigned char compressed[8])
{
	int next_bit = 4 * 8;
	int i, r, g, b;
	int c0, c1;
	unsigned char decode_colors[4 * 4];
	//	find the 2 primary colors
	c0 = compressed[0] + (compressed[1] << 8);
	c1 = compressed[2] + (compressed[3] << 8);
	stbi__rgb_888_from_565(c0, &r, &g, &b);
	decode_colors[0] = r;
	decode_colors[1] = g;
	decode_colors[2] = b;
	decode_colors[3] = 255;
	stbi__rgb_888_from_565(c1, &r, &g, &b);
	decode_colors[4] = r;
	decode_colors[5] = g;
	decode_colors[6] = b;
	decode_colors[7] = 255;
	if (c0 > c1)
	{
		//	no alpha, 2 interpolated colors
		decode_colors[8] = (2 * decode_colors[0] + decode_colors[4]) / 3;
		decode_colors[9] = (2 * decode_colors[1] + decode_colors[5]) / 3;
		decode_colors[10] = (2 * decode_colors[2] + decode_colors[6]) / 3;
		decode_colors[11] = 255;
		decode_colors[12] = (decode_colors[0] + 2 * decode_colors[4]) / 3;
		decode_colors[13] = (decode_colors[1] + 2 * decode_colors[5]) / 3;
		decode_colors[14] = (decode_colors[2] + 2 * decode_colors[6]) / 3;
		decode_colors[15] = 255;
	}
	else
	{
		//	1 interpolated color, alpha
		decode_colors[8] = (decode_colors[0] + decode_colors[4]) / 2;
		decode_colors[9] = (decode_colors[1] + decode_colors[5]) / 2;
		decode_colors[10] = (decode_colors[2] + decode_colors[6]) / 2;
		decode_colors[11] = 255;
		decode_colors[12] = 0;
		decode_colors[13] = 0;
		decode_colors[14] = 0;
		decode_colors[15] = 0;
	}
	//	decode the block
	for (i = 0; i < 16 * 4; i += 4)
	{
		int idx = ((compressed[next_bit >> 3] >> (next_bit & 7)) & 3) * 4;
		next_bit += 2;
		uncompressed[i + 0] = decode_colors[idx + 0];
		uncompressed[i + 1] = decode_colors[idx + 1];
		uncompressed[i + 2] = decode_colors[idx + 2];
		uncompressed[i + 3] = decode_colors[idx + 3];
	}
	//	done
}
void stbi__dxt_decode_DXT23_alpha_block(unsigned char uncompressed[16 * 4], unsigned char compressed[8])
{
	int i, next_bit = 0;

	for (i = 3; i < 16 * 4; i += 4)
	{
		uncompressed[i] = stbi__convert_bit_range((compressed[next_bit >> 3] >> (next_bit & 7)) & 15, 4, 8);
		next_bit += 4;
	}
}
void stbi__dxt_decode_DXT45_alpha_block(unsigned char uncompressed[16 * 4], unsigned char compressed[8])
{
	int i, next_bit = 8 * 2;
	unsigned char decode_alpha[8];
	//	each alpha value gets 3 bits, and the 1st 2 bytes are the range
	decode_alpha[0] = compressed[0];
	decode_alpha[1] = compressed[1];
	if (decode_alpha[0] > decode_alpha[1])
	{
		//	6 step intermediate
		decode_alpha[2] = (6 * decode_alpha[0] + 1 * decode_alpha[1]) / 7;
		decode_alpha[3] = (5 * decode_alpha[0] + 2 * decode_alpha[1]) / 7;
		decode_alpha[4] = (4 * decode_alpha[0] + 3 * decode_alpha[1]) / 7;
		decode_alpha[5] = (3 * decode_alpha[0] + 4 * decode_alpha[1]) / 7;
		decode_alpha[6] = (2 * decode_alpha[0] + 5 * decode_alpha[1]) / 7;
		decode_alpha[7] = (1 * decode_alpha[0] + 6 * decode_alpha[1]) / 7;
	}
	else
	{
		//	4 step intermediate, pluss full and none
		decode_alpha[2] = (4 * decode_alpha[0] + 1 * decode_alpha[1]) / 5;
		decode_alpha[3] = (3 * decode_alpha[0] + 2 * decode_alpha[1]) / 5;
		decode_alpha[4] = (2 * decode_alpha[0] + 3 * decode_alpha[1]) / 5;
		decode_alpha[5] = (1 * decode_alpha[0] + 4 * decode_alpha[1]) / 5;
		decode_alpha[6] = 0;
		decode_alpha[7] = 255;
	}
	for (i = 3; i < 16 * 4; i += 4)
	{
		int idx = 0, bit;
		bit = (compressed[next_bit >> 3] >> (next_bit & 7)) & 1;
		idx += bit << 0;
		++next_bit;
		bit = (compressed[next_bit >> 3] >> (next_bit & 7)) & 1;
		idx += bit << 1;
		++next_bit;
		bit = (compressed[next_bit >> 3] >> (next_bit & 7)) & 1;
		idx += bit << 2;
		++next_bit;
		uncompressed[i] = decode_alpha[idx & 7];
	}
	//	done
}
void stbi__dxt_decode_DXT_color_block(unsigned char uncompressed[16 * 4], unsigned char compressed[8])
{
	int next_bit = 4 * 8;
	int i, r, g, b;
	int c0, c1;
	unsigned char decode_colors[4 * 3];
	//	find the 2 primary colors
	c0 = compressed[0] + (compressed[1] << 8);
	c1 = compressed[2] + (compressed[3] << 8);
	stbi__rgb_888_from_565(c0, &r, &g, &b);
	decode_colors[0] = r;
	decode_colors[1] = g;
	decode_colors[2] = b;
	stbi__rgb_888_from_565(c1, &r, &g, &b);
	decode_colors[3] = r;
	decode_colors[4] = g;
	decode_colors[5] = b;
	//	Like DXT1, but no choicees:
	//	no alpha, 2 interpolated colors
	decode_colors[6] = (2 * decode_colors[0] + decode_colors[3]) / 3;
	decode_colors[7] = (2 * decode_colors[1] + decode_colors[4]) / 3;
	decode_colors[8] = (2 * decode_colors[2] + decode_colors[5]) / 3;
	decode_colors[9] = (decode_colors[0] + 2 * decode_colors[3]) / 3;
	decode_colors[10] = (decode_colors[1] + 2 * decode_colors[4]) / 3;
	decode_colors[11] = (decode_colors[2] + 2 * decode_colors[5]) / 3;
	//	decode the block
	for (i = 0; i < 16 * 4; i += 4)
	{
		int idx = ((compressed[next_bit >> 3] >> (next_bit & 7)) & 3) * 3;
		next_bit += 2;
		uncompressed[i + 0] = decode_colors[idx + 0];
		uncompressed[i + 1] = decode_colors[idx + 1];
		uncompressed[i + 2] = decode_colors[idx + 2];
	}
	//	done
}

static int stbi__dds_test(stbi__context *s)
{
	// Check the magic number
	if (stbi__get8(s) != 'D')
	{
		return 0;
	}
	if (stbi__get8(s) != 'D')
	{
		return 0;
	}
	if (stbi__get8(s) != 'S')
	{
		return 0;
	}
	if (stbi__get8(s) != ' ')
	{
		return 0;
	}

	return 1;
}

static stbi_uc *stbi__dds_load(stbi__context *s, int *x, int *y, int *comp, int req_comp)
{
	// All variables go up front
	stbi_uc *dds_data = NULL;
	stbi_uc block[16 * 4];
	stbi_uc compressed[8];
	int flags, DXT_family;
	int has_alpha, has_mipmap;
	int is_compressed, cubemap_faces;
	int block_pitch, num_blocks;
	int i, sz, cf;
	DDS_HEADER header;
	DDS_HEADER_DXT10 header2;

	// Check the magic number
	if (!stbi__dds_test(s))
	{
		return NULL;
	}

	// Load the header
	stbi__getn(s, (stbi_uc *)(&header), sizeof(DDS_HEADER));

	if (header.dwSize != 124)
	{
		return NULL;
	}

	// Checks
	flags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT;

	if ((header.dwFlags & flags) != flags)
	{
		return NULL;
	}

	if (header.sPixelFormat.dwSize != 32)
	{
		return NULL;
	}

	flags = DDPF_FOURCC | DDPF_RGB | DDPF_LUMINANCE | DDPF_YUV;

	if ((header.sPixelFormat.dwFlags & flags) == 0)
	{
		return NULL;
	}

	if ((header.sCaps.dwCaps1 & DDSCAPS_TEXTURE) == 0)
	{
		return NULL;
	}

	// Get the image information
	*x = s->img_x = header.dwWidth;
	*y = s->img_y = header.dwHeight;
	*comp = s->img_n = 4;

	is_compressed = (header.sPixelFormat.dwFlags & DDPF_FOURCC) / DDPF_FOURCC;
	has_alpha = (header.sPixelFormat.dwFlags & DDPF_ALPHAPIXELS) / DDPF_ALPHAPIXELS;
	has_mipmap = (header.sCaps.dwCaps1 & DDSCAPS_MIPMAP) && (header.dwMipMapCount > 1);
	cubemap_faces = (header.sCaps.dwCaps2 & DDSCAPS2_CUBEMAP) / DDSCAPS2_CUBEMAP;

	// Cubemaps need to have square faces
	cubemap_faces &= (s->img_x == s->img_y);
	cubemap_faces *= 5;
	cubemap_faces += 1;

	block_pitch = (s->img_x + 3) >> 2;
	num_blocks = block_pitch * ((s->img_y + 3) >> 2);

	if (is_compressed)
	{
#pragma region Compressed
		if (header.sPixelFormat.dwFourCC & MAKEFOURCC('D', 'X', '1', '0'))
		{
			stbi__getn(s, (stbi_uc *)(&header2), sizeof(DDS_HEADER_DXT10));
		}

		/*	compressed	*/
		//	note: header.sPixelFormat.dwFourCC is something like (('D'<<0)|('X'<<8)|('T'<<16)|('1'<<24))
		DXT_family = 1 + (header.sPixelFormat.dwFourCC >> 24) - '1';
		if ((DXT_family < 1) || (DXT_family > 5)) return NULL;
		/*	check the expected size...oops, nevermind...
		those non-compliant writers leave
		dwPitchOrLinearSize == 0	*/
		//	passed all the tests, get the RAM for decoding
		sz = (s->img_x)*(s->img_y) * 4 * cubemap_faces;
		dds_data = (unsigned char*)malloc(sz);
		/*	do this once for each face	*/
		for (cf = 0; cf < cubemap_faces; ++cf)
		{
			//	now read and decode all the blocks
			for (i = 0; i < num_blocks; ++i)
			{
				//	where are we?
				int bx, by, bw = 4, bh = 4;
				int ref_x = 4 * (i % block_pitch);
				int ref_y = 4 * (i / block_pitch);
				//	get the next block's worth of compressed data, and decompress it
				if (DXT_family == 1)
				{
					//	DXT1
					stbi__getn(s, compressed, 8);
					stbi__dxt_decode_DXT1_block(block, compressed);
				}
				else if (DXT_family < 4)
				{
					//	DXT2/3
					stbi__getn(s, compressed, 8);
					stbi__dxt_decode_DXT23_alpha_block(block, compressed);
					stbi__getn(s, compressed, 8);
					stbi__dxt_decode_DXT_color_block(block, compressed);
				}
				else
				{
					//	DXT4/5
					stbi__getn(s, compressed, 8);
					stbi__dxt_decode_DXT45_alpha_block(block, compressed);
					stbi__getn(s, compressed, 8);
					stbi__dxt_decode_DXT_color_block(block, compressed);
				}
				//	is this a partial block?
				if (ref_x + 4 > (int)s->img_x)
				{
					bw = s->img_x - ref_x;
				}
				if (ref_y + 4 > (int)s->img_y)
				{
					bh = s->img_y - ref_y;
				}
				//	now drop our decompressed data into the buffer
				for (by = 0; by < bh; ++by)
				{
					int idx = 4 * ((ref_y + by + cf*s->img_x)*s->img_x + ref_x);
					for (bx = 0; bx < bw * 4; ++bx)
					{

						dds_data[idx + bx] = block[by * 16 + bx];
					}
				}
			}
			/*	done reading and decoding the main image...
			skip MIPmaps if present	*/
			if (has_mipmap)
			{
				int block_size = 16;
				if (DXT_family == 1)
				{
					block_size = 8;
				}
				for (i = 1; i < (int)header.dwMipMapCount; ++i)
				{
					int mx = s->img_x >> (i + 2);
					int my = s->img_y >> (i + 2);
					if (mx < 1)
					{
						mx = 1;
					}
					if (my < 1)
					{
						my = 1;
					}
					stbi__skip(s, mx*my*block_size);
				}
			}
		}/* per cubemap face */
#pragma endregion
	}
	else
	{
#pragma region Uncompressed
		DXT_family = 0;

		if (header.sPixelFormat.dwRGBBitCount != 0)
		{
			s->img_n = header.sPixelFormat.dwRGBBitCount / 8;
		}
		else
		{
			s->img_n = 3;
			if (has_alpha)
			{
				s->img_n = 4;
			}
		}
		*comp = s->img_n;
		sz = s->img_x*s->img_y*s->img_n*cubemap_faces;
		dds_data = (unsigned char*)malloc(sz);
		/*	do this once for each face	*/
		for (cf = 0; cf < cubemap_faces; ++cf)
		{
			/*	read the main image for this face	*/
			stbi__getn(s, &dds_data[cf*s->img_x*s->img_y*s->img_n], s->img_x*s->img_y*s->img_n);
			/*	done reading and decoding the main image...
			skip MIPmaps if present	*/
			if (has_mipmap)
			{
				for (i = 1; i < (int)header.dwMipMapCount; ++i)
				{
					int mx = s->img_x >> i;
					int my = s->img_y >> i;
					if (mx < 1)
					{
						mx = 1;
					}
					if (my < 1)
					{
						my = 1;
					}
					stbi__skip(s, mx*my*s->img_n);
				}
			}
		}
		if (s->img_n >= 3)
		{
			/*	data was BGR, I need it RGB	*/
			for (i = 0; i < sz; i += s->img_n)
			{
				unsigned char temp = dds_data[i];
				dds_data[i] = dds_data[i + 2];
				dds_data[i + 2] = temp;
			}
		}
#pragma endregion
	}

	// Finished decompressing into RGBA, adjust the y size if we have a cubemap
	s->img_y *= cubemap_faces;
	*y = s->img_y;

	//	Test if all alpha values are 255 (i.e. no transparency)
	has_alpha = 0;

	if (s->img_n == 4)
	{
		for (i = 3; (i < sz) && (has_alpha == 0); i += 4)
		{
			has_alpha |= (dds_data[i] < 255);
		}
	}

	if ((req_comp <= 4) && (req_comp >= 1))
	{
		if (req_comp != s->img_n)
		{
			dds_data = stbi__convert_format(dds_data, s->img_n, req_comp, s->img_x, s->img_y);
			*comp = s->img_n;
		}
	}
	else
	{
		if ((has_alpha == 0) && (s->img_n == 4))
		{
			dds_data = stbi__convert_format(dds_data, 4, 3, s->img_x, s->img_y);
			*comp = 3;
		}
	}

	return dds_data;
}

#ifndef STBI_NO_STDIO
int stbi_dds_test_file(FILE *f)
{
	stbi__context s;
	int r, n = ftell(f);
	stbi__start_file(&s, f);
	r = stbi__dds_test(&s);
	fseek(f, n, SEEK_SET);
	return r;
}
#endif
int stbi_dds_test_memory(stbi_uc const *buffer, int len)
{
	stbi__context s;
	stbi__start_mem(&s, buffer, len);
	return stbi__dds_test(&s);
}

#ifndef STBI_NO_STDIO
stbi_uc *stbi_dds_load_from_file(FILE *f, int *x, int *y, int *comp, int req_comp)
{
	stbi__context s;
	stbi__start_file(&s, f);
	return stbi__dds_load(&s, x, y, comp, req_comp);
}
stbi_uc *stbi_dds_load(const char *filename, int *x, int *y, int *comp, int req_comp)
{
	stbi_uc *data;
	FILE *f = stbi__fopen(filename, "rb");
	if (!f) return NULL;
	data = stbi_dds_load_from_file(f, x, y, comp, req_comp);
	fclose(f);
	return data;
}
#endif

stbi_uc *stbi_dds_load_from_memory(stbi_uc const *buffer, int len, int *x, int *y, int *comp, int req_comp)
{
	stbi__context s;
	stbi__start_mem(&s, buffer, len);
	return stbi__dds_load(&s, x, y, comp, req_comp);
}

#endif

#ifdef __cplusplus
}
#endif

#endif // HEADER_STB_IMAGE_DDS_AUGMENTATION
