#include <esp_err.h>
#include <esp_wifi.h>
#include <stdbool.h>

/*
 * This is the (currently unofficial) 802.11 raw frame TX API,
 * defined in esp32-wifi-lib's libnet80211.a/ieee80211_output.o
 *
 * This declaration is all you need for using esp_wifi_80211_tx in your own application.
 */
esp_err_t esp_wifi_80211_tx(wifi_interface_t ifx, const void *buffer, int len, bool en_sys_seq);

uint8_t beacon_raw[] = {
	0x80, 0x00,							    // 0-1: Frame Control
	0x00, 0x00,						    	// 2-3: Duration
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff,				// 4-9: Destination address (broadcast)
	0xba, 0xde, 0xaf, 0xfe, 0x00, 0x06,		// 10-15: Source address
	0xba, 0xde, 0xaf, 0xfe, 0x00, 0x06,		// 16-21: BSSID
	0x00, 0x00,				    		// 22-23: Sequence / fragment number
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,     // 24-31: Timestamp (GETS OVERWRITTEN TO 0 BY HARDWARE)
	0x64, 0x00,			    			// 32-33: Beacon interval
	0x31, 0x04,		    				// 34-35: Capability info
	0x00, 0x00, /* FILL CONTENT HERE */	// 36-38: SSID parameter set, 0x00:length:content
	0x01, 0x08, 0x82, 0x84,	0x8b, 0x96, 0x0c, 0x12, 0x18, 0x24,	// 39-48: Supported rates
	0x03, 0x01, 0x01,				// 49-51: DS Parameter set, current channel 1 (= 0x01),
	0x05, 0x04, 0x01, 0x02, 0x00, 0x00,		// 52-57: Traffic Indication Map
	
};

uint8_t probe_raw[] = {
    /*IEEE 802.11 Probe Request*/
	0x40, 0x00, //Frame Control version=0 type=0(management)  subtype=100 (probe request)
	0x00, 0x00, //Duration
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, //A1 Destination address  broadcast
	0x50, 0x02, 0x91, 0x90, 0x0c, 0x70, //A2 Source address (STA Mac i.e. transmitter address)
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, //A3 BSSID
	0x30, 0x9e, //sequence number (This would be overwritten if 'en_sys_seq' is set to true in fn call esp_wifi_80211_tx )

	/*IEEE 802.11 Wireless Management*/
	/*Tagged Paramneters*/
	/*Tag: SSID parameter set: Wildcard SSID*/
	0x00,	//ssid.element_id
	0x00,	//ssid.length
                //ssid.name

	/*Tag: Supported Rates 5.5(B), 11(B), 1(B), 2(B), 6, 12, 24, 48, [Mbit/sec]*/
	0x01,	//wlan.tag.number => 1
	0x08,	//wlan.tag.length => 8
	0x8b,	//Supported Rates: 5.5(B) (0x8b)	(wlan.supported_rates)
	0x96,	//Supported Rates: 11(B) (0x96)		(wlan.supported_rates)
	0x82,	//Supported Rates: 1(B) (0x82)		(wlan.supported_rates)
	0x84,	//Supported Rates: 2(B) (0x84)		(wlan.supported_rates)
	0x0c,	//Supported Rates: 6 (0x0c)			(wlan.supported_rates)
	0x18,	//Supported Rates: 12 (018)			(wlan.supported_rates)
	0x30,	//Supported Rates: 24 (0x30)		(wlan.supported_rates)
	0x60,	//Supported Rates: 48 (0x60)		(wlan.supported_rates)

	/*Tag: Extended Supported Rates 54, 9, 18, 36, [Mbit/sec]*/

	0x32,	//wlan.tag.number => 50
	0x04,	//wlan.tag.length => 4
	0x6c,	//Extended Supported Rates: 54 (0x6c)	(wlan.extended_supported_rates)
	0x12,	//Extended Supported Rates: 9 (0x12)	(wlan.extended_supported_rates
	0x24,	//Extended Supported Rates: 18 (0x24)	(wlan.extended_supported_rates
	0x48,	//Extended Supported Rates: 36 (0x48)	(wlan.extended_supported_rates

	/*ACTUAL 0x88 0x7d 0x42 0xd4   0x83, 0x2d, 0xbe, 0xce*/ //Frame check sequence: 0x1b8026b0 [unverified] (wlan.fcs)
};
