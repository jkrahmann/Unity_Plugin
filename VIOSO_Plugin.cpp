// VIOSO_Plugin.cpp : Defines the exported functions for the DLL application.
#if _MSC_VER

#define UNITY_WIN 1
#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#include <windows.h>
#include <d3d11.h>

#elif defined(__APPLE__)

#if defined(__arm__) || defined(__arm64__)
#define UNITY_IPHONE 1
#else
#define UNITY_OSX 1
#endif

#elif defined(__ANDROID__)

#define UNITY_ANDROID 1

#elif defined(UNITY_METRO) || defined(UNITY_LINUX) || defined(UNITY_WEBGL)
	// these are defined externally
#elif defined(__EMSCRIPTEN__)
	// this is already defined in Unity 5.6
#define UNITY_WEBGL 1
#else
#error "Unknown platform!"
#endif

#include <map>
#include <string>
using namespace std;

#define VIOSOWARPBLEND_FILE s_VIOSOWarperPath
#define VIOSOWARPBLEND_DYNAMIC_DEFINE_IMPLEMENT
#include "../VIOSOWarpBlend/Include/VIOSOWarpBlend.h"
#include <stddef.h>
#include "Unity/IUnityGraphicsD3D11.h"
#include "Unity/IUnityGraphics.h"


/// definitions

// the warper collection
typedef struct UniqueWarper
{
	void* texHandle;
	VWB_Warper* pWarper;
	string name;
	void* pDxDevice;
	VWB_ERROR err;
	UniqueWarper() 
		: texHandle( VWB_UNDEFINED_GL_TEXTURE )
		, pWarper( NULL )
		, name( "" )
		, pDxDevice( NULL )
		, err( VWB_ERROR_FALSE ) 
	{}
	UniqueWarper( UniqueWarper const& other ) 
		: texHandle( other.texHandle )
		, pWarper( other.pWarper )
		, name( other.name )
		, pDxDevice( other.pDxDevice )
		, err( other.err )
	{}
	UniqueWarper( VWB_Warper* w, void* texSrc, string n, void* dev )
		: texHandle( texSrc )
		, pWarper( w )
		, name( n )
		, pDxDevice( dev )
		, err( VWB_ERROR_FALSE )
	{}
} UniqueWarper;
typedef map< int, UniqueWarper > WarperMap;

// the log file path
static char s_logFile[MAX_PATH] = "vioso.log";
static char s_VIOSOWarperPath[MAX_PATH] =
#ifdef _M_IX86
"VIOSOWarpBlend";
#else
"VIOSOWarpBlend64";
#endif

// the dll's module handle
static HMODULE s_hM = 0;

// the path to the config file
static const char* s_configFile = "VIOSOWarpBlend.ini";

// the interfaces, abstract handle addressing unity world
static IUnityInterfaces* s_UnityInterfaces = NULL;

// the unity graphics interface; here you get device and etc. from
static IUnityGraphics* s_Graphics = NULL;

// the device type, determines, what flavour unity is using
static UnityGfxRenderer s_DeviceType = kUnityGfxRendererNull;

// this is the map of all warpers
static WarperMap s_warpers;

// the global time
static float s_Time;


//////////////////// logging ////////////////////////
_inline VWB_ERROR VWB_logString( const char* logFilePath, int level, char const* str, bool clear = false )
{
	if( NULL != VWB__logString )
		VWB__logString( level, str );
	else
	{
		FILE* f;
		{
			if( NOERROR == fopen_s( &f, logFilePath, clear ? "w" : "a" ) )
			{
				fprintf( f, str );
				fclose( f );
			}
			return VWB_ERROR_NONE;
		}
	}
	return VWB_ERROR_GENERIC;
}

static void UNITY_INTERFACE_API OnGraphicsDeviceEvent( UnityGfxDeviceEventType eventType )
{
	// Create graphics API implementation upon initialization
	if( eventType == kUnityGfxDeviceEventInitialize )
	{
		VWB_logString( s_logFile,2, "VIOSO Unity Plugin kUnityGfxDeviceEventInitialize\n" );
	}
	// Cleanup graphics API implementation upon shutdown
	else if( eventType == kUnityGfxDeviceEventShutdown )
	{
		VWB_logString( s_logFile, 2, "VIOSO Unity Plugin kUnityGfxDeviceEventShutdown\n" );
	}
	else
	{
		VWB_logString( s_logFile,2, "VIOSO Unity Plugin OTHER DEVICE event\n" );
	}

}

