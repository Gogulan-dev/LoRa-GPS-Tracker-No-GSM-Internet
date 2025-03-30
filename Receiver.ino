#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <WebServer.h>
#include <math.h>

// LoRa module connections
#define SS 5
#define RST 22
#define DIO0 2

// WiFi credentials
const char* ssid = "username";
const char* password = "password";

// Web server on port 80
WebServer server(80);
// Geo-Fence Center Coordinates
#define CENTER_LAT 11.912588 // Your center 
#define CENTER_LON 79.635101
#define GEO_FENCE_RADIUS 150.0  // Geo-fence radius in meters

float currentLat = CENTER_LAT, currentLon = CENTER_LON;
bool isInsideGeoFence = true;
float distance_m = 0.0;

void setup() {
  Serial.begin(115200);

  // Initialize WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  delay(10000);

  // Initialize LoRa
  SPI.begin();
  LoRa.setPins(SS, RST, DIO0);
  
  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa init failed. Check connections.");
    while (1);
  }
  Serial.println("LoRa Receiver Started...");

  // Start Web Server
  server.on("/", handleWebPage);
  server.on("/data", handleJSON);
  server.begin();
}

// Function to calculate distance using Haversine formula
#define EARTH_RADIUS 6371000.0  

float getDistance(float flat1, float flon1, float flat2, float flon2) {
  float diflat = radians(flat2 - flat1);
  float diflon = radians(flon2 - flon1);
  flat1 = radians(flat1);
  flat2 = radians(flat2);
  
  float a = sin(diflat / 2) * sin(diflat / 2) + cos(flat1) * cos(flat2) * sin(diflon / 2) * sin(diflon / 2);
  float c = 2 * atan2(sqrt(a), sqrt(1 - a));
  
  return EARTH_RADIUS * c;  // Distance in meters
}

void loop() {
  server.handleClient();

  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String receivedData = "";
    while (LoRa.available()) {
      receivedData += (char)LoRa.read();
    }

    Serial.print("Received: ");
    Serial.println(receivedData);

    float lat, lon;
    if (sscanf(receivedData.c_str(), "%f,%f", &lat, &lon) == 2) {
      if (lat != 0.0 && lon != 0.0) {
        currentLat = lat;
        currentLon = lon;
        
        distance_m = getDistance(CENTER_LAT, CENTER_LON, lat, lon);
        isInsideGeoFence = (distance_m <= GEO_FENCE_RADIUS);

        Serial.printf("Latitude: %.6f, Longitude: %.6f, Distance: %.2f meters\n", lat, lon, distance_m);
        Serial.println(isInsideGeoFence ? "✅ Inside Geo-Fence" : "⚠️ Left Geo-Fence!");
      } else {
        Serial.println("⚠️ Invalid GPS data, ignoring...");
      }
    } else {
      Serial.println("⚠️ GPS data parsing failed!");
    }
  }
}

// JSON API Endpoint
void handleJSON() {
  String json = "{";
  json += "\"latitude\": " + String(currentLat, 6) + ",";
  json += "\"longitude\": " + String(currentLon, 6) + ",";
  json += "\"distance\": " + String(distance_m) + ",";
  json += "\"alert\": \"" + String(isInsideGeoFence ? "✅ Inside Geo-Fence" : "⚠️ Left Geo-Fence!") + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

// Web Page with Map
void handleWebPage() {
  String html = R"rawliteral(
    <!DOCTYPE html>
<html>
<head>
    <link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/bootstrap@4.6.2/dist/css/bootstrap.min.css">
  <script src="https://cdn.jsdelivr.net/npm/jquery@3.7.1/dist/jquery.slim.min.js"></script>
  <script src="https://cdn.jsdelivr.net/npm/popper.js@1.16.1/dist/umd/popper.min.js"></script>
  <script src="https://cdn.jsdelivr.net/npm/bootstrap@4.6.2/dist/js/bootstrap.bundle.min.js"></script>
    <title>ESP32 GPS Tracker</title>
    <script async defer src="https://maps.googleapis.com/maps/api/js?key=your-api-key&callback=initMap"></script> // your api key
    <script>
        let map, marker, path, geofenceCircle, geofenceLine;
        let pathCoordinates = [];
        let geofenceCenter = { lat: 11.912588, lng: 79.635101 }; // Geofence center coordinates
        let geofenceRadius = 150.0; // radius

        function initMap() {
            map = new google.maps.Map(document.getElementById("map"), {
                center: geofenceCenter,
                zoom: 18,
            });

            marker = new google.maps.Marker({
                position: geofenceCenter,
                map: map,
                title: "Tracker",
            });

            // Create a polyline for tracking movement
            path = new google.maps.Polyline({
                path: pathCoordinates,
                geodesic: true,
                strokeColor: "#FF0000",
                strokeOpacity: 1.0,
                strokeWeight: 2,
                map: map
            });

            // Draw geofence circle
            geofenceCircle = new google.maps.Circle({
                strokeColor: "#0000FF",
                strokeOpacity: 0.8,
                strokeWeight: 2,  
                fillColor: "#0000FF",
                fillOpacity: 0.2,
                map: map,
                center: geofenceCenter,
                radius: geofenceRadius
            });

            // Line connecting geofence center to tracker
            geofenceLine = new google.maps.Polyline({
                strokeColor: "#FF0000",  // Red line
                strokeOpacity: 1.0,
                strokeWeight: 2,
                map: map
            });

            fetchData();
            setInterval(fetchData, 3000);
        }

        function fetchData() {
            fetch('/data')
            .then(response => response.json())
            .then(data => {
                let newPos = { lat: data.latitude, lng: data.longitude };
                
                document.getElementById('lat').innerText = data.latitude.toFixed(6);
                document.getElementById('lng').innerText = data.longitude.toFixed(6);
                document.getElementById('dist').innerText = data.distance.toFixed(2) + " meters";
                document.getElementById('alert').innerText = data.alert;
                document.getElementById('alert').style.color = (data.distance > geofenceRadius) ? "red" : "green";

                // Update marker position
                marker.setPosition(newPos);
                map.setCenter(newPos);

                // Update tracking path
                pathCoordinates.push(newPos);
                path.setPath(pathCoordinates);

                // Update geofence line (red line from center to tracker)
                geofenceLine.setPath([geofenceCenter, newPos]);
                geofenceLine.setMap(map); // Re-add to map
            })
            .catch(error => console.error('Error fetching data:', error));
        }
    </script>
        <style>
            .card-header {
                background-color: #007bff; /* Blue */
                color: white;
            }
            .card-body {
                background-color: #d4edda; /* Light Green */
            }
        </style>
</head>
<body>
    <h1>ESP32 GPS Tracker</h1>
    <div class="container p-3 my-3 border">
        <p class = "card-body">Latitude: <span class ="card-header" id="lat">Loading...</span></p>
        <p class="card-body">Longitude: <span class = "card-header" id="lng">Loading...</span></p>
        <p class="card-body">Distance from Center: <span class = "card-header" id="dist">Loading...</span></p>
    </div>
    <div class="container p-3 my-3 border">
    <p>Alert: <span id="alert">Loading...</span></p>
    </div>
    <div id="map" style="width: 100%; height: 500px;"></div>
</body>
</html>

  )rawliteral";

  server.send(200, "text/html", html);
}
