from dbmanager import DBPATH
import sqlite3

if __name__ == "__main__":
    conn = sqlite3.connect(DBPATH)
    with conn:
        cur = conn.cursor()
        cur.execute('''CREATE TABLE energy_logs (
            "timestamp"	INTEGER,
            "energy"	REAL NOT NULL,
            "voltage"	REAL,
            "current"	REAL,
            PRIMARY KEY("timestamp"))''')
    conn.close()
