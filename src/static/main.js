
//global vars :(
//let chart1Data = [];
//let gaugeChart = null;
let rtb = null;
const colors = ['#4ac43d','#bd0808','#ff8030','#2b7df0']
const colordict = {"energy":'#4ac43d',"voltage":'#bd0808',"current":'#ff8030',"power":'#2b7df0'};
const nameDict = {"energy":'Energy [Wh]',"voltage":'Voltage [V]',"current":'Current [A]',"power":'Power [W]'};
const days = ["Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"];

//unitDict = {}

//Helper function to slice array
Array.prototype.sliceStep = function(b,e,s) {
  var n=[];
  for (var i=b; i<e; i+=s) {
      n.push(this[i]);
  }
  return n;
}

//Broker for real time data update
class RealTimeBroker {
  constructor(){
    this.data = Array(120).fill().map(() => [Math.floor((new Date()).getTime()/1000),0,0,0]);
    this.previousTS = 0;
    this.isRunning = false;
    this.timeInterval = null;
  }

  setCallback(gaugeCallback,rtCallback,infoCallback){
    this.gaugeCallback = gaugeCallback;
    this.rtCallback = rtCallback;
    this.infoCallback = infoCallback;
  }

  start(){
    function getRT(){
      $.get('energy/real_time', (data,status) => {
        if(status == "success" && data["status"] == "ok"){

          let deviceStatus = data["device_status"];
          let c = document.getElementById("circle_light")
          if(deviceStatus == "offline"){
            document.getElementById("status_text").innerHTML = "Logger offline"
            c.style.backgroundColor = "red";
            c.style.boxShadow = "0px 0px 5px 2px #ff0000";
          }
          else if(deviceStatus == "online"){
            document.getElementById("status_text").innerHTML = "Logger online"
            c.style.backgroundColor = "#00ff00";
            c.style.boxShadow = "0px 0px 5px 2px #00ff00";
          }

          let values = [data["value"]["timestamp"],
                        data["value"]["energy"],
                        data["value"]["voltage"],
                        data["value"]["current"]];
          rtb.update(values);
        }
        
      });
    }
    if(!(this.isRunning)){
      this.timeInterval = setInterval(getRT,5000);
      this.isRunning = true;
    }
    
  }

  stop(){
    if(this.timeInterval != null){
      clearInterval(this.timeInterval);
      this.isRunning = false;
    }
  }

  update(val){
    this.gaugeCallback(val);
    this.rtCallback(val);
    this.infoCallback(val);
  }
}

//Update selected chart tab when window get resized
window.addEventListener('resize', () => {
  let divList = document.getElementsByClassName('curve_chart1_legend_div');
  for(let v of divList){
    if(v.childNodes[1].childNodes[1].checked == true){
      v.click();
      break;
    }
  }
  divList = document.getElementsByClassName('rt_line_chart_legend_div');
  for(let v of divList){
    if(v.childNodes[1].childNodes[1].checked == true){
      v.click();
      break;
    }
  }
});

//Initialization
window.addEventListener('DOMContentLoaded', () => {
  /*
  var button1 = document.getElementById("btn1");
  button1.addEventListener("click", () => {console.log("element of surprise");});
  */ 

  var sendQueryBtn = document.getElementById("sendQueryBtn");
  sendQueryBtn.addEventListener("click", getQueryData);


  var fromDate = document.getElementById("date-from");
  var toDate = document.getElementById("date-to");
  var fromTime = document.getElementById("time-from");
  var toTime = document.getElementById("time-to");
  
  const currentDate = new Date();

  var currentTS = currentDate.getTime();
  console.log(currentTS);
  var startDate = new Date(currentTS)
  startDate.setHours(0,0,0);
  var stopDate = new Date(startDate.getTime()+24*60*60*1000);
  //stopDate.setHours(23,59,59);

  startDate = new Date(startDate.getTime() - startDate.getTimezoneOffset()*60*1000);
  stopDate = new Date(stopDate.getTime() - stopDate.getTimezoneOffset()*60*1000);
  let startS = startDate.toISOString().slice(0,-8).split('T');
  let stopS = stopDate.toISOString().slice(0,-8).split('T');

  
  fromDate.value = startS[0];
  fromTime.value = startS[1];
  toDate.value = stopS[0];
  toTime.value = stopS[1];

  let toTsHeatMap = new Date(currentTS)
  toTsHeatMap.setMinutes(0,0);
  toTsHeatMap = toTsHeatMap.getTime();
  let fromTsHeatMap = toTsHeatMap - 28*24*60*60*1000;
  toTsHeatMap += 59*60*1000;
  loadCharts(fromTsHeatMap,toTsHeatMap);
});


