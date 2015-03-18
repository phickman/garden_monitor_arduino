Also create keys.h with the content below and enter the required SSID name/password and Carriots details.

```
#define WLAN_SSID       ""        // cannot be longer than 32 characters!
#define WLAN_PASS       ""
// Security can be WLAN_SEC_UNSEC, WLAN_SEC_WEP, WLAN_SEC_WPA or WLAN_SEC_WPA2
#define WLAN_SECURITY   WLAN_SEC_WPA2

// Carriots parameters
#define WEBSITE  "api.carriots.com"
#define API_KEY  "" // not sure if full or stream only is required
#define DEVICE   ""
```
