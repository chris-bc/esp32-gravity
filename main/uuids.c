#include "bluetooth.h"

/* Source: https://bitbucket.org/bluetooth-SIG/public/raw/4b98566471c03ddbdde0cdd615166f012bc05281/assigned_numbers/uuids/service_class.yaml */
#if defined(CONFIG_BT_ENABLED) && defined(CONFIG_DECODE_UUIDS)
#define BT_UUID_COUNT 76

bt_uuid bt_uuids[BT_UUID_COUNT] = {
    {
       .uuid16 = 0x1000,
       .name = "ServiceDiscoveryServerServiceClassID"
    }, {
       .uuid16 = 0x1001,
       .name = "BrowseGroupDescriptorServiceClassID"
    }, {
        .uuid16 = 0x1101,
        .name = "SerialPort"
    }, {
        .uuid16 = 0x1102,
        .name = "LANAccessUsingPPP"
    }, {
        .uuid16 = 0x1103,
        .name = "DialupNetworking"
    }, {
        .uuid16 = 0x1104,
        .name = "IrMCSync"
    }, {
        .uuid16 = 0x1105,
        .name = "OBEXObjectPush"
    }, {
        .uuid16 = 0x1106,
        .name = "OBEXFileTransfer"
    }, {
        .uuid16 = 0x1107,
        .name = "IrMCSyncCommand"
    }, {
        .uuid16 = 0x1108,
        .name = "Headset"
    }, {
        .uuid16 = 0x1109,
        .name = "CordlessTelephony"
    }, {
        .uuid16 = 0x110A,
        .name = "AudioSource"
    }, {
        .uuid16 = 0x110B,
        .name = "AudioSink"
    }, {
        .uuid16 = 0x110C,
        .name = "A/V_RemoteControlTarget"
    }, {
        .uuid16 = 0x110D,
        .name = "AdvancedAudioDistribution"
    }, {
        .uuid16 = 0x110E,
        .name = "A/V_RemoteControl"
    }, {
        .uuid16 = 0x110F,
        .name = "A/V_RemoteControlController"
    }, {
        .uuid16 = 0x1110,
        .name = "Intercom"
    }, {
        .uuid16 = 0x1111,
        .name = "Fax"
    }, {
        .uuid16 = 0x1112,
        .name = "Headset - Audio Gateway"
    }, {
        .uuid16 = 0x1113,
        .name = "WAP"
    }, {
        .uuid16 = 0x1114,
        .name = "WAP_CLIENT"
    }, {
        .uuid16 = 0x1115,
        .name = "PANU"
    }, {
        .uuid16 = 0x1116,
        .name = "NAP"
    }, {
        .uuid16 = 0x1117,
        .name = "GN"
    }, {
        .uuid16 = 0x1118,
        .name = "DirectPrinting"
    }, {
        .uuid16 = 0x1119,
        .name = "ReferencePrinting"
    }, {
        .uuid16 = 0x111A,
        .name = "Basic Imaging Profile"
    }, {
        .uuid16 = 0x111B,
        .name = "ImagingResponder"
    }, {
        .uuid16 = 0x111C,
        .name = "ImagingAutomaticArchive"
    }, {
        .uuid16 = 0x111D,
        .name = "ImagingReferencedObjects"
    }, {
        .uuid16 = 0x111E,
        .name = "Handsfree"
    }, {
        .uuid16 = 0x111F,
        .name = "HandsfreeAudioGateway"
    }, {
        .uuid16 = 0x1120,
        .name = "DirectPrintingReferenceObjectsService"
    }, {
        .uuid16 = 0x1121,
        .name = "ReflectedUI"
    }, {
        .uuid16 = 0x1122,
        .name = "BasicPrinting"
    }, {
        .uuid16 = 0x1123,
        .name = "PrintingStatus"
    }, {
        .uuid16 = 0x1124,
        .name = "HumanInterfaceDeviceService"
    }, {
        .uuid16 = 0x1125,
        .name = "HardcopyCableReplacement"
    }, {
        .uuid16 = 0x1126,
        .name = "HCR_Print"
    }, {
        .uuid16 = 0x1127,
        .name = "HCR_Scan"
    }, {
        .uuid16 = 0x1128,
        .name = "Common_ISDN_Access"
    }, {
        .uuid16 = 0x112D,
        .name = "SIM_Access"
    }, {
        .uuid16 = 0x112E,
        .name = "Phonebook Access - PCE"
    }, {
        .uuid16 = 0x112F,
        .name = "Phonebook Access - PSE"
    }, {
        .uuid16 = 0x1130,
        .name = "Phonebook Access"
    }, {
        .uuid16 = 0x1131,
        .name = "Headset - HS"
    }, {
        .uuid16 = 0x1132,
        .name = "Message Access Server"
    }, {
        .uuid16 = 0x1133,
        .name = "Message Notification Server"
    }, {
        .uuid16 = 0x1134,
        .name = "Message Access Profile"
    }, {
        .uuid16 = 0x1135,
        .name = "GNSS"
    }, {
        .uuid16 = 0x1136,
        .name = "GNSS_Server"
    }, {
        .uuid16 = 0x1137,
        .name = "3D Display"
    }, {
        .uuid16 = 0x1138,
        .name = "3D Glasses"
    }, {
        .uuid16 = 0x1139,
        .name = "3D Synchronization"
    }, {
        .uuid16 = 0x113A,
        .name = "MPS Profile"
    }, {
        .uuid16 = 0x113B,
        .name = "MPS SC"
    }, {
        .uuid16 = 0x113C,
        .name = "CTN Access Service"
    }, {
        .uuid16 = 0x113D,
        .name = "CTN Notification Service"
    }, {
        .uuid16 = 0x113E,
        .name = "CTN Profile"
    }, {
        .uuid16 = 0x1200,
        .name = "PnPInformation"
    }, {
        .uuid16 = 0x1201,
        .name = "GenericNetworking"
    }, {
        .uuid16 = 0x1202,
        .name = "GenericFileTransfer"
    }, {
        .uuid16 = 0x1203,
        .name = "GenericAudio"
    }, {
        .uuid16 = 0x1204,
        .name = "GenericTelephony"
    }, {
        .uuid16 = 0x1205,
        .name = "UPNP_Service"
    }, {
        .uuid16 = 0x1206,
        .name = "UPNP_IP_Service"
    }, {
        .uuid16 = 0x1300,
        .name = "ESDP_UPNP_IP_PAN"
    }, {
        .uuid16 = 0x1301,
        .name = "ESDP_UPNP_IP_LAP"
    }, {
        .uuid16 = 0x1302,
        .name = "ESDP_UPNP_L2CAP"
    }, {
        .uuid16 = 0x1303,
        .name = "VideoSource"
    }, {
        .uuid16 = 0x1304,
        .name = "VideoSink"
    }, {
        .uuid16 = 0x1305,
        .name = "VideoDistribution"
    }, {
        .uuid16 = 0x1400,
        .name = "HDP"
    }, {
        .uuid16 = 0x1401,
        .name = "HDP Source"
    }, {
        .uuid16 = 0x1402,
        .name = "HDP Sink"
    }
};


#else
    #define BT_UUID_COUNT 0

    bt_uuid bt_uuids[] = {NULL};
#endif