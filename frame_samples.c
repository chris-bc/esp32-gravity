uint8_t probe_response_raw[] = {
0x50, 0x00, 0x3c, 0x00, 
0x08, 0x5a, 0x11, 0xf9, 0x23, 0x3d, // destination address
0x20, 0xe8, 0x82, 0xee, 0xd7, 0xd5, // source address
0x20, 0xe8, 0x82, 0xee, 0xd7, 0xd5, // BSSID
0xc0, 0x72,                         // Fragment number 0 seq number 1836
0xa3, 0x52, 0x5b, 0x8d, 0xd2, 0x00, 0x00, 0x00, 0x64, 0x00,
0x11, 0x11, // 802.11 Capabilities == WPA2, 0x1101 == open auth. 0x11 0x01 or 0x01 0x11?
0x00, 0x08, // Parameter Set (0), SSID Length (8)
            // To fill: SSID
0x01, 0x08, 0x8c, 0x12, 0x98, 0x24, 0xb0, 0x48, 0x60, 0x6c,
0x20, 0x01, 0x00, 0x23, 0x02, 0x14, 0x00, 0x30, 0x14, 0x01, 0x00, 0x00, 0x0f, 0xac, 0x04, 0x01,
0x00, 0x00, 0x0f, 0xac, 0x04, 0x01, 0x00, 0x00, 0x0f, 0xac, 0x02, 0x0c, 0x00, 0x46, 0x05, 0x32,
0x08, 0x01, 0x00, 0x00, 0x2d, 0x1a, 0xef, 0x08, 0x17, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x3d, 0x16, 0x95, 0x0d, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7f, 0x08, 0x04, 0x00, 0x08, 0x00, 0x00, 0x00,
0x00, 0x40, 0xbf, 0x0c, 0xb2, 0x58, 0x82, 0x0f, 0xea, 0xff, 0x00, 0x00, 0xea, 0xff, 0x00, 0x00,
0xc0, 0x05, 0x01, 0x9b, 0x00, 0x00, 0x00, 0xc3, 0x04, 0x02, 0x02, 0x02, 0x02,
0x71, 0xb5, 0x92, 0x42 // Frame check sequence
};

int PROBE_RESPONSE_DEST_ADDR_OFFSET = 4;
int PROBE_RESPONSE_SRC_ADDR_OFFSET = 10;
int PROBE_RESPONSE_BSSID_OFFSET = 16;
int PROBE_RESPONSE_AUTH_OFFSET = 34;
int PROBE_RESPONSE_SSID_OFFSET = 38;