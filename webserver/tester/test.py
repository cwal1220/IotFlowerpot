from db_requester import *


db = db_requester('http://52.78.239.38:5000')
# db = db_requester('http://www.kpuchan.com:5000')
print(db.request('/request/record', {'device_id':'ESP_0042D6CD', 'start_time':'2018-09-02 05:00:00', 'end_time':'2018-11-30 02:40:01'}))
# print(db.request('/request/led', {'device_id':'ESP_0042D6CD'}))
