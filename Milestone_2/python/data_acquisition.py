import random

def poll_inverter():
    return {
        "voltage": round(random.uniform(220, 240), 2),
        "current": round(random.uniform(1.0, 5.0), 2)
    }
