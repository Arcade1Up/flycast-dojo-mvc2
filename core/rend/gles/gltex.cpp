#include "glcache.h"
#include "gles.h"
#include "hw/pvr/pvr_mem.h"
#include "rend/TexCache.h"
#include "rend/CustomTexture.h"

#include <cstdio>
#include <cstdlib>

GlTextureCache TexCache;

static void readAsyncPixelBuffer(u32 addr);

void TextureCacheData::UploadToGPU(int width, int height, u8 *temp_tex_buffer, bool mipmapped, bool mipmapsIncluded)
{
	//upload to OpenGL !
	glcache.BindTexture(GL_TEXTURE_2D, texID);
	GLuint comps = tex_type == TextureType::_8 ? gl.single_channel_format : GL_RGBA;
	GLuint gltype;
	u32 bytes_per_pixel = 2;
	switch (tex_type)
	{
	case TextureType::_5551:
		gltype = GL_UNSIGNED_SHORT_5_5_5_1;
		break;
	case TextureType::_565:
		gltype = GL_UNSIGNED_SHORT_5_6_5;
		comps = GL_RGB;
		break;
	case TextureType::_4444:
		gltype = GL_UNSIGNED_SHORT_4_4_4_4;
		break;
	case TextureType::_8888:
		bytes_per_pixel = 4;
		gltype = GL_UNSIGNED_BYTE;
		break;
	case TextureType::_8:
		bytes_per_pixel = 1;
		gltype = GL_UNSIGNED_BYTE;
		break;
	default:
		die("Unsupported texture type");
		gltype = 0;
		break;
	}
	if (mipmapsIncluded)
	{
		int mipmapLevels = 0;
		int dim = width;
		while (dim != 0)
		{
			mipmapLevels++;
			dim >>= 1;
		}
#if !defined(GLES2) && (!defined(__APPLE__) || defined(TARGET_IPHONE))
		// Open GL 4.2 or GLES 3.0 min
		if (gl.gl_major > 4 || (gl.gl_major == 4 && gl.gl_minor >= 2)
				|| (gl.is_gles && gl.gl_major >= 3))
		{
			GLuint internalFormat;
			switch (tex_type)
			{
			case TextureType::_5551:
				internalFormat = GL_RGB5_A1;
				break;
			case TextureType::_565:
				internalFormat = GL_RGB565;
				break;
			case TextureType::_4444:
				internalFormat = GL_RGBA4;
				break;
			case TextureType::_8888:
				internalFormat = GL_RGBA8;
				break;
			case TextureType::_8:
				internalFormat = comps;
				break;
			default:
				die("Unsupported texture format");
				internalFormat = 0;
				break;
			}
			if (Updates == 1)
			{
				glTexStorage2D(GL_TEXTURE_2D, mipmapLevels, internalFormat, width, height);
				glCheck();
			}
			for (int i = 0; i < mipmapLevels; i++)
			{
				glTexSubImage2D(GL_TEXTURE_2D, mipmapLevels - i - 1, 0, 0, 1 << i, 1 << i, comps, gltype, temp_tex_buffer);
				temp_tex_buffer += (1 << (2 * i)) * bytes_per_pixel;
			}
		}
		else
#endif
		{
			for (int i = 0; i < mipmapLevels; i++)
			{
				glTexImage2D(GL_TEXTURE_2D, mipmapLevels - i - 1, comps, 1 << i, 1 << i, 0, comps, gltype, temp_tex_buffer);
				temp_tex_buffer += (1 << (2 * i)) * bytes_per_pixel;
			}
		}
	}
	else
	{
		glTexImage2D(GL_TEXTURE_2D, 0,comps, width, height, 0, comps, gltype, temp_tex_buffer);
		if (mipmapped)
			glGenerateMipmap(GL_TEXTURE_2D);
	}
	glCheck();
}
	
bool TextureCacheData::Delete()
{
	if (!BaseTextureCacheData::Delete())
		return false;

	if (texID && !sharedID)
		glcache.DeleteTextures(1, &texID);

	return true;
}

