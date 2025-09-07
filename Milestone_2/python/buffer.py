#-----Data store functions-----
buffer = []
Buffer_Size = 10

def add_to_buffer(data):
    buffer.append(data)  #add the polled data to store

def get_buffer():
    return buffer.copy()  #copy the data polling during 15 seconds before clear data

def clear_buffer():
    buffer.clear() #clear data

    
def isBufferFull():
    return len(buffer) >= Buffer_Size