//Set callbacks
google.charts.load('50', {packages:['line','corechart','treemap','gauge']});
google.charts.setOnLoadCallback(loadGoogleCharts);

function loadCharts(fromTS,toTS){
  drawHeatMap1(fromTS,toTS);
}

function loadGoogleCharts(){
  console.log("loading complete");
  getQueryData();
  rtb = new RealTimeBroker();
  let gaugeCallback = drawGaugeCharts();
  let rtCallback = drawRtLine1(rtb.data);
  let infoCallback = drawInfo();
  rtb.setCallback(gaugeCallback,rtCallback,infoCallback);
  rtb.start();
}

//Get historic data by date-time form
function getQueryData(){

  var overlay = document.getElementById("load_overlay");
  overlay.style.display = "flex"
  let f = function(){
    alert("Unable to retrieve query data")
    overlay.style.display = "none"
  }
  let queryTimer = setTimeout(f,10000);

  var fromDate = document.getElementById("date-from");
  var toDate = document.getElementById("date-to");
  var fromTime = document.getElementById("time-from");
  var toTime = document.getElementById("time-to");

  let fromTS = Math.floor(Date.parse(fromDate.value+'T'+fromTime.value)/1000);
  let toTS = Math.floor(Date.parse(toDate.value+'T'+toTime.value)/1000);

  if(typeof fromTS == "number" && typeof toTS == "number" && fromTS<=toTS){
    var mode = 'day'
    var padding = 24*60*60-1;
    if(toTS-fromTS <= 7*24*60*60){ mode = 'hour'; padding = 60*60-1; }
    if(toTS-fromTS <= 1*24*60*60){ mode = 'all'; padding = 5-1;}
    let tzoffset = -(new Date()).getTimezoneOffset()*60
    $.get(`/energy/get_data?from_ts=${fromTS}&to_ts=${toTS}&mode=${mode}&offset=${tzoffset}&types=all`, 
    function(data,status){
      console.log(status);
      console.log(data);
      drawLineChart1(data.values,data.types)
      clearTimeout(queryTimer);
      overlay.style.display = "none"
    });
  }
  else{
    alert("Invalid date inputs.");
    clearTimeout(queryTimer);
    overlay.style.display = "none"
    return;
  }
  
}

//Initialize status and info 
//return update callback
function drawInfo(){
  let dict = {};
  let tzoffset = -(new Date()).getTimezoneOffset()*60;
  $.get(`/energy/initialize?offset=${tzoffset}`, 
    function(data,status){

      if(status != "success" || data["status"] != "ok") return;
        
      let deviceStatus = data["device_status"];
      let c = document.getElementById("circle_light")
      if(deviceStatus == "offline"){
        document.getElementById("status_text").innerHTML = "Logger offline"
        c.style.backgroundColor = "red";
        c.style.boxShadow = "0px 0px 5px 2px #ff0000";
      }
      else if(deviceStatus == "online"){
        document.getElementById("status_text").innerHTML = "Logger online"
        c.style.backgroundColor = "#00ff00";
        c.style.boxShadow = "0px 0px 5px 2px #00ff00";
      }
      let value = data["value"];
      for(let v of Object.keys(value)){
        elem = document.getElementById(v);
        elem.innerHTML = Math.round(value[v]) + ' Wh';
        dict[v] = [elem, value[v]];
      }  
    }
  );

  function updateInfo(rtdata){
    let tot = rtdata[1];
    if(tot <= 0) return;
    for(let d of Object.keys(dict)){
      let val = tot - dict[d][1]
      if(d == "tot") val = tot;
      dict[d][0].innerHTML = Math.round(val) + ' Wh';
    }
  }

  return updateInfo;
}


