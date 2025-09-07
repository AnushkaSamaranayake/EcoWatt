import time
from data_acquisition import poll_inverter
from buffer import add_to_buffer,get_buffer,clear_buffer,isBufferFull
from upload import upload


poll_interval = 1
upload_interval = 15
last_poll = time.time()
last_upload = time.time()

while True:
    current_time = time.time()
    
    if current_time - last_poll >= poll_interval and not isBufferFull():  #poll after every 5 seconds
        data = poll_inverter() #read the data
        print("Polled:", data)
        add_to_buffer(data) #store to the buffer
        last_poll = current_time
    
    if current_time - last_upload >= upload_interval:
        data_to_upload = get_buffer()
        upload(data_to_upload)
        clear_buffer()
        last_upload = current_time

    
    time.sleep(1)