static void UNITY_INTERFACE_API OnRenderEvent( int eventID )
{
 	auto it = s_warpers.find( eventID );
	if( s_warpers.end() != it )
	{
		if( NULL == it->second.pWarper )
		{

			VWB_ERROR err = VWB_Create( it->second.pDxDevice, s_configFile, it->second.name.c_str(), &it->second.pWarper, 2, s_logFile );
			if( VWB_ERROR_NONE == err )
			{
				it->second.pWarper->bFlipDXVs = true;
			}
			if( VWB_ERROR_NONE == err &&
				VWB_ERROR_NONE == ( err = VWB_Init( it->second.pWarper ) ) )
			{
				char b[MAX_PATH];
				sprintf_s( b, "Successfully created warper #%i\n", eventID );
				VWB_logString( s_logFile, 2, b );
			}
			else
			{
				if( NULL != it->second.pWarper )
					VWB_Destroy( it->second.pWarper );
				s_warpers.erase( it );
				VWB_logString( s_logFile, 0, "Failed to create warper #%i\n", eventID );
			}
			it->second.err = err;
		}
		else
		{
			VWB_ERROR err = VWB_render( it->second.pWarper, it->second.texHandle, VWB_STATEMASK_RASTERSTATE );
			if( VWB_ERROR_NONE != err )
			{
				VWB_logString( s_logFile, 1, "Error: VWB_render.\n" );
			}
			it->second.err = err;
		}
	}
	else
	{
		VWB_logString( s_logFile, 2, "Error: OnRenderEvent UNKNOWN.\n" );
	}
}