//Display historic data chart
//Create buttons to switch value tab visualization
//return update callback
function drawLineChart1(data = [], types = []) {
    document.getElementById('curve_chart1_legend').innerHTML = '';
    if(types.length == 0) return;
    let ts_index = types.findIndex((x) => {return x=="timestamp"});
    let en_index = types.findIndex((x) => {return x=="energy"});
    let vv_index = types.findIndex((x) => {return x=="voltage"});
    let cc_index = types.findIndex((x) => {return x=="current"});
    let pw_index = -1;
    /*let ts_index = types.findIndex("timestamp");
    let en_index = types.findIndex("energy");
    let vv_index = types.findIndex("voltage");
    let cc_index = types.findIndex("current");*/
    var ndata = [];
    if(vv_index>=0 && cc_index>=0){
      types.push("power");
      pw_index = types.length-1;
    }
    if(data.length>500) ndata = data.sliceStep(0,data.length,Math.floor(data.length/500));
    else ndata = data;
    ndata.forEach((x,index) => {
      if(index >= ndata.length - 1) return;
      x[ts_index] = new Date(x[ts_index]*1000);
      if(vv_index>=0 && cc_index>=0) x.push(x[vv_index]*x[cc_index]);
      x[en_index] = ndata[index+1][en_index] - x[en_index];
    });
    ndata.pop();
    ndata = [types].concat(ndata);
    var dataTable = google.visualization.arrayToDataTable(ndata);
    var fm_en = new google.visualization.NumberFormat({suffix: ' Wh'});
    var fm_vv = new google.visualization.NumberFormat({suffix: ' V'});
    var fm_cc = new google.visualization.NumberFormat({suffix: ' A'});
    var fm_pw = new google.visualization.NumberFormat({suffix: ' W'});
    if(en_index != -1) fm_en.format(dataTable,en_index);
    if(vv_index != -1) fm_vv.format(dataTable,vv_index);
    if(cc_index != -1) fm_cc.format(dataTable,cc_index);
    if(pw_index != -1) fm_pw.format(dataTable,pw_index);
    var gdata = new google.visualization.DataView(dataTable);

    var options = {
      colors: colors,
      hAxis: {
        //format: 'dd/MM/yyyy-HH:mm:ss'
        format: 'HH:mm:ss'
      },
      backgroundColor: '#f2f3ee',
      chartArea:{
        backgroundColor: {
          fill:'#f2f3ee',
          opacity: 100
        }
      },
      title: 'Grafico Dati',
      //curveType: 'function',
      legend: { position: 'bottom' },
      animation:{
        duration: 1000,
        easing: 'inAndOut',
        startup: true
      },
    };

    var chartColumns = [0,1];
    gdata.setColumns(chartColumns);
    let charDiv = document.getElementById('curve_chart1');
    let legendDiv = document.getElementById('curve_chart1_legend');
    

    var chart = new google.charts.Line(charDiv);
    chart.draw(gdata, google.charts.Line.convertOptions(options));
    //var chart = new google.visualization.LineChart(charDiv);

    //chart.draw(gdata, options);

    let list = document.createElement('ul');
    list.id = "curve_chart1_legend_list";

    types.forEach((x,index) => {
      if(x == 'timestamp') return;
      let label = document.createElement("label");
      label.innerText = nameDict[x];
      label.className = "radio_label";
      let input = document.createElement("input");
      input.type = 'radio';
      input.name = 'chart1_legend';
      input.style.accentColor = colordict[x];
      /*
      input.onclick = () => {
        gdata.setColumns([0,index]);
        options.colors = [colordict[x]];
        chart.draw(gdata, google.charts.Line.convertOptions(options));
      };
      */
      label.appendChild(input);
      let circle = document.createElement('div');
      circle.style.backgroundColor = colordict[x];
      circle.className = "circle";
      let div = document.createElement("div");
      div.className = "curve_chart1_legend_div";
      div.appendChild(circle);
      div.appendChild(label);
      div.onclick = () => {
        var divs = document.getElementsByClassName("curve_chart1_legend_div");
        for(let d of divs) d.style.opacity = 0.5;
        div.style.opacity = 1;
        input.checked = true;
        gdata.setColumns([0,index]);
        options.colors = [colordict[x]];
        chart.draw(gdata, google.charts.Line.convertOptions(options));
      };
      if(index == 1){
        input.checked = true;
        div.style.opacity = 1; 
      }
      let item = document.createElement("li");
      item.className = "curve_chart1_legend_item";
      item.appendChild(div);
      list.appendChild(item);
    });

    legendDiv.appendChild(list);

}

