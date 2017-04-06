/*     Copyright 2015-2017 Egor Yusov
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF ANY PROPRIETARY RIGHTS.
 *
 *  In no event and under no legal theory, whether in tort (including negligence), 
 *  contract, or otherwise, unless required by applicable law (such as deliberate 
 *  and grossly negligent acts) or agreed to in writing, shall any Contributor be
 *  liable for any damages, including any direct, indirect, special, incidental, 
 *  or consequential damages of any character arising as a result of this License or 
 *  out of the use or inability to use the software (including but not limited to damages 
 *  for loss of goodwill, work stoppage, computer failure or malfunction, or any and 
 *  all other commercial damages or losses), even if such Contributor has been advised 
 *  of the possibility of such damages.
 */

#include "pch.h"

#include "TexRegionRender.h"
#include "RenderDeviceGLImpl.h"
#include "DeviceContextGLImpl.h"
#include "MapHelper.h"

namespace Diligent
{
    static const Char* VertexShaderSource =
    {
        //To use any built-in input or output in the gl_PerVertex and
        //gl_PerFragment blocks in separable program objects, shader code must
        //redeclare those blocks prior to use. 
        //
        // Declaring this block causes compilation error on NVidia GLES
        "#ifndef GL_ES          \n"
        "out gl_PerVertex       \n"
        "{                      \n"
        "	vec4 gl_Position;   \n"
        "};                     \n"
        "#endif                 \n"

        "void main()                                    \n"
        "{                                              \n"
        "   vec4 Bounds = vec4(-1.0, -1.0, 1.0, 1.0);   \n"
        "   vec2 PosXY[4] =                             \n"
        "   {                                           \n"
        "       Bounds.xy,                              \n"
        "       Bounds.xw,                              \n"
        "       Bounds.zy,                              \n"
        "       Bounds.zw                               \n"
        "   };                                          \n"
        "   gl_Position = vec4(PosXY[gl_VertexID], 0.0, 1.0);\n"
        "}                                              \n"
    };

    TexRegionRender::TexRegionRender( class RenderDeviceGLImpl *pDeviceGL ) : 
        m_OrigStencilRef(0),
        m_NumRenderTargets(0)
    {
        memset( m_OrigBlendFactors, 0, sizeof(m_OrigBlendFactors) );
        memset( m_pOrigRTVs, 0, sizeof( m_pOrigRTVs ) );

        ShaderCreationAttribs ShaderAttrs;
        ShaderAttrs.Desc.Name = "TexRegionRender : Vertex shader";
        ShaderAttrs.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderAttrs.Source = VertexShaderSource;
        pDeviceGL->CreateShader( ShaderAttrs, &m_pVertexShader, 
							     true // We must indicate the shader is internal device object
							   );

        static const char* SamplerType[RESOURCE_DIM_NUM_DIMENSIONS] = {};
        SamplerType[RESOURCE_DIM_TEX_1D]           = "sampler1D";
        SamplerType[RESOURCE_DIM_TEX_1D_ARRAY]     = "sampler1DArray";
        SamplerType[RESOURCE_DIM_TEX_2D]           = "sampler2D";
        SamplerType[RESOURCE_DIM_TEX_2D_ARRAY]     = "sampler2DArray",
        SamplerType[RESOURCE_DIM_TEX_3D]           = "sampler3D";
        // There is no texelFetch() for texture cube [array]
        //SamplerType[RESOURCE_DIM_TEX_CUBE]         = "samplerCube";
        //SamplerType[RESOURCE_DIM_TEX_CUBE_ARRAY]   = "smaplerCubeArray";

        static const char* SrcLocations[RESOURCE_DIM_NUM_DIMENSIONS] = {};
        SrcLocations[RESOURCE_DIM_TEX_1D]           = "int(gl_FragCoord.x) + Constants.x";
        SrcLocations[RESOURCE_DIM_TEX_1D_ARRAY]     = "ivec2(int(gl_FragCoord.x) + Constants.x, Constants.z)";
        SrcLocations[RESOURCE_DIM_TEX_2D]           = "ivec2(gl_FragCoord.xy) + Constants.xy";
        SrcLocations[RESOURCE_DIM_TEX_2D_ARRAY]     = "ivec3(ivec2(gl_FragCoord.xy) + Constants.xy, Constants.z)",
        SrcLocations[RESOURCE_DIM_TEX_3D]           = "ivec3(ivec2(gl_FragCoord.xy) + Constants.xy, Constants.z)";
        // There is no texelFetch() for texture cube [array]
        //CoordDim[RESOURCE_DIM_TEX_CUBE]         = "ivec2(gl_FragCoord.xy)";
        //CoordDim[RESOURCE_DIM_TEX_CUBE_ARRAY]   = "ivec2(gl_FragCoord.xy)";

        BufferDesc CBDesc;
        CBDesc.uiSizeInBytes = sizeof(Int32)*4;
        CBDesc.Usage = USAGE_DYNAMIC;
        CBDesc.BindFlags = BIND_UNIFORM_BUFFER;
        CBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
        pDeviceGL->CreateBuffer( CBDesc, BufferData(), &m_pConstantBuffer, 
								 true // We must indicate the buffer is internal device object
							   );

        PipelineStateDesc PSODesc;
        auto &GraphicsPipeline = PSODesc.GraphicsPipeline;
        GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;
        GraphicsPipeline.RasterizerDesc.FillMode = FILL_MODE_SOLID;

        GraphicsPipeline.DepthStencilDesc.DepthEnable = false;
        GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = false;
        GraphicsPipeline.pVS = m_pVertexShader;

        static const char* CmpTypePrefix[3] = { "", "i", "u" };
        for( Int32 Dim = RESOURCE_DIM_TEX_2D; Dim <= RESOURCE_DIM_TEX_3D; ++Dim )
        {
            const auto *SamplerDim = SamplerType[Dim];
            const auto *SrcLocation = SrcLocations[Dim];
            for( Int32 Fmt = 0; Fmt < 3; ++Fmt )
            {
                const auto *Prefix = CmpTypePrefix[Fmt];
                String Name = "TexRegionRender : Pixel shader ";
                Name.append( Prefix );
                Name.append( SamplerDim );
                ShaderAttrs.Desc.Name = Name.c_str();
                ShaderAttrs.Desc.ShaderType = SHADER_TYPE_PIXEL;
                
                std::stringstream SourceSS;
                SourceSS << "uniform " << Prefix << SamplerDim << " gSourceTex;\n"
                         << "layout( location = 0 ) out " << Prefix << "vec4 Out;\n"
                         << "uniform cbConstants\n"
                         << "{\n"
	                     << "    ivec4 Constants;\n"
                         << "};\n"
                         << "void main()\n"
                         << "{\n"
                         << "    Out = texelFetch( gSourceTex, " << SrcLocation << ", Constants.w );\n"
                         << "}\n";

                auto Source = SourceSS.str();
                ShaderAttrs.Source = Source.c_str();
                auto &FragmetShader = m_pFragmentShaders[Dim*3 + Fmt];
                pDeviceGL->CreateShader( ShaderAttrs, &FragmetShader, 
										 true // We must indicate the shader is an internal device object
									   );
                GraphicsPipeline.pPS = FragmetShader;
                pDeviceGL->CreatePipelineState( PSODesc, &m_pPSO[Dim*3 + Fmt], 
										        true // We must indicate the PSO is an internal device object
									           );

                FragmetShader->GetShaderVariable( "cbConstants" )->Set( m_pConstantBuffer );
            }
        }
    }