GLuint BindRTT(bool withDepthBuffer)
{
	GLenum channels, format;
	switch(pvrrc.fb_W_CTRL.fb_packmode)
	{
	case 0: //0x0   0555 KRGB 16 bit  (default)	Bit 15 is the value of fb_kval[7].
		channels = GL_RGBA;
		format = GL_UNSIGNED_BYTE;
		break;

	case 1: //0x1   565 RGB 16 bit
		channels = GL_RGB;
		format = GL_UNSIGNED_SHORT_5_6_5;
		break;

	case 2: //0x2   4444 ARGB 16 bit
		channels = GL_RGBA;
		format = GL_UNSIGNED_BYTE;
		break;

	case 3://0x3    1555 ARGB 16 bit    The alpha value is determined by comparison with the value of fb_alpha_threshold.
		channels = GL_RGBA;
		format = GL_UNSIGNED_BYTE;
		break;

	case 4: //0x4   888 RGB 24 bit packed
	case 5: //0x5   0888 KRGB 32 bit    K is the value of fk_kval.
	case 6: //0x6   8888 ARGB 32 bit
		WARN_LOG(RENDERER, "Unsupported render to texture format: %d", pvrrc.fb_W_CTRL.fb_packmode);
		return 0;

	case 7: //7     invalid
		WARN_LOG(RENDERER, "Invalid framebuffer format: 7");
		return 0;
	}
	u32 fbw = pvrrc.getFramebufferWidth();
	u32 fbh = pvrrc.getFramebufferHeight();
	u32 texAddress = pvrrc.fb_W_SOF1 & VRAM_MASK;
	DEBUG_LOG(RENDERER, "RTT packmode=%d stride=%d - %d x %d @ %06x", pvrrc.fb_W_CTRL.fb_packmode, pvrrc.fb_W_LINESTRIDE * 8,
			fbw, fbh, texAddress);

	if (gl.rtt.texAddress != ~0u)
		readAsyncPixelBuffer(gl.rtt.texAddress);
	gl.rtt.texAddress = texAddress;

	if (gl.rtt.fbo != 0)
		glDeleteFramebuffers(1, &gl.rtt.fbo);
	if (gl.rtt.tex != 0)
		glcache.DeleteTextures(1, &gl.rtt.tex);
	if (gl.rtt.depthb != 0)
		glDeleteRenderbuffers(1, &gl.rtt.depthb);

	u32 fbw2;
	u32 fbh2;
	getRenderToTextureDimensions(fbw, fbh, fbw2, fbh2);

#ifdef GL_PIXEL_PACK_BUFFER
	if (gl.gl_major >= 3 && config::RenderToTextureBuffer)
	{
		if (gl.rtt.pbo == 0)
			glGenBuffers(1, &gl.rtt.pbo);
		u32 glSize = fbw2 * fbh2 * 4;
		if (glSize > gl.rtt.pboSize)
		{
			glBindBuffer(GL_PIXEL_PACK_BUFFER, gl.rtt.pbo);
			glBufferData(GL_PIXEL_PACK_BUFFER, glSize, 0, GL_STREAM_READ);
			gl.rtt.pboSize = glSize;
			glCheck();
		}
	}
#endif

	// Create a texture for rendering to
	gl.rtt.tex = glcache.GenTexture();
	glcache.BindTexture(GL_TEXTURE_2D, gl.rtt.tex);
	glTexImage2D(GL_TEXTURE_2D, 0, channels, fbw2, fbh2, 0, channels, format, 0);

	// Create the object that will allow us to render to the aforementioned texture
	glGenFramebuffers(1, &gl.rtt.fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, gl.rtt.fbo);

	// Attach the texture to the FBO
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gl.rtt.tex, 0);

	if (withDepthBuffer)
	{
		// Generate and bind a render buffer which will become a depth buffer
		glGenRenderbuffers(1, &gl.rtt.depthb);
		glBindRenderbuffer(GL_RENDERBUFFER, gl.rtt.depthb);

		// Currently it is unknown to GL that we want our new render buffer to be a depth buffer.
		// glRenderbufferStorage will fix this and will allocate a depth buffer
		if (gl.is_gles)
		{
#if defined(GL_DEPTH24_STENCIL8)
            if (gl.gl_major >= 3)
                glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, fbw2, fbh2);
            else
#endif
#if defined(GL_DEPTH24_STENCIL8_OES)
            if (gl.GL_OES_packed_depth_stencil_supported)
                glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8_OES, fbw2, fbh2);
            else
#endif
#if defined(GL_DEPTH_COMPONENT24_OES)
            if (gl.GL_OES_depth24_supported)
                glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24_OES, fbw2, fbh2);
            else
#endif
				glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, fbw2, fbh2);
		}
#ifdef GL_DEPTH24_STENCIL8
		else
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, fbw2, fbh2);
#endif

		// Attach the depth buffer we just created to our FBO.
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, gl.rtt.depthb);

		if (!gl.is_gles || gl.gl_major >= 3 || gl.GL_OES_packed_depth_stencil_supported)
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, gl.rtt.depthb);
	}

	// Check that our FBO creation was successful
	GLuint uStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);

	verify(uStatus == GL_FRAMEBUFFER_COMPLETE);

	glViewport(0, 0, fbw, fbh);

	return gl.rtt.fbo;
}

