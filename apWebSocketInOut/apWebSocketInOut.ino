		#include <ESP8266WiFi.h>
		#include <ESP8266WebServer.h>
		#include <WebSocketsServer_Generic.h>
		
    /*
    progetto evoluto 7 dicembre 2023(!)
    */
		const char* ssid = "sid"; // Sostituisci con il nome della tua rete
		const char* password = "pwd12345"; // Sostituisci con la tua password
		
		ESP8266WebServer server(80);
		WebSocketsServer webSocket = WebSocketsServer(81);
		
		void setup() {
		  Serial.begin(115200);
		  WiFi.softAP(ssid, password);
		
		  server.on("/", HTTP_GET, []() {
		    server.send(200, "text/html", getHTML());
		  });
		
		  server.begin();
		
		  webSocket.begin();
		  webSocket.onEvent(webSocketEvent);
		}
		
		void loop() {
		  server.handleClient();
		  webSocket.loop();
		
      if (Serial.available() > 0) {
          String message = Serial.readStringUntil('\n');
          handleSerialMessage(message);
        }
		}
		
		void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
		  if (type == WStype_TEXT) {
		    Serial.println((char *)payload);
		  }
		}

    void handleSerialMessage(String message) {
      int commaIndex = message.indexOf(',');
      if (commaIndex != -1) {
        String control = message.substring(0, commaIndex);
        String value = message.substring(commaIndex + 1);

        String script = "document.getElementById('" + control + "').value = " + value + ";";
        script += "updateSlider(document.getElementById('" + control + "'));";
        webSocket.broadcastTXT(script);
      }
    }
		
		String getHTML() {
		  String html =  "<!DOCTYPE html><html>";
      html +=        "<head>";
      html +=        getHead();
      html +=        "</head>";
      html +=        "<body>";
      html +=        getContainer();
		  html +=        "<script>";
      html +=        "var debounceTimer;";
		  html +=        "var webSocket = new WebSocket('ws://' + window.location.hostname + ':81/');";
		  html +=        "webSocket.onmessage = function(event) {";
		  html +=        "  eval(event.data);"; // Esegue lo script ricevuto
		  html +=        "};";
      html +=        getScript();
		  html +=        "</script>";
		  html +=        "</body>";
      html +=        "</html>";
		  return html;
		}

  // initializeSliders --> updateSlider --> updateBackgroundColor
  String getScript(){
    String html = ""    
        "function updateSlider(slider) {"
            "var container = slider.parentElement;"
            "var valueLabel = container.querySelector('.value-label');"
            "valueLabel.textContent = slider.value;"
            "updateBackgroundColor(slider, container);"
            "if (debounceTimer) {"
              " clearTimeout(debounceTimer);"
            "}"
            "debounceTimer = setTimeout(function() {"
                "var message = slider.id + ',' + slider.value;"
                "webSocket.send(message);"
            "}, 100);"
        "}"
        "function updateBackgroundColor(slider, container) {"
            "var value = parseInt(slider.value, 10);" // Assicurati che sia un numero
            "var max = parseInt(slider.max, 10);" // Assicurati che sia un numero
            "var percentage = value / max;"
            "var intensity = parseInt(255 - (percentage * 255),10);"
            "var color = 'rgb(' + intensity + ', ' + intensity + ', 255)';"
            "container.style.backgroundColor = color;"
        "}"
        "function initializeSliders() {"
            "var sliders = document.querySelectorAll('.vertical-slider');"
            "for (var i = 0; i < sliders.length; i++) {" // Uso di ciclo for invece di forEach
                "updateSlider(sliders[i]);"
            "}"
        "}"
        "window.onload = function() {"
          "setTimeout(initializeSliders,100);"
        "};";
    return html;
  }

  String getHead(){
    String html = "<title>DCO1 Mixer Controls</title>"
    "<style>"
    "body {"
      "font-family: Arial, sans-serif;"
      "text-align: center;"
    "}"
    ".containers-wrapper {"
      "display: flex;"
      "flex-wrap: wrap; /* Permette agli elementi di andare a capo */"
    "}"
    ".main-container {"
      "display: inline-block;"
      "border: 1px solid #ddd;"
      "padding: 5px; /* Riduzione del padding */"
      "margin-top: 20px;"
      "font-size: 8px;"
    "}"
    ".dco-title {"
      "background-color: black;"
      "color: white;"
      "padding: 5px;"
      "font-size: 8px;"
      "text-align: center;"
      "width: 100%;"
      "box-sizing: border-box;"
      "margin-bottom: 5px; /* Riduzione dello spazio sotto il titolo */"
    "}"
    ".control-container {"
      "display: inline-block;"
      "vertical-align: top;"
      "margin: 0; /* Rimozione del margine esterno */"
      "padding: 5px; /* Riduzione del padding interno */"
      "border: 1px solid #ddd;"
      "height: auto; /* Altezza automatica in base al contenuto */"
      "min-height: 100px; /* Altezza minima per ospitare le slider */"
      "font-size: 8px;"
      "position: relative;"
    "}"
    ".vertical-slider {"
      "width: 20px;"
      "height: 100px;"
      "-webkit-appearance: slider-vertical;"
      "writing-mode: bt-lr;"
      "font-size: 8px;"
    "}"
    ".value-label {"
      "position: absolute; /* Posizionamento assoluto rispetto al suo contenitore */"
      "bottom: -20px; /* Posiziona l'etichetta sotto al cursore */"
      "left: 50%; /* Centra l'etichetta */"
      "transform: translateX(-50%); /* Allinea centralmente l'etichetta */"
      "white-space: nowrap; /* Evita l'a capo dell'etichetta */"
    "}"
    "</style>";
    return html;
  }

  String getContainer(){
    String html = "<div class='containers-wrapper'>";
    html += "<div class='main-container'>";
    html += "<div class='dco-title'>DCO1</div>";
    html += generCursor("OCT",1,3,2);
    html += generCursor("WAV",1,15,9);
    html += generCursor("LEV",1,31,25);        
    html += "</div>";
    html += "</div>";
    return html;
  }

String generCursor(String thename,int min, int max, int defvalue){
  String html = "<div class='control-container'>";
  html +=  "<div>" + thename + "</div>";
  html += "<input type='range' class='vertical-slider' min='" + 
          String(min) + "' max='" + 
          String(max) + "' value='" + 
          String(defvalue) + "' orient='vertical' oninput='updateSlider(this)' id='" + 
          thename + 
          "' >";
  html +=  "<span class='value-label'>"+String(defvalue)+"</span>";
  html +=  "</div>";
  return html;
}