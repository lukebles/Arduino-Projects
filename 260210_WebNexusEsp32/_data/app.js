/* app.js - compatibile Safari vecchio (iPad 2012)
   - Polyfill minimi (fill, forEach) + helper Array check
   - Menu/nav e titoli
   - WebSocket ws://host/ws con fallback HTTP XHR
   - Chart.js v2 plugin linee medie (opzionale, attivo solo dove serve)
   - Funzioni riutilizzabili per tutte le pagine
*/

(function () {
  'use strict';

  // =========================
  // Polyfill minimi
  // =========================
  if (!Array.prototype.fill) {
    Object.defineProperty(Array.prototype, 'fill', {
      value: function (value) {
        if (this == null) throw new TypeError('this is null or not defined');
        var O = Object(this);
        var len = O.length >>> 0;
        var start = arguments[1];
        var relativeStart = start >> 0;
        var k = relativeStart < 0 ? Math.max(len + relativeStart, 0) : Math.min(relativeStart, len);
        var end = arguments[2];
        var relativeEnd = end === undefined ? len : end >> 0;
        var final = relativeEnd < 0 ? Math.max(len + relativeEnd, 0) : Math.min(relativeEnd, len);
        while (k < final) { O[k] = value; k++; }
        return O;
      }
    });
  }

  if (!Array.prototype.forEach) {
    Array.prototype.forEach = function (callback, thisArg) {
      var T, k;
      if (this == null) throw new TypeError('this is null or not defined');
      var O = Object(this);
      var len = O.length >>> 0;
      if (typeof callback !== 'function') throw new TypeError(callback + ' is not a function');
      if (arguments.length > 1) T = thisArg;
      k = 0;
      while (k < len) {
        if (k in O) callback.call(T, O[k], k, O);
        k++;
      }
    };
  }

  function isArrayCompat(x) {
    return Object.prototype.toString.call(x) === '[object Array]';
  }

  function $(id) { return document.getElementById(id); }

  // =========================
  // UI: menu e testi
  // =========================
  function applyCommonTexts(pageKey) {
    // pageKey: "home" | "p1" | "p2" | "p3" | "p5"
    if (typeof window.config === 'undefined') return;

    // titolo pagina e descrizione
    if (pageKey === 'home') {
      document.title = config.titoloHome;
      if ($('descrizione')) $('descrizione').textContent = config.descrizioneHome;
    } else if (pageKey === 'p1') {
      document.title = config.titoloPagina1;
      if ($('descrizione')) $('descrizione').textContent = config.descrizione1;
    } else if (pageKey === 'p2') {
      document.title = config.titoloPagina2;
      if ($('descrizione')) $('descrizione').textContent = config.descrizione2;
    } else if (pageKey === 'p3') {
      document.title = config.titoloPagina3;
      if ($('descrizione')) $('descrizione').textContent = config.descrizione3;
    } else if (pageKey === 'p5') {
      document.title = config.titoloPagina5;
      if ($('descrizione')) $('descrizione').textContent = config.descrizione5;
    }

    // menu (se presenti)
    if ($('menuHome')) $('menuHome').textContent = config.menuHome;
    if ($('menuTitolo1')) $('menuTitolo1').textContent = config.menuTitolo1;
    if ($('menuTitolo2')) $('menuTitolo2').textContent = config.menuTitolo2;
    if ($('menuTitolo3')) $('menuTitolo3').textContent = config.menuTitolo3;
    if ($('menuTitolo4')) $('menuTitolo4').textContent = config.menuTitolo4;
    if ($('menuTitolo5')) $('menuTitolo5').textContent = config.menuTitolo5;
  }

  // =========================
  // Chart helpers (colori + medie)
  // =========================
  function getBarColor(datasetIndex, value, multiplier) {
    var yellowShades = ['#FFF9C4', '#FFF59D', '#FFF176', '#FFEB3B', '#FDD835', '#FBC02D', '#F57C00'];
    var blueShades   = ['#BBDEFB', '#90CAF9', '#64B5F6', '#42A5F5', '#2196F3', '#1E88E5', '#7B1FA2'];

    var shades = (datasetIndex === 0) ? blueShades : yellowShades;
    var v = value;
    if (multiplier && multiplier > 1) v = value / multiplier; // per riusare soglie "base"

    if (v <= 500)  return shades[0];
    if (v <= 1000) return shades[1];
    if (v <= 2000) return shades[2];
    if (v <= 3000) return shades[3];
    if (v <= 3600) return shades[4];
    if (v <= 4000) return shades[5];
    return shades[6];
  }

  function updateChartColors(chart, multiplier) {
    var i, j, dataset, value;
    for (i = 0; i < chart.data.datasets.length; i++) {
      dataset = chart.data.datasets[i];
      dataset.backgroundColor = [];
      for (j = 0; j < dataset.data.length; j++) {
        value = dataset.data[j];
        dataset.backgroundColor[j] = getBarColor(i, value, multiplier);
      }
    }
  }

  function calculateAverage(data) {
    var sum = 0, i;
    for (i = 0; i < data.length; i++) sum += data[i];
    return data.length ? (sum / data.length) : 0;
  }

  function drawAverageLines(chart, averages, totalAverage) {
    var ctx = chart.chart.ctx;
    var chartArea = chart.chartArea;
    var yScale = chart.scales['y-axis-0'];

    var i, yValue;

    // per dataset
    for (i = 0; i < averages.length; i++) {
      yValue = yScale.getPixelForValue(averages[i]);
      ctx.save();
      ctx.beginPath();
      ctx.moveTo(chartArea.left, yValue);
      ctx.lineTo(chartArea.right, yValue);
      ctx.strokeStyle = chart.data.datasets[i].borderColor;
      ctx.lineWidth = 2;
      ctx.stroke();
      ctx.restore();
    }

    // media totale (tratteggiata)
    yValue = yScale.getPixelForValue(totalAverage);
    ctx.save();
    ctx.beginPath();
    ctx.moveTo(chartArea.left, yValue);
    ctx.lineTo(chartArea.right, yValue);
    ctx.strokeStyle = 'rgba(0, 0, 0, 0.5)';
    ctx.lineWidth = 2;
    if (ctx.setLineDash) ctx.setLineDash([5, 5]); // se non esiste, ok (linea piena)
    ctx.stroke();
    ctx.restore();
  }

  function installAveragePluginIfNeeded() {
    // Chart.plugins.register esiste in v2.9.3
    if (!window.Chart || !Chart.plugins || !Chart.plugins.register) return;
    if (window.__WNX_AVG_PLUGIN_INSTALLED) return;

    Chart.plugins.register({
      afterDraw: function (chart) {
        if (!chart || !chart.data || !chart.data.datasets) return;
        var averages = [];
        var i, j, k;

        for (i = 0; i < chart.data.datasets.length; i++) {
          averages.push(calculateAverage(chart.data.datasets[i].data));
        }

        var totalSumData = [];
        for (j = 0; j < chart.data.labels.length; j++) {
          var totalSum = 0;
          for (k = 0; k < chart.data.datasets.length; k++) {
            totalSum += chart.data.datasets[k].data[j];
          }
          totalSumData.push(totalSum);
        }
        var totalAverage = calculateAverage(totalSumData);

        drawAverageLines(chart, averages, totalAverage);
      }
    });

    window.__WNX_AVG_PLUGIN_INSTALLED = true;
  }

  // =========================
  // XHR (fallback)
  // =========================
  function httpGetJSON(url, onOk, onErr) {
    var xhr = new XMLHttpRequest();
    xhr.onreadystatechange = function () {
      if (xhr.readyState !== 4) return;
      if (xhr.status >= 200 && xhr.status < 300) {
        try {
          onOk(JSON.parse(xhr.responseText));
        } catch (e) {
          if (onErr) onErr(e);
        }
      } else {
        if (onErr) onErr(new Error('HTTP ' + xhr.status));
      }
    };
    xhr.open('GET', url, true);
    xhr.send(null);
  }

  // =========================
  // WebSocket + fallback
  // =========================
  function connectWS(onMessage, onFail) {
    var wsUrl = 'ws://' + location.host + '/ws';
    var wsock;
    try {
      wsock = new WebSocket(wsUrl);
    } catch (e) {
      if (onFail) onFail(e);
      return null;
    }

    var opened = false;
    var openTimer = setTimeout(function () {
      if (!opened) {
        try { wsock.close(); } catch (e) {}
        if (onFail) onFail(new Error('WS timeout'));
      }
    }, 2500);

    wsock.onopen = function () {
      opened = true;
      clearTimeout(openTimer);
    };

    wsock.onmessage = function (ev) {
      // alcuni browser vecchi non hanno ev.data come stringa sempre? qui assumiamo stringa
      var obj;
      try { obj = JSON.parse(ev.data); } catch (e) { return; }
      onMessage(obj);
    };

    wsock.onerror = function () {
      // lascia che onclose gestisca
    };

    wsock.onclose = function () {
      if (!opened) {
        clearTimeout(openTimer);
        if (onFail) onFail(new Error('WS closed'));
      }
    };

    return wsock;
  }

  // =========================
  // Util: etichette "ridotte" per ore
  // =========================
  function getDifferentPart(str1, str2) {
    var words1 = str1.split(' ');
    var words2 = str2.split(' ');
    var minLength = Math.min(words1.length, words2.length);
    var diffWords = [];
    var i;
    for (i = 0; i < minLength; i++) {
      if (words1[i] !== words2[i]) diffWords.push(words2[i]);
    }
    for (i = minLength; i < words2.length; i++) diffWords.push(words2[i]);
    return diffWords.join(' ');
  }

  // =========================
  // INIT: pagine grafico (home/p1/p2)
  // =========================
  function initChartPage(opts) {
    // opts:
    //  pageKey: "home"|"p1"|"p2"
    //  wsCommand: "getPowerData"|"getHourEnergy"|"getDaysEnergy"
    //  httpUrl: "/api/instant"|"/api/hours"|"/api/days"
    //  labelMode: "instant"|"hours"|"days"
    //  colorMultiplier: 1 (instant/hours) or 4 (days)
    //  useAveragePlugin: true|false

    installAveragePluginIfNeeded();

    applyCommonTexts(opts.pageKey);

    var ctx = $('myChart') ? $('myChart').getContext('2d') : null;
    if (!ctx || !window.Chart) return;

    var labels = new Array(31).fill('');
    var data0 = new Array(31).fill(0);
    var data1 = new Array(31).fill(0);

    var chartData = {
      labels: labels,
      datasets: [
        {
          label: config.etichettaPotenzaAttiva,
          backgroundColor: '#BBDEFB',
          borderColor: '#1976D2',
          borderWidth: 1,
          data: data0
        },
        {
          label: config.etichettaPotenzaReattiva,
          backgroundColor: '#FFF9C4',
          borderColor: '#F9A825',
          borderWidth: 1,
          data: data1
        }
      ]
    };

    var chartCfg = {
      type: 'bar',
      data: chartData,
      options: {
        responsive: true,
        scales: {
          xAxes: [{ stacked: true }],
          yAxes: [{ stacked: true }]
        }
      }
    };

    var chart = new Chart(ctx, chartCfg);

    function setLastRadio(text) {
      if ($('lastRadio')) $('lastRadio').innerText = text || '';
    }

    function applyArrayPayload(arr) {
      var i;
      for (i = 0; i < 31; i++) {
        var dp = arr[i] || {};
        if (opts.labelMode === 'instant') {
          chart.data.labels[i] = (dp.timestamp || '') + ' (' + (dp.timeDiff || 0) + 'ms)';
          chart.data.datasets[0].data[i] = dp.activePower || 0;
          chart.data.datasets[1].data[i] = dp.reactivePower || 0;
        } else if (opts.labelMode === 'hours') {
          if (i > 0 && arr[i - 1] && arr[i - 1].timestamp) {
            chart.data.labels[i] = getDifferentPart(arr[i - 1].timestamp, dp.timestamp || '');
          } else {
            chart.data.labels[i] = dp.timestamp || '';
          }
          chart.data.datasets[0].data[i] = dp.activeEnergy || 0;
          chart.data.datasets[1].data[i] = dp.reactiveEnergy || 0;
        } else { // days
          chart.data.labels[i] = dp.timestamp || '';
          chart.data.datasets[0].data[i] = dp.activeEnergy || 0;
          chart.data.datasets[1].data[i] = dp.reactiveEnergy || 0;
        }
      }

      updateChartColors(chart, opts.colorMultiplier || 1);
      chart.update();
    }

    function applyLivePoint(obj) {
      // per home arrivano punti live; per p1/p2 può arrivare solo timestampLong (dal server)
      if (obj.timestampLong) setLastRadio(obj.timestampLong);

      if (opts.labelMode !== 'instant') {
        updateChartColors(chart, opts.colorMultiplier || 1);
        chart.update();
        return;
      }

      // HOME live update
      chart.data.labels.push(obj.timestamp + ' (' + obj.timeDiff + 'ms)');
      chart.data.datasets[0].data.push(obj.activePower);
      chart.data.datasets[1].data.push(obj.reactivePower);

      if (chart.data.datasets[0].data.length > 31) {
        chart.data.labels.shift();
        chart.data.datasets[0].data.shift();
        chart.data.datasets[1].data.shift();
      }

      updateChartColors(chart, opts.colorMultiplier || 1);
      chart.update();
    }

    // Prova WebSocket; se fallisce, fai HTTP polling
    var wsock = connectWS(function (msg) {
      if (isArrayCompat(msg)) applyArrayPayload(msg);
      else applyLivePoint(msg);
    }, function () {
      // fallback HTTP: 1 caricamento + polling /api/last
      httpGetJSON(opts.httpUrl, function (arr) {
        applyArrayPayload(arr);
      });

      // aggiorna timestampLong se disponibile
      setInterval(function () {
        httpGetJSON('/api/last', function (obj) {
          if (obj && obj.timestampLong) setLastRadio(obj.timestampLong);
        });
      }, 3000);
    });

    // Se WS ok, manda comando richiesta
    if (wsock) {
      // attendo un attimo che sia open (Safari vecchio a volte è lento)
      var t = setInterval(function () {
        if (wsock.readyState === 1) {
          clearInterval(t);
          wsock.send(opts.wsCommand);
        }
      }, 100);
      setTimeout(function(){ clearInterval(t); }, 4000);
    }
  }

  // =========================
  // INIT: page3 (solo timestamp)
  // =========================
  function initStatusOnlyPage(pageKey) {
    applyCommonTexts(pageKey);

    function setLastRadio(text) {
      if ($('lastRadio')) $('lastRadio').innerText = text || '';
    }

    var wsock = connectWS(function (msg) {
      if (msg && msg.timestampLong) setLastRadio(msg.timestampLong);
    }, function () {
      // fallback polling
      setInterval(function () {
        httpGetJSON('/api/last', function (obj) {
          if (obj && obj.timestampLong) setLastRadio(obj.timestampLong);
        });
      }, 3000);
    });

    // non serve inviare comandi
    (void)wsock;
  }

  // =========================
  // INIT: page5 (settings)
  // =========================
  function initSettingsPage() {
    applyCommonTexts('p5');

    var setTimeButton = $('setTimeButton');
    var testAlarmButton = $('testAlarmButton');
    var powerLimitInput = $('powerLimitInput');
    var setPowerLimitButton = $('setPowerLimitButton');
    var saveButton = $('saveButton');

    function setLastRadio(text) {
      if ($('lastRadio')) $('lastRadio').innerText = text || '';
    }

    function updatePowerLimit(val) {
      if (powerLimitInput) powerLimitInput.value = val;
    }

    var wsock = connectWS(function (msg) {
      // powerLimit response
      if (msg && typeof msg.powerLimit !== 'undefined') {
        updatePowerLimit(msg.powerLimit);
        return;
      }
      if (msg && msg.timestampLong) setLastRadio(msg.timestampLong);
    }, function () {
      // fallback: leggi powerLimit via HTTP
      httpGetJSON('/api/powerLimit', function (obj) {
        if (obj && typeof obj.powerLimit !== 'undefined') updatePowerLimit(obj.powerLimit);
      });
      // e polling timestamp
      setInterval(function () {
        httpGetJSON('/api/last', function (obj) {
          if (obj && obj.timestampLong) setLastRadio(obj.timestampLong);
        });
      }, 3000);
    });

    // se WS c'è, chiedi power limit
    if (wsock) {
      var t = setInterval(function () {
        if (wsock.readyState === 1) { clearInterval(t); wsock.send('getPowerLimit'); }
      }, 100);
      setTimeout(function(){ clearInterval(t); }, 4000);
    }

    // event handlers (compat vecchia: addEventListener esiste)
    if (setTimeButton) {
      setTimeButton.addEventListener('click', function () {
        var now = new Date();
        var dateTimeString =
          now.getFullYear() + '-' +
          ('0' + (now.getMonth() + 1)).slice(-2) + '-' +
          ('0' + now.getDate()).slice(-2) + ' ' +
          ('0' + now.getHours()).slice(-2) + ':' +
          ('0' + now.getMinutes()).slice(-2) + ':' +
          ('0' + now.getSeconds()).slice(-2);

        if (wsock && wsock.readyState === 1) wsock.send('setTime:' + dateTimeString);
        setTimeButton.textContent = 'Data/Ora Aggiornata';
      });
    }

    if (testAlarmButton) {
      testAlarmButton.addEventListener('click', function () {
        if (wsock && wsock.readyState === 1) wsock.send('ALARM-TEST');
      });
    }

    if (setPowerLimitButton) {
      setPowerLimitButton.addEventListener('click', function () {
        var v = powerLimitInput ? powerLimitInput.value : '';
        if (/^\d{1,6}$/.test(v)) {
          if (wsock && wsock.readyState === 1) wsock.send('POWER-LIMIT=' + v);
        } else {
          alert('Inserisci un valore numerico valido (massimo 6 cifre).');
        }
      });
    }

    if (saveButton) {
      saveButton.addEventListener('click', function () {
        if (wsock && wsock.readyState === 1) wsock.send('SAVE');
        saveButton.textContent = 'Dati aggiornati';
      });
    }
  }

  // =========================
  // Esporta API globale
  // =========================
  window.WNX = {
    initChartPage: initChartPage,
    initStatusOnlyPage: initStatusOnlyPage,
    initSettingsPage: initSettingsPage
  };
})();