//Display real time data chart
//Create buttons to switch value tab visualization
//return update callback
function drawRtLine1(data) {
  document.getElementById('rt_line_chart_legend').innerHTML = '';
  let currentDiv = null;
  let currentIndex = 4;
  var ndata = data;
  ndata.forEach((x) => {
    x[0] = new Date(x[0]*1000);
    x.push(x[2]*x[3]);
  });
  types = ["timestamp","energy","voltage","current","power"];
  units = [" Wh"," V"," A"," W"];
  ndata = [types].concat(ndata);
  var dataTable = google.visualization.arrayToDataTable(ndata);
  for(let i = 0; i< units.length; i++){
    let fm = new google.visualization.NumberFormat({suffix: units[i]});
    fm.format(dataTable,i+1);
  }
  var gdata = new google.visualization.DataView(dataTable);


  var options = {
    colors: colors,
    hAxis: {
      format: 'HH:mm:ss'
    },
    backgroundColor: '#f2f3ee',
    chartArea:{
      backgroundColor: {
        fill:'#f2f3ee',
        opacity: 100
      }
    },
    title: 'Dati in tempo reale',
    legend: { position: 'bottom' },
    animation:{
      duration: 1000,
      easing: 'inAndOut',
      startup: true
    },
  };

  var chartColumns = [0,4];
  gdata.setColumns(chartColumns);
  options.colors = [colors[3]];
  let charDiv = document.getElementById('rt_line_chart');
  let legendDiv = document.getElementById('rt_line_chart_legend');
  
  var chart = new google.charts.Line(charDiv);
  chart.draw(gdata, google.charts.Line.convertOptions(options));

  let list = document.createElement('ul');
  list.id = "rt_line_chart_legend_list";

  types.forEach((x,index) => {
    if(x == 'timestamp') return;
    let label = document.createElement("label");
    label.innerText = nameDict[x];
    label.className = "radio_label";
    let input = document.createElement("input");
    input.type = 'radio';
    input.name = 'rt_chart_legend';
    input.style.accentColor = colordict[x];

    label.appendChild(input);
    let circle = document.createElement('div');
    circle.style.backgroundColor = colordict[x];
    circle.className = "circle";
    let div = document.createElement("div");
    div.className = "rt_line_chart_legend_div";
    div.appendChild(circle);
    div.appendChild(label);
    div.onclick = () => {
      currentDiv = div;
      currentIndex = index;
      var divs = document.getElementsByClassName("rt_line_chart_legend_div");
      for(let d of divs) d.style.opacity = 0.5;
      div.style.opacity = 1;
      input.checked = true;
      gdata.setColumns([0,index]);
      options.colors = [colordict[x]];
      chart.draw(gdata, google.charts.Line.convertOptions(options));
    };
    if(index == 4){
      currentDiv = div;
      input.checked = true;
      div.style.opacity = 1; 
    }
    let item = document.createElement("li");
    item.className = "rt_line_chart_legend_item";
    item.appendChild(div);
    list.appendChild(item);
  });
  legendDiv.appendChild(list);

  function updateCharts(rtdata){
    dataTable.addRow([new Date(rtdata[0]*1000), rtdata[1], rtdata[2], rtdata[3], rtdata[2]*rtdata[3]]);
    dataTable.removeRow(0);
    for(let i = 0; i< units.length; i++){
      let fm = new google.visualization.NumberFormat({suffix: units[i]});
      fm.format(dataTable,i+1);
    }
    gdata = new google.visualization.DataView(dataTable);
    gdata.setColumns([0,currentIndex]);
    chart.draw(gdata, google.charts.Line.convertOptions(options));
  }

  return updateCharts;
}