void ReadRTTBuffer()
{
	u32 w = pvrrc.getFramebufferWidth();
	u32 h = pvrrc.getFramebufferHeight();

	const u8 fb_packmode = pvrrc.fb_W_CTRL.fb_packmode;

	if (config::RenderToTextureBuffer)
	{
		u32 tex_addr = gl.rtt.texAddress;
#ifdef TARGET_VIDEOCORE
		// Remove all vram locks before calling glReadPixels
		// (deadlock on rpi)
		u32 size = w * h * 2;
		u32 page_tex_addr = tex_addr & PAGE_MASK;
		u32 page_size = size + tex_addr - page_tex_addr;
		page_size = ((page_size - 1) / PAGE_SIZE + 1) * PAGE_SIZE;
		for (u32 page = page_tex_addr; page < page_tex_addr + page_size; page += PAGE_SIZE)
			VramLockedWriteOffset(page);
#endif

#ifdef GL_PIXEL_PACK_BUFFER
		if (gl.gl_major >= 3)
			glBindBuffer(GL_PIXEL_PACK_BUFFER, gl.rtt.pbo);
#endif

		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		gl.rtt.width = w;
		gl.rtt.height = h;
		u16 *dst = gl.gl_major >= 3 ? nullptr : (u16 *)&vram[tex_addr];

		gl.rtt.linestride = pvrrc.fb_W_LINESTRIDE * 8;
		if (gl.rtt.linestride == 0)
			gl.rtt.linestride = w * 2;

		GLint color_fmt, color_type;
		glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_FORMAT, &color_fmt);
		glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_TYPE, &color_type);

		if (fb_packmode == 1 && gl.rtt.linestride == w * 2 && color_fmt == GL_RGB && color_type == GL_UNSIGNED_SHORT_5_6_5)
		{
			gl.rtt.directXfer = true;
			glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, dst);
		}
		else
		{
			gl.rtt.directXfer = false;
			if (gl.gl_major >= 3)
			{
				gl.rtt.fb_w_ctrl = pvrrc.fb_W_CTRL;
				glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, 0);
			}
			else
			{
				PixelBuffer<u32> tmp_buf;
				tmp_buf.init(w, h);

				u8 *p = (u8 *)tmp_buf.data();
				glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, p);

				WriteTextureToVRam(w, h, p, dst, pvrrc.fb_W_CTRL, gl.rtt.linestride);
				gl.rtt.texAddress = ~0;
			}
		}
#ifdef GL_PIXEL_PACK_BUFFER
		glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
#endif
		glCheck();
	}
	else
	{
		//memset(&vram[gl.rtt.texAddress], 0, size);
		if (w <= 1024 && h <= 1024)
		{
			TextureCacheData *texture_data = TexCache.getRTTexture(gl.rtt.texAddress, fb_packmode, w, h);
			glcache.DeleteTextures(1, &texture_data->texID);
			texture_data->texID = gl.rtt.tex;
			gl.rtt.tex = 0;
			texture_data->dirty = 0;
			texture_data->unprotectVRam();
		}
		gl.rtt.texAddress = ~0;
	}
	glBindFramebuffer(GL_FRAMEBUFFER, gl.ofbo.origFbo);
}

static void readAsyncPixelBuffer(u32 addr)
{
#ifndef GLES2
	if (!config::RenderToTextureBuffer || gl.rtt.pbo == 0)
		return;

	u32 tex_addr = gl.rtt.texAddress;
	if (addr != tex_addr)
		return;
	gl.rtt.texAddress = ~0;

	glBindBuffer(GL_PIXEL_PACK_BUFFER, gl.rtt.pbo);
	u8 *ptr = (u8 *)glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, gl.rtt.pboSize, GL_MAP_READ_BIT);
	if (ptr == nullptr)
	{
		WARN_LOG(RENDERER, "glMapBuffer failed");
		return;
	}
	u16 *dst = (u16 *)&vram[tex_addr];

	if (gl.rtt.directXfer)
		// Can be read directly into vram
		memcpy(dst, ptr, gl.rtt.width * gl.rtt.height * 2);
	else
		WriteTextureToVRam(gl.rtt.width, gl.rtt.height, ptr, dst, gl.rtt.fb_w_ctrl, gl.rtt.linestride);

	glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
#endif
}

constexpr int AtlasTexs = 32;
constexpr int AtlasHeight = 32 * 32;
constexpr int AtlasSize = 32 * AtlasHeight / 2;
static std::unordered_map<u32, std::unique_ptr<TextureCacheData>> atlasTex;

