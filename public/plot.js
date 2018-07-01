// データベースの参照を準備
var firebaseRef = new Firebase("https://xxxxxxxx.firebaseio.com"); // ... 1
var messagesRef = firebaseRef.child('sensor').orderByChild('time').startAt(1530421800).limitToLast(6 * 24  * 7); // ... 2

var ctx_p = document.getElementById("pressure").getContext('2d');
var chart_p = new Chart(ctx_p, {
  type: 'line',
  data: {
    datasets: [{
      data: [],
      label: '気圧',
      fill: false,
      lineTension: 0.1,
      borderColor: "#FF0000",
      backgroundColor: 'rgba(255, 0, 0, 0.5)',
    }]
  },
  options: {
    scales: {
      xAxes:[{type: 'time', distribution: 'series'}],
      yAxes:[{ticks:{min: 900, max: 1100}}],
    },
  }
});
var ctx_t = document.getElementById("temparature").getContext('2d');
var chart_t = new Chart(ctx_t, {
  type: 'line',
  data: {
    datasets: [{
      data: [],
      label: '気温',
      fill: false,
      lineTension: 0.1,
      borderColor: "#FF0000",
      backgroundColor: 'rgba(255, 0, 0, 0.5)',
    }, {
      data: [],
      label: '水温',
      fill: false,
      lineTension: 0.1,
      borderColor: "#0000FF",
      backgroundColor: 'rgba(0, 0, 255, 0.5)',
    }]
  },
  options: {
    scales: {
      xAxes:[{type: 'time', distribution: 'series'}],
      yAxes:[{ticks:{min: -5, max: 40}}],
    },
  }
});
var ctx_h = document.getElementById("humidity").getContext('2d');
var chart_h = new Chart(ctx_h, {
  type: 'line',
  data: {
    datasets: [{
      data: [],
      label: '湿度',
      fill: false,
      lineTension: 0.1,
      borderColor: "#FF0000",
      backgroundColor: 'rgba(255, 0, 0, 0.5)',
    }]
  },
  options: {
    scales: {
      xAxes:[{type: 'time', distribution: 'series'}],
      yAxes:[{ticks:{min: 40, max: 100}}],
    },
  }
});
var ctx_b = document.getElementById("battery").getContext('2d');
var chart_b = new Chart(ctx_b, {
  type: 'line',
  data: {
    datasets: [{
      data: [],
      label: 'バッテリー電圧',
      fill: false,
      lineTension: 0.1,
      borderColor: "#FFFF00",
      backgroundColor: 'rgba(255, 255, 0, 0.5)',
    }]
  },
  options: {
    scales: {
      xAxes:[{type: 'time', distribution: 'series'}],
      yAxes:[{ticks:{min: 3, max: 6}}],
    },
  }
});

messagesRef.on('value', function(snapshot) {
  chart_t.update();
  chart_p.update();
  chart_h.update();
  chart_b.update();
});

messagesRef.on('child_added', function(snapshot) { // ... 3
  var msg = snapshot.val();
  var date = new moment(msg.time, 'X');
  chart_p.data.datasets[0].data.push(msg.ambient.pressure);
  chart_p.data.labels.push(date);
  chart_h.data.datasets[0].data.push(msg.ambient.humidity);
  chart_h.data.labels.push(date);
  chart_t.data.datasets[0].data.push(msg.ambient.temparature);
  chart_t.data.datasets[1].data.push(msg.aquarium.temparature);
  chart_t.data.labels.push(date);
  chart_b.data.datasets[0].data.push(msg.battery);
  chart_b.data.labels.push(date);
});