    void TexRegionRender::SetStates( DeviceContextGLImpl *pCtxGL )
    {
        pCtxGL->GetRenderTargets( m_NumRenderTargets, m_pOrigRTVs, &m_pOrigDSV );

        Uint32 NumViewports = 0;
        pCtxGL->GetViewports( NumViewports, nullptr );
        m_OrigViewports.resize(NumViewports);
        pCtxGL->GetViewports( NumViewports, m_OrigViewports.data() );

        pCtxGL->GetPipelineState(&m_pOrigPSO, m_OrigBlendFactors, m_OrigStencilRef);
    }

    void TexRegionRender::RestoreStates( DeviceContextGLImpl *pCtxGL )
    {
        pCtxGL->SetRenderTargets( m_NumRenderTargets, m_pOrigRTVs, m_pOrigDSV );
        for( Uint32 rt = 0; rt < _countof( m_pOrigRTVs ); ++rt )
        {
            if( m_pOrigRTVs[rt] )
                m_pOrigRTVs[rt]->Release();
            m_pOrigRTVs[rt] = nullptr;
        }
        m_pOrigDSV.Release();

        pCtxGL->SetViewports( (Uint32)m_OrigViewports.size(), m_OrigViewports.data(), 0, 0 );

        pCtxGL->SetPipelineState( m_pOrigPSO );
        m_pOrigPSO.Release();
        pCtxGL->SetStencilRef(m_OrigStencilRef);
        pCtxGL->SetBlendFactors(m_OrigBlendFactors);
    }

    void TexRegionRender::Render( DeviceContextGLImpl *pCtxGL,
                                   ITextureView *pSrcSRV,
                                   RESOURCE_DIMENSION TexType,
                                   TEXTURE_FORMAT TexFormat,
                                   Int32 DstToSrcXOffset,
                                   Int32 DstToSrcYOffset,
                                   Int32 SrcZ,
                                   Int32 SrcMipLevel)
    {
        {
            MapHelper< int > pConstant( pCtxGL, m_pConstantBuffer, MAP_WRITE_DISCARD, 0 );
            pConstant[0] = DstToSrcXOffset;
            pConstant[1] = DstToSrcYOffset;
            pConstant[2] = SrcZ;
            pConstant[3] = SrcMipLevel;
        }

        const auto &TexFmtAttribs = GetTextureFormatAttribs(TexFormat);
        Uint32 FSInd = TexType * 3;
        if( TexFmtAttribs.ComponentType == COMPONENT_TYPE_SINT )
            FSInd += 1;
        else if( TexFmtAttribs.ComponentType == COMPONENT_TYPE_UINT )
            FSInd += 2;

        if( TexFmtAttribs.ComponentType == COMPONENT_TYPE_SNORM )
        {
            LOG_WARNING_MESSAGE("CopyData() is performed by rendering to texture.\n"
                                "There might be an issue in OpenGL driver on NVidia hardware: when rendering to SNORM textures, all negative values are clamped to zero.")
        }

        pCtxGL->SetPipelineState(m_pPSO[FSInd]);
        auto SrcTexVar = m_pFragmentShaders[FSInd]->GetShaderVariable( "gSourceTex" );
        SrcTexVar->Set( pSrcSRV );

        DrawAttribs DrawAttrs;
        DrawAttrs.NumVertices = 4;
        DrawAttrs.Topology = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        pCtxGL->Draw( DrawAttrs );

        SrcTexVar->Set( nullptr );
    }
}