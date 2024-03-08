#ifndef PAGINEWEB_H
#define PAGINEWEB_H

#include "incomune.h"

char* getHTMLsetTime() {
  static char html[HTML_SIZE];
  memset(html, 0, HTML_SIZE);  // Pulisce l'array
  // ========================
  // Parte iniziale dell'HTML
  // ========================
  strcat(html,
    "<!DOCTYPE html>"
    "<html lang=\"it\">"
    "<head>"
      "<meta charset=\"UTF-8\">"
      "<title>Tabella con SVG e Pulsante</title>"
      "<style>"
      "* {font-family:monospace}"
      "table, th, td {border: 1px solid black;; border-collapse: collapse; padding: 0}"
      "th, td {padding: 0; margin: 0; text-align: right;}"
      ".svg-cell {padding: 0; margin: 0;}"
      "</style>"
    "</head>"
    "<body>"
    "<table><tr>"
    "<td><a href=/>Home</a></td>"
    "<td><a href=/setTime>Set Time</a></td>"
    "<td><a href=/getEnergyHours>Get Energy Hours</a></td>"
    "</tr></table>" 
    "<table id = 'tabellaSvg'>");

  // ========
  // tabella
  // ========
  char cell[200];  // Aumenta la dimensione se necessario
  for (int row = 0; row < TAB_ROWS; row++) {
    strcat(html, "<tr>");
    for (int col = 0; col < TAB_COLUMNS; col++) {
      sprintf(cell, "<td id='cell-%d-%d'></td>", col, row);
      strcat(html, cell);
    }
    strcat(html, "</tr>\n");
  }
  // ==================
  // Script JavaScript
  // ==================
  strcat(html, 
  "<script>"
  // Apertura socket BROWSER --> ESP01 "
  "var webSocket = new WebSocket('ws://' + window.location.hostname + ':81/');"
  // Funzione per inserire un pulsante e aggiungere l'eventListener"
  "function inserisciPulsante(idCella, testoPulsante) {"
    "var cella = document.getElementById(idCella);"
    "var pulsante = document.createElement('button');    "
    "pulsante.addEventListener('click', function() {"
      "var now = new Date();"
      "var dateTimeString = now.getFullYear() + '-' + "
        "('0' + (now.getMonth() + 1)).slice(-2) + '-' + "
        "('0' + now.getDate()).slice(-2) + ' ' + "
        "('0' + now.getHours()).slice(-2) + ':' + "
        "('0' + now.getMinutes()).slice(-2) + ':' + "
        "('0' + now.getSeconds()).slice(-2);      "
      "webSocket.send(dateTimeString);" // <<------- PULSANTE BROWSER -> ESP-01"
      "pulsante.textContent = 'Data/Ora Aggiornata';"
    "});"
    "pulsante.innerText = testoPulsante;"
    "cella.appendChild(pulsante);"
  "}"
  // Inserimento pulsante"
  "inserisciPulsante('cell-2-2', 'Clicca Qui');"
  "</script>"
  "</body>"
  "</html>"  
  );

  return html;
}

