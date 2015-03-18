Also create keys.h with the content below and enter the required SSID name/password and Carriots details.

```
#define WLAN_SSID       "" // SSID name (cannot be longer than 32 characters)
#define WLAN_PASS       "" // SSID password
// Security can be WLAN_SEC_UNSEC, WLAN_SEC_WEP, WLAN_SEC_WPA or WLAN_SEC_WPA2
#define WLAN_SECURITY   WLAN_SEC_WPA2

// Carriots parameters
#define WEBSITE  "api.carriots.com" // the Carriots API address, shouldn't need to change
#define API_KEY  "" // Carriots API key, not sure if full or stream only is required
#define DEVICE   "" // Carriots device name, usually sensor@username.username
```