//Display heatmap of monthly consumption stat
function drawHeatMap1(fromTS, toTS){

  if(typeof fromTS != "number" || typeof toTS != "number"){
    alert("Invalid date inputs.");
    return;
  }

  let tzoffset = -(new Date()).getTimezoneOffset()*60;
  $.get(`/energy/get_data?from_ts=${Math.floor(fromTS/1000)}&to_ts=${Math.floor(toTS/1000)}&types=energy&mode=hour&offset=${tzoffset}`, 
    function(data,status){
      
      if(status != "success" || data["status"] != "ok") return;

      let ts_index = data["types"].findIndex((x) => {return x=="timestamp"});
      let en_index = data["types"].findIndex((x) => {return x=="energy"});
      let values = Array(7).fill().map(()=>Array(24).fill().map(() => []));
      let count = Array(7).fill().map(()=>Array(24).fill(0));
      data["values"].forEach((x,index) => {
      if(index>=data["values"].length-1) return;
        let d = new Date(x[ts_index]*1000);
        let v = data["values"][index+1][en_index]-x[en_index];
        values[d.getDay()][d.getHours()].push(v);
        count[d.getDay()][d.getHours()] += 1;
      });
      /*.map((x) => {
        let d = new Date(x[ts_index]);
        return {"day":d.getDay(), "hour":d.getHours(), "energy":x[en_index]}})
      .reduce((dict,x) => {
        if(!(x["day"] in dict)) dict[x["day"]] = {};
        if(!(x["hour"] in dict[x["day"]])) dict[x["day"]][x["hour"]] = [];
        dict[x["day"]][x["hour"]].push(x["energy"]);
      },{})*/
      let graphData = []
      for(let x=0; x<24; x++){
      for(let y=1; y<8; y++){
        let heat = 0;
        let count = 0;
        for(let k=0; k<values[y%7][x].length; k++){
          heat += values[y%7][x][k];
          count += 1;
        }
        if(count != 0) heat /= count;
        graphData.push({x:x, y:days[(y%7)], heat:heat});
      }
      }
      chart = anychart.heatMap(graphData);
      let title = chart.title();
      title.enabled(true);
      title.align("left");
      title.text("HeatMap consumo ultimo mese");
      let customColorScale = anychart.scales.linearColor();
      customColorScale.colors([colors[0], '#ffd60a', colors[1]]);
      chart.colorScale(customColorScale);
      let tooltip = chart.tooltip();
      tooltip.titleFormat("{%y} {%x}:00");
      chart.container("map_chart1");
      chart.background().fill('#f2f3ee');
      chart.draw();
  });
}

//Draw gauge charts
//return update callback
function drawGaugeCharts(){

    var options1 = {
      min:200, max:250,
      width: 400, height: 150,
      greenFrom:200, greenTo: 250,
      yellowFrom:210, yellowTo: 240,
      redFrom: 215, redTo: 235,
      minorTicks: 5
    };

    var options2 = {
      min:0, max:20,
      width: 400, height: 150,
      greenFrom: 0, greenTo: 10,
      redFrom: 15, redTo: 20,
      yellowFrom:10, yellowTo: 15,
      minorTicks: 5
    };

    var options3 = {
      min: 0, max:4,
      width: 400, height: 150,
      redFrom: 3, redTo: 4,
      yellowFrom:2.5, yellowTo: 3,
      greenFrom:0, greenTo:2.5,
      minorTicks: 5
    }

    var chart1 = new google.visualization.Gauge(document.getElementById('gauge_1'));
    var chart2 = new google.visualization.Gauge(document.getElementById('gauge_2'));
    var chart3 = new google.visualization.Gauge(document.getElementById('gauge_3'));

    var charts = [chart1,chart2,chart3];
    var opts = [options1,options2,options3]
    
    function updateCharts(rtdata) {
      var d = google.visualization.arrayToDataTable([
        ['Label', 'Value'],
        ['Voltage', rtdata[2]],
        ['Current', rtdata[3]],
        ['Power', rtdata[2]*rtdata[3]/1000]
      ]);

      for(let i=0; i<3; i++){
        let dw = new google.visualization.DataView(d);
        dw.setRows([i]);
        charts[i].draw(dw,opts[i]);
      }

      $('#gauge_1 > table > tbody > tr > td > div > div:nth-child(1) > div > svg > g > path:nth-child(3)').attr('fill','#dc3912');
      $('#gauge_1 > table > tbody > tr > td > div > div:nth-child(1) > div > svg > g > path:nth-child(5)').attr('fill','#109618');
    } 

    updateCharts([0,0,0,0]);

    return updateCharts;
}
