
#include <initguid.h>
#include <SetupAPI.h>
#include <usbspec.h>
#include <WinBase.h>
#include <devioctl.h>
#include <usbioctl.h>
#include <usbuser.h>
#include <strsafe.h>
#include "usbdesc.h"
#include <usbiodef.h>

/*****************************************************************************
P R A G M A S
*****************************************************************************/

#pragma once

/*****************************************************************************
D E F I N E S
*****************************************************************************/

#define  OOPS()

#define ALLOC(dwBytes) GlobalAlloc(GPTR,(dwBytes))

#define FREE(hMem)  GlobalFree((hMem))

#define MAX_DRIVER_KEY_NAME 256
#define MAX_DEVICE_PROP 200

FORCEINLINE
VOID
InitializeListHead(
_Out_ PLIST_ENTRY ListHead
)
{
	ListHead->Flink = ListHead->Blink = ListHead;
}

#define InsertTailList(ListHead,Entry) {\
	PLIST_ENTRY _EX_Blink; \
	PLIST_ENTRY _EX_ListHead; \
	_EX_ListHead = (ListHead); \
	_EX_Blink = _EX_ListHead->Blink; \
	(Entry)->Flink = _EX_ListHead; \
	(Entry)->Blink = _EX_Blink; \
	_EX_Blink->Flink = (Entry); \
	_EX_ListHead->Blink = (Entry); \
}

/*****************************************************************************
G L O B A L S
*****************************************************************************/

int  TotalHubs;


/*****************************************************************************
T Y P E D E F S
*****************************************************************************/

typedef struct _USBPATH
{
	char DriverPath;
	BYTE SerialNumber[200];
	int SerialNumberSize;
}USBINFO;

typedef enum _USBDEVICEINFOTYPE
{
	HostControllerInfo,
	RootHubInfo,
	ExternalHubInfo,
	DeviceInfo
} USBDEVICEINFOTYPE, *PUSBDEVICEINFOTYPE;

typedef enum _TREEICON
{
	ComputerIcon,
	HubIcon,
	NoDeviceIcon,
	GoodDeviceIcon,
	BadDeviceIcon,
	GoodSsDeviceIcon,
	NoSsDeviceIcon
} TREEICON;

typedef struct _USB_DEVICE_PNP_STRINGS
{
	PCHAR DeviceId;
	PCHAR DeviceDesc;
	PCHAR HwId;
	PCHAR Service;
	PCHAR DeviceClass;
	PCHAR PowerState;
} USB_DEVICE_PNP_STRINGS, *PUSB_DEVICE_PNP_STRINGS;

typedef struct _USBHOSTCONTROLLERINFO
{
	USBDEVICEINFOTYPE                   DeviceInfoType;
	LIST_ENTRY                          ListEntry;
	PCHAR                               DriverKey;
	ULONG                               VendorID;
	ULONG                               DeviceID;
	ULONG                               SubSysID;
	ULONG                               Revision;
	USB_POWER_INFO                      USBPowerInfo[6];
	BOOL                                BusDeviceFunctionValid;
	ULONG                               BusNumber;
	USHORT                              BusDevice;
	USHORT                              BusFunction;
	PUSB_CONTROLLER_INFO_0              ControllerInfo;
	PUSB_DEVICE_PNP_STRINGS             UsbDeviceProperties;
} USBHOSTCONTROLLERINFO, *PUSBHOSTCONTROLLERINFO;

typedef struct
{
	USB_DEVICE_DESCRIPTOR			 DeviceDescriptor;
	USB_CONFIGURATION_DESCRIPTOR	 ConfigDesc;
	USB_INTERFACE_DESCRIPTOR	     InterfaceDesc;
	USB_ENDPOINT_DESCRIPTOR	         EndpointDescriptor[2];
	CHAR DeviceId[50];
	CHAR DeviceDesc[40];
	CHAR HwId[80];
	CHAR Service[20];
	CHAR DeviceClass[20];

} USBSENDDEVICEDESC, *PUSBSENDDEVICEDESC;


typedef struct _DEVICE_GUID_LIST {
	HDEVINFO   DeviceInfo;
	LIST_ENTRY ListHead;
} DEVICE_GUID_LIST, *PDEVICE_GUID_LIST;

typedef struct _STRING_DESCRIPTOR_NODE
{
	struct _STRING_DESCRIPTOR_NODE *Next;
	UCHAR                           DescriptorIndex;
	USHORT                          LanguageID;
	USB_STRING_DESCRIPTOR           StringDescriptor[1];
} STRING_DESCRIPTOR_NODE, *PSTRING_DESCRIPTOR_NODE;

