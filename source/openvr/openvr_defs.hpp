#pragma once

namespace vr {

enum EVRInitError
{
	VRInitError_None	= 0,
	VRInitError_Unknown = 1,

	VRInitError_Init_InstallationNotFound			= 100,
	VRInitError_Init_InstallationCorrupt			= 101,
	VRInitError_Init_VRClientDLLNotFound			= 102,
	VRInitError_Init_FileNotFound					= 103,
	VRInitError_Init_FactoryNotFound				= 104,
	VRInitError_Init_InterfaceNotFound				= 105,
	VRInitError_Init_InvalidInterface				= 106,
	VRInitError_Init_UserConfigDirectoryInvalid		= 107,
	VRInitError_Init_HmdNotFound					= 108,
	VRInitError_Init_NotInitialized					= 109,
	VRInitError_Init_PathRegistryNotFound			= 110,
	VRInitError_Init_NoConfigPath					= 111,
	VRInitError_Init_NoLogPath						= 112,
	VRInitError_Init_PathRegistryNotWritable		= 113,
	VRInitError_Init_AppInfoInitFailed				= 114,
	VRInitError_Init_Retry							= 115, // Used internally to cause retries to vrserver
	VRInitError_Init_InitCanceledByUser				= 116, // The calling application should silently exit. The user canceled app startup
	VRInitError_Init_AnotherAppLaunching			= 117, 
	VRInitError_Init_SettingsInitFailed				= 118, 
	VRInitError_Init_ShuttingDown					= 119,
	VRInitError_Init_TooManyObjects					= 120,
	VRInitError_Init_NoServerForBackgroundApp		= 121,
	VRInitError_Init_NotSupportedWithCompositor		= 122,
	VRInitError_Init_NotAvailableToUtilityApps		= 123,
	VRInitError_Init_Internal				 		= 124,
	VRInitError_Init_HmdDriverIdIsNone		 		= 125,
	VRInitError_Init_HmdNotFoundPresenceFailed 		= 126,
	VRInitError_Init_VRMonitorNotFound				= 127,
	VRInitError_Init_VRMonitorStartupFailed			= 128,
	VRInitError_Init_LowPowerWatchdogNotSupported	= 129, 
	VRInitError_Init_InvalidApplicationType			= 130,
	VRInitError_Init_NotAvailableToWatchdogApps		= 131,
	VRInitError_Init_WatchdogDisabledInSettings		= 132,
	VRInitError_Init_VRDashboardNotFound			= 133,
	VRInitError_Init_VRDashboardStartupFailed		= 134,
	VRInitError_Init_VRHomeNotFound					= 135,
	VRInitError_Init_VRHomeStartupFailed			= 136,
	VRInitError_Init_RebootingBusy					= 137,
	VRInitError_Init_FirmwareUpdateBusy				= 138,
	VRInitError_Init_FirmwareRecoveryBusy			= 139,
	VRInitError_Init_USBServiceBusy					= 140,
	VRInitError_Init_VRWebHelperStartupFailed		= 141,
	VRInitError_Init_TrackerManagerInitFailed		= 142,
	VRInitError_Init_AlreadyRunning					= 143,
	VRInitError_Init_FailedForVrMonitor				= 144,
	VRInitError_Init_PropertyManagerInitFailed		= 145,
	VRInitError_Init_WebServerFailed				= 146,

	VRInitError_Driver_Failed						= 200,
	VRInitError_Driver_Unknown						= 201,
	VRInitError_Driver_HmdUnknown					= 202,
	VRInitError_Driver_NotLoaded					= 203,
	VRInitError_Driver_RuntimeOutOfDate				= 204,
	VRInitError_Driver_HmdInUse						= 205,
	VRInitError_Driver_NotCalibrated				= 206,
	VRInitError_Driver_CalibrationInvalid			= 207,
	VRInitError_Driver_HmdDisplayNotFound			= 208,
	VRInitError_Driver_TrackedDeviceInterfaceUnknown = 209,
	// VRInitError_Driver_HmdDisplayNotFoundAfterFix = 210, // not needed: here for historic reasons
	VRInitError_Driver_HmdDriverIdOutOfBounds		= 211,
	VRInitError_Driver_HmdDisplayMirrored			= 212,
	VRInitError_Driver_HmdDisplayNotFoundLaptop		= 213,
	// Never make error 259 because we return it from main and it would conflict with STILL_ACTIVE

