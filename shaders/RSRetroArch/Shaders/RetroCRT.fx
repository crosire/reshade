#include "ReShade.fxh"

/*
Retro (CRT) shader, created for use in PPSSPP by KillaMaaki and ported to ReShade 4 by Marot Satil.

Source: https://github.com/hrydgard/ppsspp/blob/master/assets/shaders/crt.fsh

PSPSDK BSD-compatible copyright notice (Some parts of the PSPSDK were used in this emulator (defines, constants, headers))

Copyright (c) 2005  adresd
Copyright (c) 2005  Marcus R. Brown
Copyright (c) 2005  James Forshaw
Copyright (c) 2005  John Kelley
Copyright (c) 2005  Jesper Svennevid
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The names of the authors may not be used to endorse or promote products
   derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

uniform float Timer < source = "timer"; >;

float4 RetroCRTPass(float4 vpos : SV_Position, float2 texcoord : TexCoord) : SV_Target
{
	const float GLTimer = Timer * 0.0001;

	const float vPos = float( ( texcoord.y + GLTimer * 0.5 ) * 272.0 );
	const float line_intensity = vPos - 2.0 * floor(vPos / 2.0);
    
	// color shift
	const float2 shift = float2( line_intensity * 0.0005, 0 );
    
	// shift R and G channels to simulate NTSC color bleed
	const float2 colorShift = float2( 0.001, 0 );

	const float4 c = float4( tex2D( ReShade::BackBuffer, texcoord + colorShift + shift ).x,			// Red
					tex2D( ReShade::BackBuffer, texcoord - colorShift + shift ).y * 0.99,	// Green
					tex2D( ReShade::BackBuffer, texcoord ).z,				// Blue
					1.0) * clamp( line_intensity, 0.85, 1.0 );

	return c + (sin( ( texcoord.y + GLTimer ) * 4.0 ) * 0.02);
}

technique RetroCRT
{
	pass
	{
		VertexShader = PostProcessVS;
		PixelShader = RetroCRTPass;
	}
}