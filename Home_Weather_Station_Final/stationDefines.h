/* DHT22 */
#define DHTPIN D3  
#define DHTTYPE DHT22 
int localHum = 0;
int localTemp = 0;

/* TimeClient */
const float UTC_OFFSET = -3;

const int UPDATE_INTERVAL_SECS = 5 * 60; // Update every 5min

// flag changed in the ticker function every 10 minutes
bool readyForWeatherUpdate = false;

String lastUpdate = "--";