	VRInitError_IPC_ServerInitFailed				= 300,
	VRInitError_IPC_ConnectFailed					= 301,
	VRInitError_IPC_SharedStateInitFailed			= 302,
	VRInitError_IPC_CompositorInitFailed			= 303,
	VRInitError_IPC_MutexInitFailed					= 304,
	VRInitError_IPC_Failed							= 305,
	VRInitError_IPC_CompositorConnectFailed			= 306,
	VRInitError_IPC_CompositorInvalidConnectResponse = 307,
	VRInitError_IPC_ConnectFailedAfterMultipleAttempts = 308,
	VRInitError_IPC_ConnectFailedAfterTargetExited = 309,
	VRInitError_IPC_NamespaceUnavailable			 = 310,

	VRInitError_Compositor_Failed												= 400,
	VRInitError_Compositor_D3D11HardwareRequired								= 401,
	VRInitError_Compositor_FirmwareRequiresUpdate								= 402,
	VRInitError_Compositor_OverlayInitFailed									= 403,
	VRInitError_Compositor_ScreenshotsInitFailed								= 404,
	VRInitError_Compositor_UnableToCreateDevice									= 405,
	VRInitError_Compositor_SharedStateIsNull									= 406,
	VRInitError_Compositor_NotificationManagerIsNull							= 407,
	VRInitError_Compositor_ResourceManagerClientIsNull							= 408,
	VRInitError_Compositor_MessageOverlaySharedStateInitFailure					= 409,
	VRInitError_Compositor_PropertiesInterfaceIsNull							= 410,
	VRInitError_Compositor_CreateFullscreenWindowFailed							= 411,
	VRInitError_Compositor_SettingsInterfaceIsNull								= 412,
	VRInitError_Compositor_FailedToShowWindow									= 413,
	VRInitError_Compositor_DistortInterfaceIsNull								= 414,
	VRInitError_Compositor_DisplayFrequencyFailure								= 415,
	VRInitError_Compositor_RendererInitializationFailed							= 416,
	VRInitError_Compositor_DXGIFactoryInterfaceIsNull							= 417,
	VRInitError_Compositor_DXGIFactoryCreateFailed								= 418,
	VRInitError_Compositor_DXGIFactoryQueryFailed								= 419,
	VRInitError_Compositor_InvalidAdapterDesktop								= 420,
	VRInitError_Compositor_InvalidHmdAttachment									= 421,
	VRInitError_Compositor_InvalidOutputDesktop									= 422,
	VRInitError_Compositor_InvalidDeviceProvided								= 423,
	VRInitError_Compositor_D3D11RendererInitializationFailed					= 424,
	VRInitError_Compositor_FailedToFindDisplayMode								= 425,
	VRInitError_Compositor_FailedToCreateSwapChain								= 426,
	VRInitError_Compositor_FailedToGetBackBuffer								= 427,
	VRInitError_Compositor_FailedToCreateRenderTarget							= 428,
	VRInitError_Compositor_FailedToCreateDXGI2SwapChain							= 429,
	VRInitError_Compositor_FailedtoGetDXGI2BackBuffer							= 430,
	VRInitError_Compositor_FailedToCreateDXGI2RenderTarget						= 431,
	VRInitError_Compositor_FailedToGetDXGIDeviceInterface						= 432,
	VRInitError_Compositor_SelectDisplayMode									= 433,
	VRInitError_Compositor_FailedToCreateNvAPIRenderTargets						= 434,
	VRInitError_Compositor_NvAPISetDisplayMode									= 435,
	VRInitError_Compositor_FailedToCreateDirectModeDisplay						= 436,
	VRInitError_Compositor_InvalidHmdPropertyContainer							= 437,
	VRInitError_Compositor_UpdateDisplayFrequency								= 438,
	VRInitError_Compositor_CreateRasterizerState								= 439,
	VRInitError_Compositor_CreateWireframeRasterizerState						= 440,
	VRInitError_Compositor_CreateSamplerState									= 441,
	VRInitError_Compositor_CreateClampToBorderSamplerState						= 442,
	VRInitError_Compositor_CreateAnisoSamplerState								= 443,
	VRInitError_Compositor_CreateOverlaySamplerState							= 444,
	VRInitError_Compositor_CreatePanoramaSamplerState							= 445,
	VRInitError_Compositor_CreateFontSamplerState								= 446,
	VRInitError_Compositor_CreateNoBlendState									= 447,
	VRInitError_Compositor_CreateBlendState										= 448,
	VRInitError_Compositor_CreateAlphaBlendState								= 449,
	VRInitError_Compositor_CreateBlendStateMaskR								= 450,
	VRInitError_Compositor_CreateBlendStateMaskG								= 451,
	VRInitError_Compositor_CreateBlendStateMaskB								= 452,
	VRInitError_Compositor_CreateDepthStencilState								= 453,
	VRInitError_Compositor_CreateDepthStencilStateNoWrite						= 454,
	VRInitError_Compositor_CreateDepthStencilStateNoDepth						= 455,
	VRInitError_Compositor_CreateFlushTexture									= 456,
	VRInitError_Compositor_CreateDistortionSurfaces								= 457,
	VRInitError_Compositor_CreateConstantBuffer									= 458,
	VRInitError_Compositor_CreateHmdPoseConstantBuffer							= 459,
	VRInitError_Compositor_CreateHmdPoseStagingConstantBuffer					= 460,
	VRInitError_Compositor_CreateSharedFrameInfoConstantBuffer					= 461,
	VRInitError_Compositor_CreateOverlayConstantBuffer							= 462,
	VRInitError_Compositor_CreateSceneTextureIndexConstantBuffer				= 463,
	VRInitError_Compositor_CreateReadableSceneTextureIndexConstantBuffer		= 464,
	VRInitError_Compositor_CreateLayerGraphicsTextureIndexConstantBuffer		= 465,
	VRInitError_Compositor_CreateLayerComputeTextureIndexConstantBuffer			= 466,
	VRInitError_Compositor_CreateLayerComputeSceneTextureIndexConstantBuffer	= 467,
	VRInitError_Compositor_CreateComputeHmdPoseConstantBuffer					= 468,
	VRInitError_Compositor_CreateGeomConstantBuffer								= 469,
	VRInitError_Compositor_CreatePanelMaskConstantBuffer						= 470,
	VRInitError_Compositor_CreatePixelSimUBO									= 471,
	VRInitError_Compositor_CreateMSAARenderTextures								= 472,
	VRInitError_Compositor_CreateResolveRenderTextures							= 473,
	VRInitError_Compositor_CreateComputeResolveRenderTextures					= 474,
	VRInitError_Compositor_CreateDriverDirectModeResolveTextures				= 475,
	VRInitError_Compositor_OpenDriverDirectModeResolveTextures					= 476,
	VRInitError_Compositor_CreateFallbackSyncTexture							= 477,
	VRInitError_Compositor_ShareFallbackSyncTexture								= 478,
	VRInitError_Compositor_CreateOverlayIndexBuffer								= 479,
	VRInitError_Compositor_CreateOverlayVertexBuffer							= 480,
	VRInitError_Compositor_CreateTextVertexBuffer								= 481,
	VRInitError_Compositor_CreateTextIndexBuffer								= 482,
	VRInitError_Compositor_CreateMirrorTextures									= 483,
	VRInitError_Compositor_CreateLastFrameRenderTexture							= 484,
	VRInitError_Compositor_CreateMirrorOverlay									= 485,
	VRInitError_Compositor_FailedToCreateVirtualDisplayBackbuffer				= 486,
	VRInitError_Compositor_DisplayModeNotSupported								= 487,
	VRInitError_Compositor_CreateOverlayInvalidCall								= 488,
	VRInitError_Compositor_CreateOverlayAlreadyInitialized						= 489,
	VRInitError_Compositor_FailedToCreateMailbox								= 490,
	
