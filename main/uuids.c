#if defined(CONFIG_IDF_TARGET_ESP32)

bt_uuid bt_uuids[] = {
    {
        .uuid16 = 0x1000,
        .name = "ServiceDiscoveryServerServiceClassID"
    }, {
        .uuid16 = 0x1001,
        .name = "BrowseGroupDescriptorServiceClassID"
    }
};


#else
    bt_uuid bt_uuids[] = NULL;
#endif