cmake_minimum_required (VERSION 3.3)

project(GraphicsEngineOpenGL CXX)

set(INCLUDE 
	include/AsyncWritableResource.h
	include/BufferGLImpl.h
	include/BufferViewGLImpl.h
	include/DeviceContextGLImpl.h 
	include/FBOCache.h
	include/GLContext.h
	include/GLContextState.h
	include/GLObjectWrapper.h
	include/GLProgram.h
	include/GLProgramResources.h
	include/GLTypeConversions.h
	include/pch.h
	include/PipelineStateGLImpl.h
	include/RenderDeviceGLImpl.h
	include/SamplerGLImpl.h
	include/ShaderGLImpl.h
	include/ShaderResourceBindingGLImpl.h
	include/SwapChainGLImpl.h
	include/TexRegionRender.h
	include/Texture1D_OGL.h
	include/Texture1DArray_OGL.h
	include/Texture2D_OGL.h
	include/Texture2DArray_OGL.h
	include/Texture3D_OGL.h
	include/TextureBaseGL.h
	include/TextureCube_OGL.h
	include/TextureCubeArray_OGL.h
	include/TextureViewGLImpl.h
	include/VAOCache.h
)

set(INTERFACE 
	interface/BaseInterfacesGL.h
	interface/BufferGL.h
	interface/BufferViewGL.h
	interface/DeviceContextGL.h
	interface/PipelineStateGL.h
	interface/RenderDeviceFactoryOpenGL.h
	interface/RenderDeviceGL.h
	interface/SamplerGL.h
	interface/ShaderGL.h
	interface/ShaderResourceBindingGL.h
	interface/SwapChainGL.h
	interface/TextureGL.h
	interface/TextureViewGL.h
)

set(SOURCE 
	src/BufferGLImpl.cpp
	src/BufferViewGLImpl.cpp
	src/DeviceContextGLImpl.cpp
	src/FBOCache.cpp
	src/GLContextState.cpp
	src/GLObjectWrapper.cpp
	src/GLProgram.cpp
	src/GLProgramResources.cpp
	src/GLTypeConversions.cpp
	src/PipelineStateGLImpl.cpp
	src/RenderDeviceFactoryOpenGL.cpp
	src/RenderDeviceGLImpl.cpp
	src/SamplerGLImpl.cpp
	src/ShaderGLImpl.cpp
	src/ShaderResourceBindingGLImpl.cpp
	src/SwapChainGLImpl.cpp
	src/TexRegionRender.cpp
	src/Texture1D_OGL.cpp
	src/Texture1DArray_OGL.cpp
	src/Texture2D_OGL.cpp
	src/Texture2DArray_OGL.cpp
	src/Texture3D_OGL.cpp
	src/TextureBaseGL.cpp
	src/TextureCube_OGL.cpp
	src/TextureCubeArray_OGL.cpp
	src/TextureViewGLImpl.cpp
	src/VAOCache.cpp
)

if(PLATFORM_WIN32)
	list(APPEND SOURCE src/GLContextWindows.cpp)
	list(APPEND INCLUDE include/GLContextWindows.h)
elseif(PLATFORM_ANDROID)
	list(APPEND SOURCE src/GLContextAndroid.cpp)
	list(APPEND SOURCE src/RenderDeviceGLESImpl.cpp)
	list(APPEND SOURCE src/GLStubs.cpp)

	list(APPEND INCLUDE include/GLContextAndroid.h)
	list(APPEND INCLUDE include/GLStubs.h)
	list(APPEND INCLUDE include/RenderDeviceGLESImpl.h)

	list(APPEND INTERFACE interface/RenderDeviceGLES.h)
elseif(PLATFORM_LINUX)
	list(APPEND SOURCE src/GLContextLinux.cpp)
	list(APPEND INCLUDE include/GLContextLinux.h)
else()
	message(FATAL_ERROR "Unknown platform")
endif()

add_library(GraphicsEngineOpenGLInterface INTERFACE)
target_include_directories(GraphicsEngineOpenGLInterface
INTERFACE
	interface
)
target_link_libraries(GraphicsEngineOpenGLInterface 
INTERFACE 
	GraphicsEngineInterface
)