	VRInitError_VendorSpecific_UnableToConnectToOculusRuntime		= 1000,
	VRInitError_VendorSpecific_WindowsNotInDevMode					= 1001,

	VRInitError_VendorSpecific_HmdFound_CantOpenDevice 				= 1101,
	VRInitError_VendorSpecific_HmdFound_UnableToRequestConfigStart	= 1102,
	VRInitError_VendorSpecific_HmdFound_NoStoredConfig 				= 1103,
	VRInitError_VendorSpecific_HmdFound_ConfigTooBig 				= 1104,
	VRInitError_VendorSpecific_HmdFound_ConfigTooSmall 				= 1105,
	VRInitError_VendorSpecific_HmdFound_UnableToInitZLib 			= 1106,
	VRInitError_VendorSpecific_HmdFound_CantReadFirmwareVersion 	= 1107,
	VRInitError_VendorSpecific_HmdFound_UnableToSendUserDataStart	= 1108,
	VRInitError_VendorSpecific_HmdFound_UnableToGetUserDataStart	= 1109,
	VRInitError_VendorSpecific_HmdFound_UnableToGetUserDataNext		= 1110,
	VRInitError_VendorSpecific_HmdFound_UserDataAddressRange		= 1111,
	VRInitError_VendorSpecific_HmdFound_UserDataError				= 1112,
	VRInitError_VendorSpecific_HmdFound_ConfigFailedSanityCheck		= 1113,
	VRInitError_VendorSpecific_OculusRuntimeBadInstall				= 1114,