char* getHTMLmain() {
  static char html[HTML_SIZE];
  memset(html, 0, HTML_SIZE);  // Pulisce l'array
  // ========================
  // Parte iniziale dell'HTML
  // ========================
  strcat(html,
    "<!DOCTYPE html>"
    "<html lang=\"it\">"
    "<head>"
      "<meta charset=\"UTF-8\">"
      "<title>Tabella con SVG e Pulsante</title>"
      "<style>"
      //"table, th, td {border-collapse: collapse; padding: 0}"
      "table {"
        "border: 1px solid black;"
        "border-collapse: collapse;"
      "}"            
      "table td, table th {"
        "border: 1px solid gray;"
        "border-collapse: collapse;"
        "padding: 0px 5px;"  //verticale orizzontale
      "}"      
      //"th, td {padding: 0; margin: 0; text-align: right;}"
      ".svg-cell {padding: 0; margin: 0;}"
      "td:nth-child(3) {"
      "color: blue;"
      "font-weight: bold;"
      "}"
      "td:nth-child(4) {"
      "font-family:monospace;"
      "}"
      "td:nth-child(5) {"
      "font-style: italic;"
      "}"

      "</style>"
    "</head>"
    "<body>"
    "<table><tr>"
    "<td><a href=/>Home</a></td>"
    "<td><a href=/setTime>Set Time</a></td>"
    "<td><a href=/getEnergyHours>Get Energy Hours</a></td>"
    "<td><a href=/getEnergyDays>Get Energy Days</a></td>"

    "</tr></table>" 
    "<table id = 'tabellaSvg'>"
    );

  // ========
  // tabella
  // ========
  char cell[200];  // Aumenta la dimensione se necessario
  for (int row = 0; row < TAB_ROWS; row++) {
    strcat(html, "<tr>");
    for (int col = 0; col < TAB_COLUMNS; col++) {
      if (col == 1) {
        sprintf(cell, "<td class='svg-cell'>"
                      "<svg id='svg-%d-%d'>"
                      "</svg>"
                      "</td>",col, row);
        strcat(html, cell);
      } else {
        sprintf(cell, "<td id='cell-%d-%d'></td>", col, row);
        strcat(html, cell);
      }
    }
    strcat(html, "</tr>");
  }
  // ==================
  // Script JavaScript
  // ==================
  strcat(html, 
  "</tbody>"
  "</table>"
  "<script>"
  // Apertura socket BROWSER --> ESP01 "
  "var webSocket = new WebSocket('ws://' + window.location.hostname + ':81/');"
  // =============================================="
  // Gestione del messaggio ricevuto via WebSocket"
  // =============================================="
  "webSocket.onmessage = function(event) {"
    "var data = parseData(event.data);"
    "data.reverse();"
    "updateTable(data);"
    "insertSvgRectangles(data);"
  "};"
  // Analizzare i dati ricevuti"

  "function padStart(str, targetLength, padString) {"
    "str = str.toString();" 
    "while (str.length < targetLength) {"
      "str = padString + str;"
    "}"
    "return str;"
  "}"  
  "function parseData(dataString) {"
    "var rows = dataString.split(';').filter(function(row) {"
        "return row.length > 0;"
    "});"
    "return rows.map(function(row) {"
      "var values = row.split(',');"
      "var dataOra = values[1] + '/' +"
                  "padStart(values[2], 2, '0') + '/' +"
                  "padStart(values[3], 2, '0') + ' ' +"
                  "padStart(values[4], 2, '0') + ':' +"
                  "padStart(values[5], 2, '0') + ':' +"
                  "padStart(values[6], 2, '0');"
      "return {"
          //chiave: valore "
          "potenza: values[8],"
          "dataora: dataOra,"
          "ms_deltaT: values[7],"
      "};"
    "}"
    ");"
  "}"
  "function updateTable(data) {"
    "var table = document.getElementById('tabellaSvg');"
    "data.forEach(function(row, rowIndex) {"
      "var tableRow = table.rows[rowIndex];"
      "var colIndex = 2;"
      "for (var key in row) {"
          "tableRow.cells[colIndex].innerText = row[key];"
          "colIndex++;"
      "}"
    "});"
  "}"
  "function insertSvgRectangles(data) {"
    "var scaleFactor = 60 / 4000;"// 60 / maxPotenza;" // Calcola il fattore di scala"
    "var table = document.getElementById('tabellaSvg');"
    "data.forEach(function(row, rowIndex) {"
      // Assicurati che la riga della tabella esista"
      "var tableRow = table.rows[rowIndex];"
      // Assumi che la seconda colonna debba contenere il rettangolo SVG"
      "var cell = tableRow.cells[1];" // Le celle sono indicizzate a partire da 0"
      // Crea l'elemento SVG"
      "var svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');"
      "svg.setAttribute('width', '61');" // Larghezza fissa dell'SVG"
      "svg.setAttribute('height', '13');" // Altezza fissa dell'SVG"
      // Crea il rettangolo SVG"
      "var rect = document.createElementNS('http://www.w3.org/2000/svg', 'rect');"
      "var rectWidth = row.potenza * scaleFactor + 3 ;"
      "rect.setAttribute('width', rectWidth);"
      "rect.setAttribute('height', '12');" // Altezza fissa del rettangolo"
      "rect.setAttribute('fill', getColorBasedOnValue(row.potenza));" // Colore di riempimento del rettangolo"
      // Aggiungi il rettangolo all'SVG, e l'SVG alla cella della tabella"
      "svg.appendChild(rect);"
      "cell.innerHTML = '';" // Pulisci il contenuto precedente della cella"
      "cell.appendChild(svg);"
    "});"
  "}"
  "function getColorBasedOnValue(value) {"
    "if (value < 0) return 'gray';" // Numeri negativi."
    "else if (value < 4) return 'black';"
    "else if (value < 250) return 'green';"
    "else if (value < 500) return 'yellow';"
    "else if (value < 1000) return 'orange';"
    "else if (value < 2000) return 'red';"
    "else if (value < 3600) return 'blue';"
    "else return 'purple';" // Valori >= 3000."
  "}"
  "</script>"
  "</body>"
  "</html>"
  );

  return html;
}

#endif PAGINEWEB_H