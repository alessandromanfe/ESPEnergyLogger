import sqlite3
from collections import defaultdict
import random
from datetime import datetime
import time
import apscheduler
import apscheduler.schedulers
import apscheduler.schedulers.background

#DBPATH = "../data/energyLogs.db"
DBPATH = "./data/energyLogs.db"

QUEUE_TIME = 60

TYPELIST = ["energy","voltage","current"]
MODELIST = ["day","hour","all"]
logQueue = []
lastValue = None
prevTS = 0


# Flush all data from queue to DB
def flushQueue():
    if len(logQueue) == 0:
        return
    values = [tuple(d.values()) for d in logQueue]
    conn = sqlite3.connect(DBPATH)
    with conn:
        cur = conn.cursor()
        cur.executemany("INSERT INTO energy_logs (timestamp,energy,voltage,current)VALUES(?, ?, ?, ?);", values)
    conn.close()
    logQueue.clear()


# Process incoming data from logger
# store value in queue and update required vars  
def processLogData(args):
    global logQueue
    global lastValue
    global prevTS
    
    #Parse args
    if args["timestamp"] is None or args["energy"] is None:
        return
    if args["voltage"] is None:
        args["voltage"] = 0
    if args["current"] is None:
        args["current"] = 0

    for k in args:
        if k == "timestamp":
            args[k] = int(args[k])
        else:
            args[k] = float(args[k])

    #Update queue and vars
    if lastValue is None:
        prevTS = args["timestamp"]
    else:
        prevTS = lastValue["timestamp"]
    lastValue = args
    logQueue.append(args)


# Retrieve historic data from DataBase
# according to period defined by timestamp args
# also checks queue for unflushed data
def getLogData(args):
    global logQueue

    #Parse args and setting parameters for the sql query
    if (args["from_ts"] is None or args["to_ts"] is None 
        or (not args["from_ts"].isdigit()) or (not args["to_ts"].isdigit())
        or int(args["to_ts"])<int(args["from_ts"])):
        return {"status":"error","reason":"invalid parameters"}
    mode = args["mode"]
    if mode is None or mode not in MODELIST:
        mode = 'day'
    types = args["types"]
    if mode != 'all' and types is None:
        types = "energy" 
    if types is None or types == "all":
        types = ["energy","voltage","current"]
    else:
        types = types.split(',')
        types = [t for t in TYPELIST if t in types]
    types = ["timestamp"] + types
    offset = args["offset"]
    if offset is None or not offset.isdigit():
        offset = '0'
    offset = int(offset)
    from_ts = int(args["from_ts"])
    to_ts = int(args["to_ts"])
    divider = None
    if mode == 'day':
        divider = 86400
    if mode == 'hour':
        divider = 3600


    #Retrieve data from logQueue if present
    values = []
    if len(logQueue) != 0 and logQueue[0]["timestamp"] <= to_ts:
        values = [tuple([v[t] for t in types]) for v in logQueue if v["timestamp"]<=to_ts and v["timestamp"]>=from_ts]
    if len(logQueue) != 0 and logQueue[0]["timestamp"] <= from_ts:
        return {"status":"ok", "types":types, "values":values}
    
    #Build sql query
    if divider is not None:
        dd = defaultdict(lambda : [])
        [dd[(v[0]+offset)//divider].append(v) for v in values]
        values = [min(dd[d], key= lambda x: x[0]) for d in dd]
        sqlStr = ('''SELECT MIN(timestamp),{}
                    FROM energy_logs
                    WHERE timestamp BETWEEN {} AND {}
                        AND ((timestamp+{})/60)%{} == 0
                    GROUP BY (timestamp+{})/{};'''
                    .format(",".join(types[1:]), from_ts, to_ts, offset, divider//60, offset, divider))  
    else:
        sqlStr = ('''SELECT {} 
                    FROM energy_logs 
                    WHERE timestamp BETWEEN {} AND {}
                    ORDER BY timestamp ASC;'''
                    .format(",".join(types), from_ts, to_ts))
        
    #Execute query and precess data
    conn = sqlite3.connect(DBPATH)
    with conn:
        cur = conn.cursor()
        cur.execute(sqlStr)
        result = cur.fetchall()
    conn.close()
    if divider is not None and len(values)!=0 and result[-1][0]//divider == values[0][0]//divider:
        values = values[1:]
    values = result + values

    return {"status":"ok", "types":types, "values":values}


# Get last logger value
# return dummy value if not present
# (called by js broker to update webpage)
def getRealTimeData():
    global lastValue
    global prevTS

    lv = lastValue
    device_status = "online"
    if lv is None or int(round(time.time()))-lv["timestamp"]>30:
        now = int(datetime.now().timestamp())
        lv = {"timestamp":now,"energy":0,"voltage":0,"current":0}
        device_status = "offline"
    return {"status": "ok", "value":lv, "device_status":device_status}


# Get data to initialize logger device or web page
# (daily partial, monthly partial, total) energy consumption
# if required data is not yet available execute db query
def getInitializeData(args):
    global lastValue

    #Parse args and get 
    offset = args["offset"]
    if offset is None:
        offset = time.localtime().tm_gmtoff
    fromTS = int(datetime.now().replace(day=1,hour=0,minute=0,second=0,microsecond=0).timestamp())
    toTS = int(datetime.now().timestamp())

    #Get last energy value, update if necessary
    lv = lastValue
    deviceStatus = "offline"
    if lv is not None:
        if toTS - lv["timestamp"]<15:
            deviceStatus = "online"
        lv = lv["energy"]
    elif len(logQueue) != 0:
        lv = logQueue[-1]["energy"]
    else:
        sqlStr = ('''SELECT timestamp,energy
                            FROM energy_logs
                            ORDER BY timestamp DESC LIMIT 1;''')
        conn = sqlite3.connect(DBPATH)
        with conn:
            cur = conn.cursor()
            cur.execute(sqlStr)
            lv = cur.fetchall()
        conn.close()
        if lv is None or len(lv) == 0:
            lv = 0
        else:
            lv = lv[0][1]
        
    #Get daily value
    sqlStr = ('''SELECT timestamp,energy
                        FROM energy_logs
                        WHERE timestamp BETWEEN {} AND {}
                            AND ((timestamp+{})/60)%{} == 0
                        GROUP BY (timestamp+{})/{};'''
                        .format(fromTS, toTS, offset, 86400//60, offset, 86400))
    
    conn = sqlite3.connect(DBPATH)
    with conn:
        cur = conn.cursor()
        cur.execute(sqlStr)
        result = cur.fetchall()
    conn.close()
    #Get partial energy consumption (day,month)
    if result is None or len(result) == 0:
        mv = 0
        dv = 0
    else:
        mv = result[0][1]
        dv = result[-1][1]
    
    return {"status": "ok", "value": {"day":lv-dv, "month":lv-mv, "tot":lv}, "device_status":deviceStatus}


#Create scheduler to run job to flush queue into DB
queueScheduler = apscheduler.schedulers.background.BackgroundScheduler()
queueScheduler.add_job(flushQueue,'interval',seconds=QUEUE_TIME)
queueScheduler.start()