	VRInitError_Steam_SteamInstallationNotFound = 2000,

	// Strictly a placeholder
	VRInitError_LastError
};

enum EVRCompositorError
{
	VRCompositorError_None						= 0,
	VRCompositorError_RequestFailed				= 1,
	VRCompositorError_IncompatibleVersion		= 100,
	VRCompositorError_DoNotHaveFocus			= 101,
	VRCompositorError_InvalidTexture			= 102,
	VRCompositorError_IsNotSceneApplication		= 103,
	VRCompositorError_TextureIsOnWrongDevice	= 104,
	VRCompositorError_TextureUsesUnsupportedFormat = 105,
	VRCompositorError_SharedTexturesNotSupported = 106,
	VRCompositorError_IndexOutOfRange			= 107,
	VRCompositorError_AlreadySubmitted			= 108,
	VRCompositorError_InvalidBounds				= 109,
};

enum EVREye
{
	Eye_Left = 0,
	Eye_Right = 1
};

enum ETextureType
{
	TextureType_Invalid = -1, // Handle has been invalidated
	TextureType_DirectX = 0, // Handle is an ID3D11Texture
	TextureType_OpenGL = 1,  // Handle is an OpenGL texture name or an OpenGL render buffer name, depending on submit flags
	TextureType_Vulkan = 2, // Handle is a pointer to a VRVulkanTextureData_t structure
	TextureType_IOSurface = 3, // Handle is a macOS cross-process-sharable IOSurfaceRef, deprecated in favor of TextureType_Metal on supported platforms
	TextureType_DirectX12 = 4, // Handle is a pointer to a D3D12TextureData_t structure
	TextureType_DXGISharedHandle = 5, // Handle is a HANDLE DXGI share handle, only supported for Overlay render targets. 
									  // this texture is used directly by our renderer, so only perform atomic (copyresource or resolve) on it
	TextureType_Metal = 6, // Handle is a MTLTexture conforming to the MTLSharedTexture protocol. Textures submitted to IVRCompositor::Submit which
						   // are of type MTLTextureType2DArray assume layer 0 is the left eye texture (vr::EVREye::Eye_left), layer 1 is the right
						   // eye texture (vr::EVREye::Eye_Right)
};

enum EColorSpace
{
	ColorSpace_Auto = 0,	// Assumes 'gamma' for 8-bit per component formats, otherwise 'linear'.  This mirrors the DXGI formats which have _SRGB variants.
	ColorSpace_Gamma = 1,	// Texture data can be displayed directly on the display without any conversion (a.k.a. display native format).
	ColorSpace_Linear = 2,	// Same as gamma but has been converted to a linear representation using DXGI's sRGB conversion algorithm.
};

struct Texture_t
{
	void* handle; // See ETextureType definition above
	ETextureType eType;
	EColorSpace eColorSpace;
};

/** Allows the application to control what part of the provided texture will be used in the
* frame buffer. */
struct VRTextureBounds_t
{
	float uMin, vMin;
	float uMax, vMax;
};

/** Allows the application to control how scene textures are used by the compositor when calling Submit. */
enum EVRSubmitFlags
{
	// Simple render path. App submits rendered left and right eye images with no lens distortion correction applied.
	Submit_Default = 0x00,

	// App submits final left and right eye images with lens distortion already applied (lens distortion makes the images appear
	// barrel distorted with chromatic aberration correction applied). The app would have used the data returned by
	// vr::IVRSystem::ComputeDistortion() to apply the correct distortion to the rendered images before calling Submit().
	Submit_LensDistortionAlreadyApplied = 0x01,

	// If the texture pointer passed in is actually a renderbuffer (e.g. for MSAA in OpenGL) then set this flag.
	Submit_GlRenderBuffer = 0x02,

	// Do not use
	Submit_Reserved = 0x04,

	// Set to indicate that pTexture is a pointer to a VRTextureWithPose_t.
	// This flag can be combined with Submit_TextureWithDepth to pass a VRTextureWithPoseAndDepth_t.
	Submit_TextureWithPose = 0x08,

	// Set to indicate that pTexture is a pointer to a VRTextureWithDepth_t.
	// This flag can be combined with Submit_TextureWithPose to pass a VRTextureWithPoseAndDepth_t.
	Submit_TextureWithDepth = 0x10,
};

}
