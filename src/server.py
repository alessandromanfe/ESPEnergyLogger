from flask import Flask, Response, request, send_from_directory, jsonify
import dbmanager as dbm

sendParam = ["timestamp","energy","voltage","current"]
getParam = ["from_ts","to_ts","mode","types","offset"]

app = Flask(__name__,static_url_path='/static')

@app.route('/')
def page():
    return send_from_directory('static', "page.html")

@app.route('/energy')
def page():
    return send_from_directory('static', "page.html")

@app.route('/energy/send_data')
def sendData():
    ip = request.environ['HTTP_X_FORWARDED_FOR']
    if ip is None or not isinstance(ip, str) or ip[:10] != '192.168.1.':
        return '', 403
    args = {}
    for p in sendParam:
        args[p] = request.args.get(p)
    dbm.processLogData(args)
    return '', 200

@app.route('/energy/real_time')
def realTime():
    return jsonify(dbm.getRealTimeData())

@app.route('/energy/initialize')
def initialize():
    args = {}
    args["offset"] = request.args.get("offset")
    return jsonify(dbm.getInitializeData(args))

@app.route('/energy/get_data')
def getData():
    args = {}
    for p in getParam:
        args[p] = request.args.get(p)
    x = dbm.getLogData(args)
    return x

@app.route('/energy/get_hour_data')
def getHourData():
    return

if __name__ == "__main__":
    app.run(debug=True, host="0.0.0.0", port=5000)