typedef struct _DEVICE_INFO_NODE {
	HDEVINFO                         DeviceInfo;
	LIST_ENTRY                       ListEntry;
	SP_DEVINFO_DATA                  DeviceInfoData;
	SP_DEVICE_INTERFACE_DATA         DeviceInterfaceData;
	PSP_DEVICE_INTERFACE_DETAIL_DATA DeviceDetailData;
	PSTR                             DeviceDescName;
	ULONG                            DeviceDescNameLength;
	PSTR                             DeviceDriverName;
	ULONG                            DeviceDriverNameLength;
	DEVICE_POWER_STATE               LatestDevicePowerState;
} DEVICE_INFO_NODE, *PDEVICE_INFO_NODE;

typedef struct _USBEXTERNALHUBINFO
{
	USBDEVICEINFOTYPE                      DeviceInfoType;
	PUSB_NODE_INFORMATION                  HubInfo;
	PUSB_HUB_INFORMATION_EX                HubInfoEx;
	PCHAR                                  HubName;
	PUSB_NODE_CONNECTION_INFORMATION_EX    ConnectionInfo;
	PUSB_PORT_CONNECTOR_PROPERTIES         PortConnectorProps;
	PUSB_DESCRIPTOR_REQUEST                ConfigDesc;
	PUSB_DESCRIPTOR_REQUEST                BosDesc;
	PSTRING_DESCRIPTOR_NODE                StringDescs;
	PUSB_NODE_CONNECTION_INFORMATION_EX_V2 ConnectionInfoV2; // NULL if root HUB
	PUSB_DEVICE_PNP_STRINGS                UsbDeviceProperties;
	PDEVICE_INFO_NODE                      DeviceInfoNode;
	PUSB_HUB_CAPABILITIES_EX               HubCapabilityEx;
} USBEXTERNALHUBINFO, *PUSBEXTERNALHUBINFO;

typedef struct _USBROOTHUBINFO
{
	USBDEVICEINFOTYPE                   DeviceInfoType;
	PUSB_NODE_INFORMATION               HubInfo;
	PUSB_HUB_INFORMATION_EX             HubInfoEx;
	PCHAR                               HubName;
	PUSB_PORT_CONNECTOR_PROPERTIES      PortConnectorProps;
	PUSB_DEVICE_PNP_STRINGS             UsbDeviceProperties;
	PDEVICE_INFO_NODE                   DeviceInfoNode;
	PUSB_HUB_CAPABILITIES_EX            HubCapabilityEx;

} USBROOTHUBINFO, *PUSBROOTHUBINFO;

// HubInfo, HubName may be in USBDEVICEINFOTYPE, so they can be removed
typedef struct
{
	USBDEVICEINFOTYPE                      DeviceInfoType;
	PUSB_NODE_INFORMATION                  HubInfo;          // NULL if not a HUB
	PUSB_HUB_INFORMATION_EX                HubInfoEx;        // NULL if not a HUB
	PCHAR                                  HubName;          // NULL if not a HUB
	PUSB_NODE_CONNECTION_INFORMATION_EX    ConnectionInfo;   // NULL if root HUB
	PUSB_PORT_CONNECTOR_PROPERTIES         PortConnectorProps;
	PUSB_DESCRIPTOR_REQUEST                ConfigDesc;       // NULL if root HUB
	PUSB_DESCRIPTOR_REQUEST                BosDesc;          // NULL if root HUB
	PSTRING_DESCRIPTOR_NODE                StringDescs;
	PUSB_NODE_CONNECTION_INFORMATION_EX_V2 ConnectionInfoV2; // NULL if root HUB
	PUSB_DEVICE_PNP_STRINGS                UsbDeviceProperties;
	PDEVICE_INFO_NODE                      DeviceInfoNode;
	PUSB_HUB_CAPABILITIES_EX               HubCapabilityEx;  // NULL if not a HUB
} USBDEVICEINFO, *PUSBDEVICEINFO;



/*****************************************************************************
F U N C T I O N    P R O T O T Y P E S
*****************************************************************************/

PCHAR GetHCDDriverKeyName(
	HANDLE  HCD
	);

PCHAR WideStrToMultiStr(
	_In_reads_bytes_(cbWideStr) PWCHAR WideStr,
	_In_ size_t                   cbWideStr
	);

VOID
EnumerateHostController(
HANDLE                   hHCDev, _Inout_ PCHAR            leafName,
_In_    HANDLE           deviceInfo,
_In_    PSP_DEVINFO_DATA deviceInfoData
);