add_library(GraphicsEngineOpenGL-static STATIC 
	${SOURCE} ${INTERFACE} ${INCLUDE}
	readme.md
)

add_library(GraphicsEngineOpenGL-shared SHARED 
	${SOURCE} ${INTERFACE} ${INCLUDE}
	readme.md
)
if(PLATFORM_WIN32)
	target_sources(GraphicsEngineOpenGL-shared 
	PRIVATE	
		src/DLLMain.cpp
		src/GraphicsEngineOpenGL.def
	)
endif()

target_include_directories(GraphicsEngineOpenGL-static
PRIVATE
	include
	../HLSL2GLSLConverterLib/include
)

target_include_directories(GraphicsEngineOpenGL-shared
PRIVATE
	include
	../HLSL2GLSLConverterLib/include
)

set(PRIVATE_DEPENDENCIES 
	BuildSettings 
	Common 
	TargetPlatform
	GraphicsEngine
)

set(PUBLIC_DEPENDENCIES 
	GraphicsEngineOpenGLInterface
	HLSL2GLSLConverterLib
)

if(PLATFORM_WIN32)
	set(PRIVATE_DEPENDENCIES ${PRIVATE_DEPENDENCIES} glew-static opengl32.lib)
elseif(PLATFORM_ANDROID)
	set(PRIVATE_DEPENDENCIES ${PRIVATE_DEPENDENCIES} GLESv3 EGL android)
	target_compile_definitions(GraphicsEngineOpenGL-static PRIVATE BUILDING_DLL USE_GL3_STUB=0)
	target_compile_definitions(GraphicsEngineOpenGL-shared PRIVATE BUILDING_DLL USE_GL3_STUB=0)
	set_target_properties(GraphicsEngineOpenGL-shared PROPERTIES CXX_VISIBILITY_PRESET hidden) # -fvisibility=hidden
elseif(PLATFORM_LINUX)
	set(PRIVATE_DEPENDENCIES ${PRIVATE_DEPENDENCIES} glew-static)
else()
	message(FATAL_ERROR "Unknown platform")
endif()

target_link_libraries(GraphicsEngineOpenGL-static PRIVATE ${PRIVATE_DEPENDENCIES} PUBLIC ${PUBLIC_DEPENDENCIES})
target_link_libraries(GraphicsEngineOpenGL-shared PRIVATE ${PRIVATE_DEPENDENCIES} PUBLIC ${PUBLIC_DEPENDENCIES})
target_compile_definitions(GraphicsEngineOpenGL-shared PUBLIC ENGINE_DLL)

if(PLATFORM_WIN32)
	set_target_properties(GraphicsEngineOpenGL-shared PROPERTIES
		OUTPUT_NAME_DEBUG GraphicsEngineOpenGL${DLL_DBG_SUFFIX}
		OUTPUT_NAME_RELEASE GraphicsEngineOpenGL${DLL_REL_SUFFIX}
		OUTPUT_NAME_RELWITHDEBINFO GraphicsEngineOpenGL${DLL_REL_SUFFIX}
		OUTPUT_NAME_MINSIZEREL GraphicsEngineOpenGL${DLL_REL_SUFFIX}
	)
else()
	set_target_properties(GraphicsEngineOpenGL-shared PROPERTIES
		OUTPUT_NAME GraphicsEngineOpenGL
	)
endif()

set_common_target_properties(GraphicsEngineOpenGL-shared)
set_common_target_properties(GraphicsEngineOpenGL-static)

source_group("src" FILES ${SOURCE})
if(PLATFORM_WIN32)
	source_group("dll" FILES 
		src/DLLMain.cpp
		src/GraphicsEngineOpenGL.def
	)
endif()

source_group("include" FILES ${INCLUDE})
source_group("interface" FILES ${INTERFACE})

set_target_properties(GraphicsEngineOpenGL-static PROPERTIES
	FOLDER Core/Graphics
)
set_target_properties(GraphicsEngineOpenGL-shared PROPERTIES
	FOLDER Core/Graphics
)

set_source_files_properties(
    readme.md PROPERTIES HEADER_FILE_ONLY TRUE
)
