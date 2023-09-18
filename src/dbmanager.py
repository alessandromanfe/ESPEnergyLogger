import sqlite3
from collections import defaultdict
import random
from datetime import datetime
import time

conn = sqlite3.connect("energyLogs.db")

QUEUE_TIME = 60

typeList = ["energy","voltage","current"]
modeList = ["day","hour","all"]
logQueue = []
lastValue = None

def processLogData(args):
    global logQueue
    global lastValue
    
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
    
    lastValue = args
    logQueue.append(args)
    if args["timestamp"] - logQueue[0]["timestamp"] > QUEUE_TIME:
        values = [tuple(d.values()) for d in logQueue]
        print(values)
        conn = sqlite3.connect("energyLogs.db")
        with conn:
            cur = conn.cursor()
            cur.executemany('''INSERT INTO energy_logs (timestamp,energy,voltage,current)
                            VALUES(?, ?, ?, ?);''', values)
        conn.close()
        logQueue.clear()

    for k in args:
        print(k,'\t',args[k])

def getLogData(args):
    global logQueue

    if (args["from_ts"] is None or args["to_ts"] is None 
        or (not args["from_ts"].isdigit()) or (not args["to_ts"].isdigit())
        or int(args["to_ts"])<int(args["from_ts"])):
        return {"status":"error","reason":"invalid parameters"}
    mode = args["mode"]
    if mode is None or mode not in modeList:
        mode = 'day'
    types = args["types"]
    if mode != 'all' and types is None:
        types = "energy" 
    if types is None or types == "all":
        types = ["energy","voltage","current"]
    else:
        types = types.split(',')
        types = [t for t in typeList if t in types]
    types = ["timestamp"] + types
    offset = args["offset"]
    if offset is None or not offset.isdigit():
        offset = '0'
    offset = int(offset)
    from_ts = int(args["from_ts"])
    to_ts = int(args["to_ts"])



    values = []
    if len(logQueue) != 0 and logQueue[0]["timestamp"] <= to_ts:
        values = [tuple([v[t] for t in types]) for v in logQueue 
                  if v["timestamp"]<=to_ts and v["timestamp"]>=from_ts]
    if len(logQueue) != 0 and logQueue[0]["timestamp"] <= from_ts:
        return {"status":"ok", "types":types, "values":values}
    
    
    conn = sqlite3.connect("energyLogs.db")
    with conn:
        cur = conn.cursor()
        divider = None
        if mode == 'day':
            divider = 86400
        if mode == 'hour':
            divider = 3600
        
        if divider is not None:
            dd = defaultdict(lambda : [])
            [dd[(v[0]+offset)//divider].append(v) for v in values]
            values = [min(dd[d], key= lambda x: x[0]) for d in dd]
            cur.execute('''SELECT MIN(timestamp),{}
                        FROM energy_logs
                        WHERE timestamp BETWEEN {} AND {}
                            AND ((timestamp+{})/60)%{} == 0
                        GROUP BY (timestamp+{})/{};'''
                        .format(",".join(types[1:]), from_ts, to_ts, offset, divider//60, offset, divider))  
        
        else:
            cur.execute('''SELECT {} 
                        FROM energy_logs 
                        WHERE timestamp BETWEEN {} AND {}
                        ORDER BY timestamp ASC;'''
                        .format(",".join(types), from_ts, to_ts))
        result = cur.fetchall()
        if divider is not None and len(values)!=0 and result[-1][0]//divider == values[0][0]//divider:
            values = values[1:]
        values = result + values
    conn.close()

    return {"status":"ok", "types":types, "values":values}

def getHourLogData(args):
    '''
    SELECT MIN(timestamp),energy
    FROM energy_logs
    WHERE timestamp BETWEEN 1695599495 AND 1696099495
        AND (timestamp/60)%60 == 0
    --ORDER BY timestamp ASC
    GROUP BY timestamp/3600'''

def getRealTimeData():
    global lastValue
    if lastValue is None or int(datetime.now().timestamp())-lastValue["timestamp"]>15:
        now = int(datetime.now().timestamp())
        return {"status": "ok", "value":{"timestamp":now,"energy":0,"voltage":0,"current":0}, "device_status":"offline"}
    return {"status": "ok", "value":lastValue, "device_status":"online"}

def getInitializeData(args):
    offset = args["offset"]
    if offset is None:
        offset = time.localtime().tm_gmtoff

    fromTS = int(datetime.now().replace(day=1,hour=0,minute=0,second=0,microsecond=0).timestamp())
    toTS = int(datetime.now().timestamp())

    global lastValue
    lv = lastValue
    deviceStatus = "offline"
    if lv is None:
        if len(logQueue) != 0:
            lv = logQueue[-1]["energy"]
        else:
            conn = sqlite3.connect("energyLogs.db")
            with conn:
                cur = conn.cursor()
                cur.execute('''SELECT timestamp,energy
                                FROM energy_logs
                                ORDER BY timestamp DESC LIMIT 1;''')
                lv = cur.fetchall()[0][1]
            conn.close()
        
    else:
        if toTS - lv["timestamp"]<15:
            deviceStatus = "online"
        lv = lv["energy"]

    

    
    conn = sqlite3.connect("energyLogs.db")
    with conn:
        cur = conn.cursor()
        cur.execute('''SELECT timestamp,energy
                        FROM energy_logs
                        WHERE timestamp BETWEEN {} AND {}
                            AND ((timestamp+{})/60)%{} == 0
                        GROUP BY (timestamp+{})/{};'''
                        .format(fromTS, toTS, offset, 86400//60, offset, 86400))
        result = cur.fetchall()
        mv = result[0][1]
        dv = result[-1][1]
    conn.close()
    
    return {"status": "ok", "value": {"day":lv-dv, "month":lv-mv, "tot":lv}, "device_status":deviceStatus}