_Success_(return == TRUE)
BOOL
GetDeviceProperty(
_In_    HDEVINFO         DeviceInfoSet,
_In_    PSP_DEVINFO_DATA DeviceInfoData,
_In_    DWORD            Property,
_Outptr_ LPTSTR         *ppBuffer
);


BOOL
DriverNameToDeviceInst(
_In_reads_bytes_(cbDriverName) PCHAR DriverName,
_In_ size_t cbDriverName,
_Out_ HDEVINFO *pDevInfo,
_Out_writes_bytes_(sizeof(SP_DEVINFO_DATA)) PSP_DEVINFO_DATA pDevInfoData
);

VOID FreeDeviceProperties(_In_ PUSB_DEVICE_PNP_STRINGS *ppDevProps);

PUSB_DEVICE_PNP_STRINGS
DriverNameToDeviceProperties(
_In_reads_bytes_(cbDriverName) PCHAR  DriverName,
_In_ size_t cbDriverName
);

DWORD
GetHostControllerPowerMap(
HANDLE hHCDev,
PUSBHOSTCONTROLLERINFO hcInfo);

DWORD
GetHostControllerInfo(
HANDLE hHCDev,
PUSBHOSTCONTROLLERINFO hcInfo);

PCHAR GetRootHubName(
	HANDLE HostController
	);

PCHAR GetDriverKeyName(
	HANDLE  Hub,
	ULONG   ConnectionIndex
	);

PUSB_DESCRIPTOR_REQUEST
GetConfigDescriptor(
HANDLE  hHubDevice,
ULONG   ConnectionIndex,
UCHAR   DescriptorIndex
);

PDEVICE_INFO_NODE
FindMatchingDeviceNodeForDriverName(
_In_ PSTR   DriverKeyName,
_In_ BOOLEAN IsHub
);

PUSB_DESCRIPTOR_REQUEST
GetBOSDescriptor(
HANDLE  hHubDevice,
ULONG   ConnectionIndex
);

PSTRING_DESCRIPTOR_NODE
GetStringDescriptor(
HANDLE  hHubDevice,
ULONG   ConnectionIndex,
UCHAR   DescriptorIndex,
USHORT  LanguageID
);

HRESULT
GetStringDescriptors(
_In_ HANDLE                         hHubDevice,
_In_ ULONG                          ConnectionIndex,
_In_ UCHAR                          DescriptorIndex,
_In_ ULONG                          NumLanguageIDs,
_In_reads_(NumLanguageIDs) USHORT  *LanguageIDs,
_In_ PSTRING_DESCRIPTOR_NODE        StringDescNodeHead
);

BOOL
AreThereStringDescriptors(
PUSB_DEVICE_DESCRIPTOR          DeviceDesc,
PUSB_CONFIGURATION_DESCRIPTOR   ConfigDesc
);

PSTRING_DESCRIPTOR_NODE
GetAllStringDescriptors(
HANDLE                          hHubDevice,
ULONG                           ConnectionIndex,
PUSB_DEVICE_DESCRIPTOR          DeviceDesc,
PUSB_CONFIGURATION_DESCRIPTOR   ConfigDesc
);

PCHAR GetExternalHubName(
	HANDLE  Hub,
	ULONG   ConnectionIndex
	);

VOID
EnumerateHub(
_In_reads_(cbHubName) PCHAR                     HubName,
_In_ size_t                                     cbHubName,
_In_opt_ PUSB_NODE_CONNECTION_INFORMATION_EX    ConnectionInfo,
_In_opt_ PUSB_NODE_CONNECTION_INFORMATION_EX_V2 ConnectionInfoV2,
_In_opt_ PUSB_PORT_CONNECTOR_PROPERTIES         PortConnectorProps,
_In_opt_ PUSB_DESCRIPTOR_REQUEST                ConfigDesc,
_In_opt_ PUSB_DESCRIPTOR_REQUEST                BosDesc,
_In_opt_ PSTRING_DESCRIPTOR_NODE                StringDescs,
_In_opt_ PUSB_DEVICE_PNP_STRINGS                DevProps
);

VOID
EnumerateHubPorts(
HANDLE      hHubDevice,
ULONG       NumPorts
);

VOID
DisplayStringDescriptor(
UCHAR                   Index,
PSTRING_DESCRIPTOR_NODE StringDescs,
DEVICE_POWER_STATE      LatestDevicePowerState
);

BOOL CheckFile(char *SerialNumber, char path, int serial_len);
BOOL CheckUSB();