// exposed functions to UNITY core
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginLoad( IUnityInterfaces* unityInterfaces )
{
	VWB_logString( s_logFile,2, "VIOSO Unity Plugin PluginLoad" );
	s_UnityInterfaces = unityInterfaces;
	s_Graphics = s_UnityInterfaces->Get<IUnityGraphics>();
	s_DeviceType = s_Graphics->GetRenderer();

	s_Graphics->RegisterDeviceEventCallback( OnGraphicsDeviceEvent );
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginUnload()
{
	s_Graphics->UnregisterDeviceEventCallback( OnGraphicsDeviceEvent );
	s_Graphics = NULL;
	s_UnityInterfaces = NULL;
	s_DeviceType = kUnityGfxRendererNull;
	VWB_logString( s_logFile,2, "VIOSO Unity Plugin PluginUnload" );
}

extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetRenderEventFunc()
{
	return (UnityRenderingEvent)OnRenderEvent;
}

extern "C" VWB_ERROR UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API Init( int* pChannelID, LPCSTR name )
{
	VWB_ERROR err = VWB_ERROR_NONE;
	if( NULL == pChannelID || NULL == name || 0 == name[0] )
		return VWB_ERROR_PARAMETER;

	if( s_warpers.empty() )
	{
		VWB_logString( s_logFile, 2, "\n Loading VIOSOWarpBlend ...\n" );

		char n[MAX_PATH];
		GetModuleFileName( NULL, n, MAX_PATH );
		VWB_logString( s_logFile, 2, "Process location: " );
		VWB_logString( s_logFile, 2, n );
		VWB_logString( s_logFile, 2, "\nPlugin location: " );
		GetModuleFileName( s_hM, n, MAX_PATH );
		VWB_logString( s_logFile, 2, n );
		GetModuleFileName( s_hM, s_VIOSOWarperPath, MAX_PATH );
		char* s = strrchr( s_VIOSOWarperPath, '\\' );
		if( NULL != s )
			s[0] = 0;
		#if defined( _M_IX86 )
		strcat_s( s_VIOSOWarperPath, "\\VIOSOWarpBlend.dll" );
		#else
		strcat_s( s_VIOSOWarperPath, "\\VIOSOWarpBlend64.dll" );
		#endif
		VWB_logString( s_logFile, 2, "\nTry VIOSOWarpBlend location: " );
		VWB_logString( s_logFile, 2, s_VIOSOWarperPath );
		VWB_logString( s_logFile, 2, "\n" );
		#define VIOSOWARPBLEND_DYNAMIC_INITIALIZE
		#include "../VIOSOWarpBlend/Include/VIOSOWarpBlend.h"
		if( NULL == VWB_Create )
		{
			VWB_logString( s_logFile, 2, "\n FATAL ERROR: Failed to load VIOSOWarpBlend\n" );
			return VWB_ERROR_GENERIC;
		}
		else
		{
			VWB_logString( s_logFile, 2, "\nSUCCESS: VIOSOWarpBlend loaded.\n" );
		}
	}
	else
	{
		// check, if name is already used...
		for( auto it = s_warpers.begin(); it != s_warpers.end(); it++ )
		{
			if( 0 == strcmp( it->second.name.c_str(), name ) )
			{
				VWB_logString( s_logFile, 2, name );
				VWB_logString( s_logFile, 0, "FATAL ERROR: name already in use.\n" );
				return VWB_ERROR_GENERIC;
			}
		}

	}

	// find free id..
	*pChannelID = 0;
	while( 1 )
	{
		auto it = s_warpers.begin();
		for( ; it != s_warpers.end(); it++ )
		{
			if( it->first == *pChannelID )
			{
				( *pChannelID )++;
				break;
			}
		}
		if( it == s_warpers.end() )
			break;
	}

	char logb[MAX_PATH] = { 0 };
	sprintf_s( logb, "Warper create issued \"%s\" as #%i", name, *pChannelID );
	VWB_logString( s_logFile, 2, logb );

	void* pDxDevice = NULL;
	s_DeviceType = s_Graphics->GetRenderer();
	switch( s_DeviceType )
	{
	case kUnityGfxRendererD3D11:
		{
			IUnityGraphicsD3D11* d3d = s_UnityInterfaces->Get<IUnityGraphicsD3D11>();
			if( NULL != d3d && NULL != ( pDxDevice = d3d->GetDevice() ) )
			{
				VWB_logString( s_logFile,0, " D3D11 - Device found.\n" );
			}
			else
			{
				VWB_logString( s_logFile, 0, "\nERROR: Missing D3D11 device!\n" );
				return VWB_ERROR_GENERIC;
			}
			break;
		}
	case kUnityGfxRendererOpenGLCore:
		{
			VWB_logString( s_logFile, 2, " OpenGLCore\n" );
			break;
		}
	default:
		{
			VWB_logString( s_logFile,0, "\nERROR: unsupported graphic engine.\n" );
			return VWB_ERROR_GENERIC;
		}
	}

	UniqueWarper& w = s_warpers[*pChannelID];

	w.pDxDevice = pDxDevice;
	w.name = name;
	w.err = err;

	return err;
}

extern "C" VWB_ERROR UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API Destroy( int channelID )
{
	VWB_ERROR err = VWB_ERROR_NONE;
	auto it = s_warpers.find( channelID);
	if( s_warpers.end() != it )
	{
		VWB_Destroy( it->second.pWarper );
		s_warpers.erase( it );
		VWB_logString( s_logFile, 2, "Warper destroyed." );
	}
	else
	{
		VWB_logString( s_logFile, 1, "Warning: tried to destroy unknown warper ID." );
		err = VWB_ERROR_PARAMETER;
	}
	if( s_warpers.empty() )
	{
		VWB_logString( s_logFile, 2, "Unloading VIOSOWarpBlend\n" );

		#define VIOSOWARPBLEND_DYNAMIC_UNINITIALIZE
		#include "../VIOSOWarpBlend/Include/VIOSOWarpBlend.h"
	}
	return err;
}

extern "C" VWB_ERROR UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetError( int channelID, int* err )
{
	if( NULL == err )
		return VWB_ERROR_PARAMETER;

	auto it = s_warpers.find( channelID );
	if( s_warpers.end() != it )
	{
		*err = it->second.err;
		return VWB_ERROR_NONE;
	}
	return VWB_ERROR_PARAMETER;
}

extern "C" VWB_ERROR UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UpdateTex( int channelID, void* texHandle )
{
	auto it = s_warpers.find( channelID );
	if( s_warpers.end() != it )
	{
		it->second.texHandle = texHandle;
		return VWB_ERROR_NONE;
	}
	return VWB_ERROR_GENERIC;
}
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetTimeFromUnity( float t )
{
	s_Time = t;
}

extern "C" int UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetViewClip( int channelID, float* pEye, float* pRot, float* pView, float* pClip )
{
	auto it = s_warpers.find( channelID );
	if( s_warpers.end() != it )
	{
		VWB_ERROR err = VWB_getViewClip( it->second.pWarper, pEye, pRot, pView, pClip ); // pClip is left, top, right, bottom, all outward distances; unity wants left, right, bottom, top in coordinates
		pClip[0] *= -1; // reverse left
		float t = pClip[1]; // save VIOSO's top
		pClip[1] = pClip[2]; // Unity's right is VIOSO's right
		pClip[2] = -t; // Unity's bottom is reversed VIOSO's top (because of reversed v texture coordinate)
		return err;
	}
	else
	{
		VWB_logString( s_logFile, 1, "Warning: GetViewClip tried to address unknown warper ID." );
	}
	return VWB_ERROR_GENERIC;
}

extern "C" int UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetViewProj( int channelID, float* pEye, float* pRot, float* pView, float* pProj )
{
	auto it = s_warpers.find( channelID );
	if( s_warpers.end() != it )
	{
		VWB_ERROR err = VWB_getViewProj( it->second.pWarper, pEye, pRot, pView, pProj ); // pClip is left, top, right, bottom, all outward distances; unity wants left, right, bottom, top in coordinates
		return err;
	}
	else
	{
		VWB_logString( s_logFile, 1, "Warning: GetViewViewProj tried to address unknown warper ID." );
	}
	return VWB_ERROR_GENERIC;
}

BOOL APIENTRY DllMain( HMODULE hModule,
					   DWORD  ul_reason_for_call,
					   LPVOID lpReserved
)
{
	switch( ul_reason_for_call )
	{
	case DLL_PROCESS_ATTACH:
		s_hM = hModule;
		GetModuleFileNameA( s_hM, s_logFile, MAX_PATH );
		if( char* dot = strrchr( s_logFile, '.' ) )
		{
			*dot = 0;
		}
		strcat_s( s_logFile, ".log" );
		VWB_logString( s_logFile, 2, "\n\nVIOSO Unity Plugin loaded\n", true );
		break;
	case DLL_THREAD_ATTACH:
		break;
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
		VWB_logString( s_logFile,2, "VIOSO Unity Plugin to unload" );
		break;
	}
	return TRUE;
}