void updateUVForAtlas(PolyParam *pp, float shift, float div)
{
	Vertex *vtx = &pvrrc.verts.head()[pp->first];
	for (u32 i = 0; i < pp->count; i++, vtx++)
		vtx->v = (vtx->v + shift) / div;
}

void OpenGLRenderer::clearTextureAtlas()
{
	for (auto& it : atlasTex)
		it.second->Delete();
	atlasTex.clear();
}

BaseTextureCacheData *gl_GetTexture(TSP tsp, TCW tcw, PolyParam *pp)
{
	u32 texAddr = (tcw.TexAddr << 3) & VRAM_MASK;
	if (tcw.PixelFmt == PixelPal4 && tsp.TexU == 2 && tsp.TexV == 2 && texAddr >= 0x417400 && texAddr < 0x477400)
	{
		u32 atlasAddr = texAddr - (texAddr - 0x417400) % AtlasSize;
		updateUVForAtlas(pp, (texAddr - atlasAddr) / 0x200, (float)AtlasTexs);
		pp->tcw.TexAddr = atlasAddr >> 3; // so that polys can be merged
		auto it = atlasTex.find(atlasAddr);
		if (it != atlasTex.end())
			return it->second.get();

		u8 *atlasMem = (u8 *)malloc(AtlasSize * 2); // 8 bpp
		PixelBuffer<u8> pixbuf;
		for (int i = 0; i < AtlasTexs; i++)
		{
			u32 addr = atlasAddr + i * 0x200;
			pixbuf.init(32, 32, &atlasMem[i * 0x400]);
			texPAL4PT_TW(&pixbuf, &vram[addr], 32, 32);
		}
		auto& tex = atlasTex[atlasAddr] = std::unique_ptr<TextureCacheData>(new TextureCacheData(tsp, tcw));
		tex->gpuPalette = true;
		tex->tex_type = TextureType::_8;
		tex->UploadToGPU(32, AtlasHeight, atlasMem, false, false);
		free(atlasMem);
		return tex.get();
	}

	//lookup texture
	TextureCacheData* tf = TexCache.getTextureCacheData(tsp, tcw);

	readAsyncPixelBuffer(tf->sa_tex);

	//update if needed
	if (tf->NeedsUpdate())
	{
		// 512 x 512
		if (tsp.TexU == 6 && tsp.TexV == 6)
		{
			tf->ComputeHash();
			auto it = TexCache.preloadedTextures.find(tf->texture_hash);
			if (it != TexCache.preloadedTextures.end())
			{
				tf->dirty = 0;
				tf->gpuPalette = false;
				tf->texID = it->second;
				tf->sharedID = true;
				tf->protectVRam();
				return tf;
			}
		}
		tf->Update();
	}
	else
	{
		if (tf->IsCustomTextureAvailable())
		{
			TexCache.DeleteLater(tf->texID);
			tf->texID = glcache.GenTexture();
			tf->CheckCustomTexture();
		}
	}
	return tf;
}

void GlTextureCache::PreloadTextures()
{
	settings.content.gameId = "MARVEL VS CAPCOM2  JAPAN";
	custom_texture.Init();
	custom_texture.LoadMap();
	for (const auto& pair : custom_texture.texture_map)
	{
		int width, height;
		u8 *data = custom_texture.LoadCustomTexture(pair.first, width, height);
		TSP tsp{};
		tsp.TexU = bitscanrev(width) - 3;
		tsp.TexV = bitscanrev(height) - 3;
		TCW tcw{};
		TextureCacheData tex(tsp, tcw);
		tex.tex_type = TextureType::_8888;
		tex.UploadToGPU(width, height, data, false, false);
		free(data);
		preloadedTextures[pair.first] = tex.texID;
		tex.texID = 0;
	}
}

void GlTextureCache::FreePreloadedTextures()
{
	for (const auto& pair : preloadedTextures)
		glcache.DeleteTextures(1, &pair.second);
	preloadedTextures.clear();
}

GLuint fbTextureId;

void RenderFramebuffer()
{
	if (FB_R_SIZE.fb_x_size == 0 || FB_R_SIZE.fb_y_size == 0)
		return;

	PixelBuffer<u32> pb;
	int width;
	int height;
	ReadFramebuffer(pb, width, height);
	
	if (fbTextureId == 0)
		fbTextureId = glcache.GenTexture();
	
	glcache.BindTexture(GL_TEXTURE_2D, fbTextureId);
	
	//set texture repeat mode
	glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pb.data());
}
